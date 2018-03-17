#pragma once
#include "asdbase.h"
#include "lock.h"
#include "timer.h"
#include "semaphore.h"
#include "sysres.h"
#include "util.h"
#include <functional>
#include <thread>
#include <queue>
#include <unordered_set>
#include <unordered_map>


namespace asd
{
	struct ThreadPoolOption
	{
		uint32_t StartThreadCount = Get_HW_Concurrency();

		struct AutoScaling
		{
			bool Use = false;
			double CpuUsageHigh = 0.95; // 95%
			double CpuUsageLow = (1.0 / Get_HW_Concurrency()) * (Get_HW_Concurrency() - 1);
			uint32_t MinThreadCount = 1;
			uint32_t MaxThreadCount = std::numeric_limits<uint32_t>::max();
			Timer::Millisec Cycle = Timer::Millisec(1000);
			uint32_t IncreaseCount = Get_HW_Concurrency();
		} AutoScaling;

		bool CollectStats = false;
		Timer::Millisec CollectStats_Cycle = Timer::Millisec(1000);

		bool UseNotifier = false;
		int SpinWaitCount = 5;

		bool UseEmbeddedTimer = false;


		inline bool UseCenterQueue() const
		{
			return AutoScaling.Use;
		}
	};



	struct ThreadPoolStats
	{
		struct atomic_t : public std::atomic<uint64_t>
		{
			using Base = std::atomic<uint64_t>;
			using Base::Base;
			using Base::operator=;

			atomic_t()
			{
				Base::operator=(0);
			}

			atomic_t(IN const atomic_t& a_copy)
			{
				Base::operator=(a_copy.load());
			}

			atomic_t& operator=(IN const atomic_t& a_copy)
			{
				Base::operator=(a_copy.load());
				return *this;
			}
		};

		uint64_t WaitingCount() const
		{
			return totalPushCount - totalProcCount;
		}

		double RecentWaitingTimeMs() const
		{
			if (recentPushCount == 0)
				return 0;

			double period = (double)recentPeriod.count();
			if (period == 0)
				return std::numeric_limits<double>::max();

			return (double)recentWaitingCount / (recentPushCount / period);
		}

		double TotalConflictRate() const
		{
			if (totalProcCount == 0)
				return 0;
			return totalConflictCount / (double)totalProcCount;
		}

		void Refresh()
		{
			auto now = Timer::Now();
			recentPeriod = Timer::Diff(recentRefreshTime, now);
			recentRefreshTime = now;

			recentPushCount = totalPushCount - recentTotalPushCount;
			recentTotalPushCount = totalPushCount;

			recentWaitingCount = WaitingCount();
		}

		void Push()
		{
			++totalPushCount;
		}

		void Pop()
		{
			++totalProcCount;
		}

		atomic_t totalPushCount;
		atomic_t totalProcCount;
		atomic_t totalConflictCount;

		uint64_t recentPushCount = 0;
		uint64_t recentTotalPushCount = 0;
		uint64_t recentWaitingCount = 0;

		Timer::Millisec recentPeriod = Timer::Millisec(0);
		Timer::TimePoint recentRefreshTime = Timer::Now();

		uint64_t threadCount = 0;
		atomic_t sleepingThreadCount;
	};



	template <typename SEQ_KEY, typename HASH=std::hash<SEQ_KEY>, typename EQ=std::equal_to<SEQ_KEY>>
	class ThreadPool
	{
	public:
		using ThisType = ThreadPool<SEQ_KEY, HASH, EQ>;
		using SeqKey = SEQ_KEY;


		ThreadPool(IN const ThreadPoolOption& a_option)
		{
			Reset(a_option);
		}


		virtual ~ThreadPool()
		{
			asd_BeginDestructor();
			Stop();
			asd_EndDestructor();
		}


		ThreadPoolStats Reset(IN const ThreadPoolOption& a_option)
		{
			auto stats = Stop();
			std::shared_ptr<Data> data(new Data(a_option));
			std::atomic_exchange(&m_data, data);
			return stats;
		}


		ThreadPoolStats Stop()
		{
			auto data = std::atomic_exchange(&m_data, std::shared_ptr<Data>());
			if (data == nullptr)
				return ThreadPoolStats();

			auto lock = GetLock(data->lock);
			if (data->run == false)
				return data->stats;

			if (data->workers.find(GetCurrentThreadID()) != data->workers.end())
				asd_RaiseException("self-deadlock");

			data->run = false;

			if (data->timer != nullptr)
				data->timer.reset();

			if (data->option.UseCenterQueue()) {
				lock.unlock();
				for (auto centerQueueLock=GetLock(data->centerQueueLock); data->centerQueue.size()>0; centerQueueLock.lock()) {
					while (data->standby.size() > 0)
						NeedNotify(data.get(), nullptr).Notify();
					centerQueueLock.unlock();
					std::this_thread::sleep_for(Timer::Millisec(1));
				}
				lock.lock();
			}

			while (data->workers.size() > 0) {
				for (Worker* worker : data->workerList) {
					auto workerLock = GetLock(worker->lock);
					worker->run = false;
					NeedNotify(data.get(), worker).Notify();
				}
				lock.unlock();
				std::this_thread::sleep_for(Timer::Millisec(1));
				lock.lock();
			}

			return data->stats;
		}


		void Start()
		{
			auto data = std::atomic_load(&m_data);
			if (data == nullptr)
				return;

			auto lock = GetLock(data->lock);
			asd_ChkErrAndRet(data->run, "already started");
			data->run = true;
			lock.unlock();

			if (data->option.AutoScaling.Use)
				CpuUsage(); // 인스턴스 초기화를 위해 호출

			uint32_t startThreadCount = max(data->option.StartThreadCount, 1U);
			if (data->option.AutoScaling.Use) {
				auto MinCnt = data->option.AutoScaling.MinThreadCount;
				auto MaxCnt = data->option.AutoScaling.MaxThreadCount;
				if (MinCnt > MaxCnt || MaxCnt == 0)
					asd_RaiseException("invalid AutoScaling option");
				startThreadCount = max(startThreadCount, MinCnt);
				startThreadCount = min(startThreadCount, MaxCnt);
			}
			AddWorker(data, startThreadCount);

			if (data->option.UseEmbeddedTimer)
				data->timer.reset(new Timer);

			if (data->option.CollectStats)
				PushStatTask(data);

			if (data->option.AutoScaling.Use)
				PushAutoScalingTask(data);
		}


		ThreadPoolStats GetStats() const
		{
			auto data = std::atomic_load(&m_data);
			if (data == nullptr)
				return ThreadPoolStats();

			auto lock = GetSharedLock(data->lock);
			auto stats = data->stats;
			lock.unlock_shared();
			return stats;
		}
		

		template <typename FUNC, typename... PARAMS>
		inline Task_ptr Push(FUNC&& a_func,
							 PARAMS&&... a_params)
		{
			TaskObj taskObj;
			taskObj.seq = false;
			taskObj.task = CreateTask(std::forward<FUNC>(a_func),
									  std::forward<PARAMS>(a_params)...);
			auto data = std::atomic_load(&m_data);
			return PushTask(data, taskObj);
		}

		template <typename FUNC, typename... PARAMS>
		inline Task_ptr PushAfter(IN Timer::Millisec a_afterMs,
								  FUNC&& a_func,
								  PARAMS&&... a_params)
		{
			return PushAt(Timer::Now() + a_afterMs,
						  std::forward<FUNC>(a_func),
						  std::forward<PARAMS>(a_params)...);
		}


		template <typename FUNC, typename... PARAMS>
		inline Task_ptr PushAt(IN Timer::TimePoint a_timepoint,
							   FUNC&& a_func,
							   PARAMS&&... a_params)
		{
			TaskObj taskObj;
			taskObj.seq = false;
			taskObj.task = CreateTask(std::forward<FUNC>(a_func),
									  std::forward<PARAMS>(a_params)...);
			auto data = std::atomic_load(&m_data);
			return PushTimerTask(std::move(data),
								 a_timepoint,
								 std::move(taskObj));
		}


		template <typename KEY, typename FUNC, typename... PARAMS>
		inline Task_ptr PushSeq(KEY&& a_key,
								FUNC&& a_func,
								PARAMS&&... a_params)
		{
			TaskObj taskObj;
			taskObj.seq = true;
			taskObj.keyInfo.hash = HASH()(a_key);
			taskObj.keyInfo.key = std::forward<KEY>(a_key);
			taskObj.task = CreateTask(std::forward<FUNC>(a_func),
									  std::forward<PARAMS>(a_params)...);
			auto data = std::atomic_load(&m_data);
			return PushTask(data, taskObj);
		}


		template <typename KEY, typename FUNC, typename... PARAMS>
		inline Task_ptr PushSeqAfter(IN Timer::Millisec a_afterMs,
									 KEY&& a_key,
									 FUNC&& a_func,
									 PARAMS&&... a_params)
		{
			return PushSeqAt(Timer::Now() + a_afterMs,
							 std::forward<KEY>(a_key),
							 std::forward<FUNC>(a_func),
							 std::forward<PARAMS>(a_params)...);
		}


		template <typename KEY, typename FUNC, typename... PARAMS>
		inline Task_ptr PushSeqAt(IN Timer::TimePoint a_timepoint,
								  KEY&& a_key,
								  FUNC&& a_func,
								  PARAMS&&... a_params)
		{
			TaskObj taskObj;
			taskObj.seq = true;
			taskObj.keyInfo.hash = HASH()(a_key);
			taskObj.keyInfo.key = std::forward<KEY>(a_key);
			taskObj.task = CreateTask(std::forward<FUNC>(a_func),
									  std::forward<PARAMS>(a_params)...);
			auto data = std::atomic_load(&m_data);
			return PushTimerTask(std::move(data),
								 a_timepoint,
								 std::move(taskObj));
		}


	private:
		struct Data;


		struct SeqKeyInfo
		{
			SeqKey key;
			size_t hash;
		};

		struct TaskObj
		{
			Task_ptr task;
			bool seq;
			SeqKeyInfo keyInfo;
		};

		class TaskQueue
		{
		public:
			size_t size() const
			{
				return m_size;
			}

			bool empty() const
			{
				return m_size == 0;
			}

			void emplace_back(MOVE TaskObj&& a_data)
			{
				Node* newNode = m_nodePool.Alloc(std::move(a_data));
				push(newNode, newNode, 1);
			}

			TaskObj& front()
			{
				return m_head->data;
			}

			void pop_front()
			{
				if (m_head == nullptr)
					return;
				Node* head = m_head;
				m_head = head->next;
				if (m_tail == head)
					m_tail = nullptr;
				m_nodePool.Free(head);
				--m_size;
			}

			void clear()
			{
				while (size() > 0)
					pop_front();
			}

			~TaskQueue()
			{
				clear();
			}

		private:
			struct Node
			{
				TaskObj data;
				Node* next = nullptr;
				Node(MOVE TaskObj&& a_mv) : data(std::move(a_mv)) {}
			};

			inline void push(IN Node* a_head,
							 IN Node* a_tail,
							 IN size_t a_size)
			{
				if (m_tail != nullptr)
					m_tail->next = a_head;
				else
					m_head = a_head;
				m_tail = a_tail;
				m_size += a_size;
			}

			ObjectPool<Node> m_nodePool;

			size_t m_size = 0;
			Node* m_head = nullptr;
			Node* m_tail = nullptr;
		};//TaskQueue


		struct Notifier
		{
			Semaphore* notify = nullptr;
			void Notify()
			{
				if (notify)
					notify->Post();
			}
		};

		struct Worker
		{
			mutable Mutex lock;

			uint32_t tid = 0;
			size_t index = std::numeric_limits<size_t>::max();

			bool run = true;
			TaskQueue queue[2];
			TaskQueue* publicQueue = &queue[0];
			TaskQueue* privateQueue = &queue[1];

			bool sleep = false; // UseCenterQueue() == false 경우에만 사용
			Semaphore notify;
		};

		struct Work
		{
			Worker* worker = nullptr;
			int reserveCount = 0;
			int procCount = 0;
		};


		class WorkingMap
		{
		public:
			WorkingMap()
				: ShardCount(FindNearPrimeNumber(min(std::numeric_limits<uint16_t>::max(), 2*Get_HW_Concurrency())))
			{
				m_shards.resize(ShardCount);
				m_locks.resize(ShardCount);
			}


#define asd_ThreadPool_WorkingMap_FindShard(a_keyInfo, shard)		\
			const size_t idx = a_keyInfo.hash % ShardCount;			\
			auto lock = GetLock(m_locks[idx]);						\
			auto& shard = m_shards[idx];							\

#define asd_ThreadPool_WorkingMap_FindWork(a_keyInfo, work)			\
			asd_ThreadPool_WorkingMap_FindShard(a_keyInfo, shard)	\
			auto& work = shard[a_keyInfo];							\


			Worker* Reserve(IN const SeqKeyInfo& a_keyInfo,
							REF Data* a_data)
			{
				asd_ThreadPool_WorkingMap_FindWork(a_keyInfo, work);

				if (a_data->option.UseCenterQueue()) {
					++work.reserveCount;
				}
				else {
					++work.procCount;
					if (work.worker == nullptr)
						work.worker = a_data->PickWorker();
					else
						++a_data->stats.totalConflictCount;
				}
				return work.worker;
			}

			Worker* SetWorker(IN const SeqKeyInfo& a_keyInfo,
							  REF Worker* a_worker)
			{
				asd_ThreadPool_WorkingMap_FindWork(a_keyInfo, work);

				--work.reserveCount;
				++work.procCount;

				if (work.worker == nullptr)
					work.worker = a_worker;

				return work.worker;
			}

			void Finish(IN const SeqKeyInfo& a_keyInfo)
			{
				asd_ThreadPool_WorkingMap_FindShard(a_keyInfo, shard);

				auto it = shard.find(a_keyInfo);
				asd_ChkErrAndRet(it == shard.end(), "unknown error");
				auto& work = it->second;

				if (--work.procCount == 0) {
					if (work.reserveCount == 0)
						shard.erase(it);
					else
						work.worker = nullptr;
				}
			}

		private:
			const size_t ShardCount;

			struct SeqKeyInfoHash
			{
				inline size_t operator()(IN const SeqKeyInfo& a_keyInfo) const
				{
					return a_keyInfo.hash;
				}
			};
			struct SeqKeyInfoEqual
			{
				inline bool operator()(IN const SeqKeyInfo& a_keyInfo1,
									   IN const SeqKeyInfo& a_keyInfo2) const
				{
					return EQ()(a_keyInfo1.key, a_keyInfo2.key);
				}
			};
			using Map = std::unordered_map<SeqKeyInfo, Work, SeqKeyInfoHash, SeqKeyInfoEqual>;

			std::vector<Map> m_shards;
			std::vector<Mutex> m_locks;
		}; //WorkingMap


		struct Data
		{
			const ThreadPoolOption option;

			// 작업쓰레드 목록 관련
			mutable SharedMutex lock;
			std::unordered_map<uint32_t, Worker*> workers;
			std::vector<Worker*> workerList;
			std::atomic<size_t> RRSeq;
			bool run = false; // 종료 중 Push를 막기 위한 플래그

			// SeqKey 별 담당 현황
			WorkingMap workingMap;

			// 통계
			ThreadPoolStats stats;

			// 내장 타이머 (UseEmbeddedTimer == true 경우에만 사용)
			std::unique_ptr<Timer> timer;

			// 중앙큐 관련 (AutoScaling.Use == true 경우에만 사용)
			mutable Mutex centerQueueLock;
			TaskQueue centerQueue;
			std::unordered_set<Worker*> standby;


			Data(IN const ThreadPoolOption& a_option)
				: option(a_option)
			{
				RRSeq = 0;
			}

			Worker* PickWorker()
			{
				return workerList[++RRSeq % workerList.size()];
			}
		};


		// 작업쓰레드 추가
		static void AddWorker(IN std::shared_ptr<Data> a_data,
							  IN uint32_t a_count)
		{
			for (uint32_t i=0; i<a_count; ++i) {
				asd_BeginTry();
				std::thread(&ThisType::Working, a_data).detach();
				asd_EndTryUnknown_Default();
			}
		}


		// 타이머 작업 예약
		static Task_ptr PushTimerTask(MOVE std::shared_ptr<Data>&& a_data,
									  IN Timer::TimePoint a_timePoint,
									  MOVE TaskObj&& a_task)
		{
			if (a_data == nullptr)
				return nullptr;

			auto lock = GetSharedLock(a_data->lock);
			if (a_data->run == false)
				return nullptr;

			auto timer = a_data->timer ? a_data->timer.get() : &Timer::GlobalInstance();
			return timer->PushAt(a_timePoint,
								 &ThisType::PushTask,
								 std::move(a_data),
								 std::move(a_task));
		}


		// 작업 등록
		static Task_ptr PushTask(REF std::shared_ptr<Data>& a_data,
								 REF TaskObj& a_task)
		{
			auto task = a_task.task;
			if (a_data == nullptr)
				return nullptr;

			auto lock = GetSharedLock(a_data->lock);

			if (!a_data->run || a_data->workerList.empty())
				return nullptr;

			Notifier notifier;
			if (a_data->option.UseCenterQueue()) {
				if (a_task.seq)
					a_data->workingMap.Reserve(a_task.keyInfo, a_data.get());

				auto centerQueueLock = GetLock(a_data->centerQueueLock);
				a_data->centerQueue.emplace_back(std::move(a_task));

				notifier = NeedNotify(a_data.get(), nullptr);
				lock.unlock_shared();
			}
			else {
				Worker* worker;
				if (a_task.seq)
					worker = a_data->workingMap.Reserve(a_task.keyInfo, a_data.get());
				else
					worker = a_data->PickWorker();

				auto workerLock = GetLock(worker->lock);
				lock.unlock_shared();

				worker->publicQueue->emplace_back(std::move(a_task));
				notifier = NeedNotify(a_data.get(), worker);
			}

			notifier.Notify();
			a_data->stats.Push();
			return task;
		}


		// 작업 알림
		static Notifier NeedNotify(REF Data* a_data,
								   REF Worker* a_worker)
		{
			Notifier ret;
			if (!a_data->option.UseNotifier)
				return ret;

			if (a_data->option.UseCenterQueue()) {
				// acquired a_data->centerQueueLock
				Worker* worker = a_worker;
				if (worker == nullptr) {
					if (a_data->standby.empty())
						return ret;
					auto it = a_data->standby.begin();
					worker = *it;
					a_data->standby.erase(it);
					ret.notify = &worker->notify;
					return ret;
				}
				else {
					if (1 == a_data->standby.erase(worker))
						ret.notify = &worker->notify;
					return ret;
				}
			}
			else {
				// acquired a_worker->lock
				asd_DAssert(a_worker != nullptr);
				if (a_worker->sleep) {
					a_worker->sleep = false;
					ret.notify = &a_worker->notify;
				}
				return ret;
			}
			return ret;
		}


		// 작업 대기
		static bool Ready(REF Data* a_data,
						  REF Worker* a_worker)
		{
			auto workerLock = GetLock(a_worker->lock);

			asd_RAssert(a_worker->privateQueue->empty(), "unknown error");

			int spinCount = a_data->option.SpinWaitCount;
			for (; a_worker->publicQueue->empty(); workerLock.lock()) {
				if (!a_worker->run)
					return false;

				bool spin = false;
				if (spinCount > 0) {
					spin = true;
					--spinCount;
				}
				a_worker->sleep = !spin && a_data->option.UseNotifier;

				workerLock.unlock();

				Notifier notifier;
				if (a_data->option.UseCenterQueue()) {
					Worker* responsibleWorker = nullptr;
					auto centerQueueLock = GetLock(a_data->centerQueueLock);
					if (a_data->centerQueue.size() > 0) {
						TaskObj& taskObj = a_data->centerQueue.front();
						if (taskObj.seq)
							responsibleWorker = a_data->workingMap.SetWorker(taskObj.keyInfo, a_worker);
						else
							responsibleWorker = a_worker;

						auto responsibleWorkerLock = GetLock(responsibleWorker->lock);
						responsibleWorker->publicQueue->emplace_back(std::move(taskObj));
						a_data->centerQueue.pop_front();
						responsibleWorkerLock.unlock();

						if (responsibleWorker == a_worker)
							continue;

						++a_data->stats.totalConflictCount;
						notifier = NeedNotify(a_data, responsibleWorker);
					}

					if (a_data->centerQueue.size() > 0)
						spin = true;
					else if (!spin && a_data->option.UseNotifier)
						a_data->standby.emplace(a_worker);
				}
				notifier.Notify();

				if (spin) {
					std::this_thread::yield();
				}
				else {
					++a_data->stats.sleepingThreadCount;
					if (a_data->option.UseNotifier)
						a_worker->notify.Wait();
					else
						std::this_thread::sleep_for(Timer::Millisec(1));
					--a_data->stats.sleepingThreadCount;
				}
			}

			a_worker->sleep = false;
			std::swap(a_worker->publicQueue, a_worker->privateQueue);
			return true;
		}


		// 작업쓰레드 메인루프
		static void Working(REF std::shared_ptr<Data> a_data)
		{
			asd_BeginTry();
			if (a_data == nullptr)
				return;

			Worker curWorker;
			{
				auto lock = GetLock(a_data->lock);
				if (a_data->workers.size() >= a_data->option.AutoScaling.MaxThreadCount)
					return;

				curWorker.tid = GetCurrentThreadID();

				asd_ChkErrAndRet(!a_data->workers.emplace(curWorker.tid, &curWorker).second, "already registered thread");
				a_data->workerList.emplace_back(&curWorker);
				asd_RAssert(a_data->workers.size() == a_data->workerList.size(), "unknown error");
				a_data->stats.threadCount = a_data->workers.size();

				curWorker.index = a_data->workerList.size() - 1;
			}

			while (Ready(a_data.get(), &curWorker)) {
				while (curWorker.privateQueue->size() > 0) {
					asd_BeginTry();

					TaskObj& taskObj = curWorker.privateQueue->front();

					taskObj.task->Execute();
					taskObj.task.reset();

					if (taskObj.seq)
						a_data->workingMap.Finish(taskObj.keyInfo);

					a_data->stats.Pop();
					curWorker.privateQueue->pop_front();

					asd_EndTryUnknown_Default();
				}
			}

			DeleteWorker(a_data.get(), &curWorker);

			auto workerLock = GetLock(curWorker.lock);
			asd_RAssert(curWorker.publicQueue->empty(), "exist remain task");
			asd_RAssert(curWorker.privateQueue->empty(), "exist remain task");
			asd_EndTryUnknown_Default();
		}


		// 작업쓰레드 제거
		static void DeleteWorker(REF Data* a_data,
								 REF Worker* a_worker)
		{
			auto lock = GetLock(a_data->lock);

			const bool AutoScalingProc = a_worker == nullptr;
			const size_t last = a_data->workerList.size() - 1;

			if (AutoScalingProc) {
				if (a_data->workerList.size() <= max(a_data->option.AutoScaling.MinThreadCount, 1U))
					return;
				a_worker = a_data->workerList[last];
			}

			auto it = a_data->workers.find(a_worker->tid);
			if (it == a_data->workers.end())
				return;

			asd_ChkErrAndRet(a_worker != it->second, "unknown error");

			Notifier notifier;
			{
				auto centerQueueLock = GetLock(a_data->centerQueueLock);
				auto workerLock = GetLock(a_worker->lock);
				a_worker->run = false;
				notifier = NeedNotify(a_data, a_worker);
			}
			notifier.Notify();

			std::swap(a_data->workerList[a_worker->index], a_data->workerList[last]);

			a_data->workerList[a_worker->index]->index = a_worker->index;
			a_data->workerList.resize(last);

			a_data->workers.erase(it);
			a_data->stats.threadCount = a_data->workers.size();
			asd_RAssert(a_data->workers.size() == a_data->workerList.size(), "unknown error");
		}


		// AutoScaling 기능
		static void PushAutoScalingTask(IN std::shared_ptr<Data> a_data)
		{
			if (a_data == nullptr)
				return;

			auto task = CreateTask([a_data]() mutable
			{
				auto lock = GetSharedLock(a_data->lock);

				if (!a_data->run || a_data->workers.empty())
					return;

				auto proc([&]() -> std::function<void()>
				{
					static const auto s_none = [](){};
					double cpu = CpuUsage();
					if (cpu < a_data->option.AutoScaling.CpuUsageLow) {
						uint64_t sleepingThreadCount = a_data->stats.sleepingThreadCount;
						if (sleepingThreadCount > max(0.1*a_data->workers.size(), 1))
							return [&](){ DeleteWorker(a_data.get(), nullptr); };
						if (sleepingThreadCount > 0)
							return s_none;
						return [&](){ AddWorker(a_data, a_data->option.AutoScaling.IncreaseCount); };
					}
					else if (cpu > a_data->option.AutoScaling.CpuUsageHigh) {
						if (a_data->workers.size() <= 1)
							return s_none;
						return [&](){ DeleteWorker(a_data.get(), nullptr); };
					}
					return s_none;
				}());
				lock.unlock_shared();
				proc();
				PushAutoScalingTask(a_data);
			});

			auto at = Timer::Now() + max(GetCpuUsageCheckCycle(), a_data->option.AutoScaling.Cycle);
			if (a_data->timer != nullptr) {
				a_data->timer->PushTask(at, task);
			}
			else {
				TaskObj taskObj;
				taskObj.seq = true;
				taskObj.keyInfo.key = SeqKey();
				taskObj.keyInfo.hash = HASH()(taskObj.keyInfo.key);
				taskObj.task = std::move(task);
				PushTimerTask(std::move(a_data), at, std::move(taskObj));
			}
		}


		// 통계 수집
		static void PushStatTask(IN std::shared_ptr<Data> a_data)
		{
			if (a_data == nullptr)
				return;

			auto task = CreateTask([a_data]() mutable
			{
				auto lock = GetLock(a_data->lock);

				if (!a_data->run)
					return;

				a_data->stats.Refresh();

				lock.unlock();
				PushStatTask(a_data);
			});

			auto at = Timer::Now() + a_data->option.CollectStats_Cycle;
			if (a_data->timer != nullptr) {
				a_data->timer->PushTask(at, task);
			}
			else {
				TaskObj taskObj;
				taskObj.seq = false;
				taskObj.task = std::move(task);
				PushTimerTask(std::move(a_data), at, std::move(taskObj));
			}
		}

		std::shared_ptr<Data> m_data;
	};
}


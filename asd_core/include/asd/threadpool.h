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
	template <typename ELEM>
	class SimpleQueue
	{
	public:
		SimpleQueue()
			: m_nodePool(10000)
		{
		}

		size_t size() const
		{
			return m_size;
		}

		bool empty() const
		{
			return m_size == 0;
		}

		void emplace_back(MOVE ELEM&& a_data)
		{
			Node* newNode = m_nodePool.Alloc(std::move(a_data));
			push(newNode, newNode, 1);
		}

		ELEM& front()
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
			m_nodePool.Clear();
		}

		~SimpleQueue()
		{
			clear();
		}

	private:
		struct Node
		{
			ELEM data;
			Node* next = nullptr;
			Node(MOVE ELEM&& a_mv) : data(std::move(a_mv)) {}
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
	};



	struct ThreadPoolOption
	{
		uint32_t ThreadCount = Get_HW_Concurrency();

		bool CollectStats = false;
		Timer::Millisec CollectStats_Interval = Timer::Millisec(1000);

		bool UseNotifier = true;
		int SpinWaitCount = 5;

		bool UseEmbeddedTimer = false;

		enum struct Pick {
			ShortestQueue,
			RoundRobin,
		};
		Pick PickAlgorithm = Pick::ShortestQueue;
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
			if (totalPushCount == 0)
				return 0;
			return totalConflictCount / (double)totalPushCount;
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

		uint64_t Push()
		{
			return ++totalPushCount;
		}

		uint64_t Pop()
		{
			return ++totalProcCount;
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

			for (; data->workers.size() > 0; lock.lock()) {
				for (uint32_t i=0; i<data->workerCount; ++i) {
					Worker& worker = data->workerList[i];
					auto workerLock = GetLock(worker.lock);
					worker.run = false;
					NeedNotify(data.get(), &worker).Notify();
				}
				lock.unlock();
				std::this_thread::sleep_for(Timer::Millisec(1));
			}

			return data->stats;
		}


		void Start()
		{
			auto data = std::atomic_load(&m_data);
			if (data == nullptr) {
				asd_OnErr("is stopped. plz call Reset()");
				return;
			}

			auto lock = GetLock(data->lock);
			if (data->run) {
				asd_OnErr("already started");
				return;
			}
			data->run = true;

			data->workerCount = max(data->option.ThreadCount, 1U);
			if (data->workerList)
				asd_RaiseException("unknown error");

			data->workerList = new Worker[data->workerCount];

			for (uint32_t i=0; i<data->workerCount; ++i) {
				data->workerList[i].index = i;
				std::thread(&ThisType::Working, data, i).detach();
			}
			lock.unlock();

			if (data->option.UseEmbeddedTimer)
				data->timer.reset(new Timer);

			if (data->option.CollectStats)
				PushStatTask(data);
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


		Task_ptr Push(MOVE Task_ptr&& a_task)
		{
			TaskObj taskObj;
			taskObj.seq = false;
			taskObj.task = std::move(a_task);
			auto data = std::atomic_load(&m_data);
			return PushTask(data, taskObj);
		}


		inline Task_ptr Push(IN const Task_ptr& a_task)
		{
			return Push(Task_ptr(a_task));
		}


		template <typename FUNC, typename... PARAMS>
		inline Task_ptr Push(FUNC&& a_func,
							 PARAMS&&... a_params)
		{
			return Push(CreateTask(std::forward<FUNC>(a_func),
								   std::forward<PARAMS>(a_params)...));
		}


		Task_ptr PushAt(IN Timer::TimePoint a_timepoint,
						MOVE Task_ptr&& a_task)
		{
			TaskObj taskObj;
			taskObj.seq = false;
			taskObj.task = std::move(a_task);
			auto data = std::atomic_load(&m_data);
			return PushTimerTask(std::move(data),
								 a_timepoint,
								 std::move(taskObj));
		}


		inline Task_ptr PushAt(IN Timer::TimePoint a_timepoint,
							   IN const Task_ptr& a_task)
		{
			return PushAt(a_timepoint, Task_ptr(a_task));
		}


		template <typename FUNC, typename... PARAMS>
		inline Task_ptr PushAt(IN Timer::TimePoint a_timepoint,
							   FUNC&& a_func,
							   PARAMS&&... a_params)
		{
			return PushAt(a_timepoint,
						  CreateTask(std::forward<FUNC>(a_func),
									 std::forward<PARAMS>(a_params)...));
		}


		template <typename DURATION>
		inline Task_ptr PushAfter(IN DURATION a_after,
								  MOVE Task_ptr&& a_task)
		{
			return PushAt(Timer::Now() + a_after,
						  std::move(a_task));
		}


		template <typename DURATION>
		inline Task_ptr PushAfter(IN DURATION a_after,
								  IN const Task_ptr& a_task)
		{
			return PushAt(Timer::Now() + a_after,
						  a_task);
		}


		template <typename DURATION, typename FUNC, typename... PARAMS>
		inline Task_ptr PushAfter(IN DURATION a_after,
								  FUNC&& a_func,
								  PARAMS&&... a_params)
		{
			return PushAfter(a_after,
							 CreateTask(std::forward<FUNC>(a_func),
										std::forward<PARAMS>(a_params)...));
		}


		template <typename KEY>
		Task_ptr PushSeq(KEY&& a_key,
						 MOVE Task_ptr&& a_task)
		{
			TaskObj taskObj;
			taskObj.seq = true;
			taskObj.keyInfo.hash = HASH()(a_key);
			taskObj.keyInfo.key = std::forward<KEY>(a_key);
			taskObj.task = std::move(a_task);
			auto data = std::atomic_load(&m_data);
			return PushTask(data, taskObj);
		}


		template <typename KEY>
		inline Task_ptr PushSeq(KEY&& a_key,
								IN const Task_ptr& a_task)
		{
			return PushSeq(std::forward<KEY>(a_key),
						   Task_ptr(a_task));
		}


		template <typename KEY, typename FUNC, typename... PARAMS>
		inline Task_ptr PushSeq(KEY&& a_key,
								FUNC&& a_func,
								PARAMS&&... a_params)
		{
			return PushSeq(std::forward<KEY>(a_key),
						   CreateTask(std::forward<FUNC>(a_func),
									  std::forward<PARAMS>(a_params)...));
		}


		template <typename KEY>
		Task_ptr PushSeqAt(IN Timer::TimePoint a_timepoint,
						   KEY&& a_key,
						   MOVE Task_ptr&& a_task)
		{
			TaskObj taskObj;
			taskObj.seq = true;
			taskObj.keyInfo.hash = HASH()(a_key);
			taskObj.keyInfo.key = std::forward<KEY>(a_key);
			taskObj.task = std::move(a_task);
			auto data = std::atomic_load(&m_data);
			return PushTimerTask(std::move(data),
								 a_timepoint,
								 std::move(taskObj));
		}


		template <typename KEY>
		inline Task_ptr PushSeqAt(IN Timer::TimePoint a_timepoint,
								  KEY&& a_key,
								  const Task_ptr& a_task)
		{
			return PushSeqAt(a_timepoint,
							 std::forward<KEY>(a_key),
							 Task_ptr(a_task));
		}


		template <typename KEY, typename FUNC, typename... PARAMS>
		inline Task_ptr PushSeqAt(IN Timer::TimePoint a_timepoint,
								  KEY&& a_key,
								  FUNC&& a_func,
								  PARAMS&&... a_params)
		{
			return PushSeqAt(a_timepoint,
							 std::forward<KEY>(a_key),
							 CreateTask(std::forward<FUNC>(a_func),
										std::forward<PARAMS>(a_params)...));
		}


		template <typename DURATION, typename KEY>
		inline Task_ptr PushSeqAfter(IN DURATION a_after,
									 KEY&& a_key,
									 MOVE Task_ptr&& a_task)
		{
			return PushSeqAt(Timer::Now() + a_after,
							 std::forward<KEY>(a_key),
							 std::move(a_task));
		}


		template <typename DURATION, typename KEY>
		inline Task_ptr PushSeqAfter(IN DURATION a_after,
									 KEY&& a_key,
									 IN const Task_ptr& a_task)
		{
			return PushSeqAt(Timer::Now() + a_after,
							 std::forward<KEY>(a_key),
							 a_task);
		}


		template <typename DURATION, typename KEY, typename FUNC, typename... PARAMS>
		inline Task_ptr PushSeqAfter(IN DURATION a_after,
									 KEY&& a_key,
									 FUNC&& a_func,
									 PARAMS&&... a_params)
		{
			return PushSeqAfter(a_after,
								std::forward<KEY>(a_key),
								CreateTask(std::forward<FUNC>(a_func),
										   std::forward<PARAMS>(a_params)...));
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
			SimpleQueue<TaskObj> queue[2];
			SimpleQueue<TaskObj>* publicQueue = &queue[0];
			SimpleQueue<TaskObj>* privateQueue = &queue[1];

			bool waitNotify = false;
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

				++work.procCount;
				if (work.worker == nullptr)
					work.worker = a_data->PickWorker();
				else
					++a_data->stats.totalConflictCount;
				return work.worker;
			}

			void Finish(IN const SeqKeyInfo& a_keyInfo)
			{
				asd_ThreadPool_WorkingMap_FindShard(a_keyInfo, shard);

				auto it = shard.find(a_keyInfo);
				if (it == shard.end()) {
					asd_OnErr("unknown error");
					return;
				}
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
			Worker* workerList = nullptr;
			uint32_t workerCount = 0;
			std::atomic<size_t> RRSeq;
			bool run = false; // 종료 중 Push를 막기 위한 플래그

			// SeqKey 별 담당 현황
			WorkingMap workingMap;

			// 통계
			ThreadPoolStats stats;

			// 내장 타이머 (UseEmbeddedTimer == true 경우에만 사용)
			std::unique_ptr<Timer> timer;


			Data(IN const ThreadPoolOption& a_option)
				: option(a_option)
			{
				RRSeq = 0;
			}

			~Data()
			{
				if (workerList)
					delete[] workerList;
			}

			Worker* PickWorker()
			{
				switch (option.PickAlgorithm) {
					case ThreadPoolOption::Pick::RoundRobin:
						return PickWorkerT<ThreadPoolOption::Pick::RoundRobin>();

					case ThreadPoolOption::Pick::ShortestQueue:
					default:
						return PickWorkerT<ThreadPoolOption::Pick::ShortestQueue>();
				}
			}

			template <ThreadPoolOption::Pick>
			Worker* PickWorkerT();

			template<>
			Worker* PickWorkerT<ThreadPoolOption::Pick::RoundRobin>()
			{
				const size_t beginIdx = ++RRSeq % workerCount;
				for (size_t i=0; i<workerCount; ++i) {
					Worker* worker = &workerList[(beginIdx+i) % workerCount];
					if (worker->run)
						return worker;
				}
				return nullptr;
			}

			template<>
			Worker* PickWorkerT<ThreadPoolOption::Pick::ShortestQueue>()
			{
				Worker* workerA = &workerList[0];
				for (size_t i=1; i<workerCount; ++i) {
					Worker* workerB = &workerList[i];
					if (workerB->run && workerA->publicQueue->size() > workerB->publicQueue->size())
						workerA = workerB;
				}
				return workerA;
			}
		};


		// 타이머 작업 예약
		static Task_ptr PushTimerTask(MOVE std::shared_ptr<Data>&& a_data,
									  IN Timer::TimePoint a_timePoint,
									  MOVE TaskObj&& a_task)
		{
			if (a_data == nullptr)
				return nullptr;

			auto lock = GetSharedLock(a_data->lock);

			if (!a_data->run) {
				asd_OnErr("thread-pool was stopped");
				return nullptr;
			}

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

			if (!a_data->run) {
				asd_OnErr("thread-pool was stopped");
				return nullptr;
			}

			Worker* worker;
			if (a_task.seq)
				worker = a_data->workingMap.Reserve(a_task.keyInfo, a_data.get());
			else
				worker = a_data->PickWorker();

			if (worker == nullptr) {
				asd_OnErr("empty thread");
				return nullptr;
			}

			auto workerLock = GetLock(worker->lock);
			lock.unlock_shared();

			if (!worker->run) {
				asd_OnErr("thread-pool was stopped");
				return nullptr;
			}

			worker->publicQueue->emplace_back(std::move(a_task));
			Notifier notifier = NeedNotify(a_data.get(), worker);

			workerLock.unlock();

			notifier.Notify();
			a_data->stats.Push();
			return task;
		}


		// 작업 알림
		static Notifier NeedNotify(REF Data* a_data,
								   REF Worker* a_worker)
		{
			// already acquired a_worker->lock

			Notifier ret;
			if (!a_data->option.UseNotifier)
				return ret;

			if (a_worker == nullptr) {
				asd_OnErr("unknown error");
			}
			else if (a_worker->waitNotify) {
				a_worker->waitNotify = false;
				ret.notify = &a_worker->notify;
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
				a_worker->waitNotify = !spin && a_data->option.UseNotifier;

				workerLock.unlock();

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

			a_worker->waitNotify = false;
			std::swap(a_worker->publicQueue, a_worker->privateQueue);
			return true;
		}


		// 작업쓰레드 메인루프
		static void Working(REF std::shared_ptr<Data> a_data,
							IN uint32_t a_workerIdx)
		{
			asd_BeginTry();
			if (a_data == nullptr)
				return;

			Worker& curWorker = [&]() -> Worker& {
				auto lock = GetLock(a_data->lock);

				Worker& curWorker = a_data->workerList[a_workerIdx];
				curWorker.tid = GetCurrentThreadID();

				if (!a_data->workers.emplace(curWorker.tid, &curWorker).second) {
					asd_OnErr("already registered thread");
					return curWorker;
				}
				a_data->stats.threadCount = a_data->workers.size();
				return curWorker;
			}();

			while (Ready(a_data.get(), &curWorker)) {
				while (curWorker.privateQueue->size() > 0) {
					TaskObj& taskObj = curWorker.privateQueue->front();

					asd_BeginTry();
					taskObj.task->Execute();
					taskObj.task.reset();
					asd_EndTryUnknown_Default();

					if (taskObj.seq)
						a_data->workingMap.Finish(taskObj.keyInfo);

					curWorker.privateQueue->pop_front();
					a_data->stats.Pop();
				}
			}

			DeleteWorker(a_data, &curWorker);

			auto workerLock = GetLock(curWorker.lock);
			asd_RAssert(curWorker.publicQueue->empty(), "exist remain task");
			asd_RAssert(curWorker.privateQueue->empty(), "exist remain task");
			asd_EndTryUnknown_Default();
		}


		// 작업쓰레드 제거
		static void DeleteWorker(REF std::shared_ptr<Data> a_data,
								 REF Worker* a_worker)
		{
			auto lock = GetLock(a_data->lock);

			auto it = a_data->workers.find(a_worker->tid);
			if (it == a_data->workers.end())
				return;

			if (a_worker != it->second) {
				asd_OnErr("unknown error");
				return;
			}

			Notifier notifier;
			{
				auto workerLock = GetLock(a_worker->lock);
				a_worker->run = false;
				notifier = NeedNotify(a_data.get(), a_worker);
			}
			notifier.Notify();

			a_data->workers.erase(it);
			a_data->stats.threadCount = a_data->workers.size();
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

			auto at = Timer::Now() + a_data->option.CollectStats_Interval;
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



	class ScalableThreadPool
	{
	public:
		struct Option
		{
			uint32_t MinWorkerCount = 1;
			uint32_t MaxWorkerCount = 100 * Get_HW_Concurrency();
			uint32_t WorkerExpireTimeMs = 1000 * 60;
			double ScaleUpCpuUsage = 0.8;
			double ScaleDownCpuUsage = 0.95;
			uint32_t ScaleUpWorkerCountPerSec = 1 * Get_HW_Concurrency();
		};

		ScalableThreadPool(IN const Option& a_option);
		~ScalableThreadPool();

		bool AddWorker();
		ThreadPoolStats Stop();
		ThreadPoolStats GetStats();

		template <typename FUNC, typename... PARAMS>
		inline Task_ptr Push(FUNC&& a_func,
							 PARAMS&&... a_params)
		{
			return Push(CreateTask(std::forward<FUNC>(a_func),
								   std::forward<PARAMS>(a_params)...));
		}

		inline Task_ptr Push(MOVE Task_ptr&& a_task)
		{
			auto data = std::atomic_load(&m_data);
			return PushTask(data, a_task);
		}


	private:
		struct Worker
		{
			uint32_t tid = 0;
			Semaphore notify;
			bool signaled = false;
		};

		struct Data
		{
			const Option option;
			Mutex lock;
			bool stop = false;
			std::unordered_map<Worker*, std::shared_ptr<Worker>> workers;
			std::deque<Worker*> waiters;
			SimpleQueue<Task_ptr> taskQueue;
			ThreadPoolStats stats;
			Timer::TimePoint beginScaleUpTime;
			uint32_t scaleUpCount = 0;
			Data(IN const Option& a_option) : option(a_option) {}
		};

		static bool AddWorker(REF std::shared_ptr<Data>& a_data);

		static Task_ptr PushTask(REF std::shared_ptr<Data>& a_data,
								 REF Task_ptr& a_task);

		static void Working(IN std::shared_ptr<Data> a_data,
							IN std::shared_ptr<Worker> a_worker);

		static Worker* PopWaiter(REF std::shared_ptr<Data>& a_data);

		static bool CheckExpired(REF std::shared_ptr<Data>& a_data,
								 REF std::shared_ptr<Worker>& a_worker);

		static void DeleteWorker(REF std::shared_ptr<Data>& a_data,
								 IN std::shared_ptr<Worker> a_worker);

		std::shared_ptr<Data> m_data;
	};
}

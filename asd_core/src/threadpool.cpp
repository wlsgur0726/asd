#include "stdafx.h"
#include "asd/threadpool.h"
#include "asd/lock.h"
#include "asd/semaphore.h"
#include "asd/sysres.h"
#include "asd/util.h"
#include <functional>
#include <thread>
#include <queue>
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

		void emplace_back(ELEM&& a_data)
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
			Node(ELEM&& a_mv) : data(std::move(a_mv)) {}
		};

		inline void push(Node* a_head,
						 Node* a_tail,
						 size_t a_size)
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



	struct ThreadPoolData
	{
		struct TaskObj
		{
			bool seq;
			size_t hash;
			Task_ptr task;
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
			int count = 0;
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

			Worker* Reserve(size_t a_hash,
							ThreadPoolData* a_data)
			{
				const size_t idx = a_hash % ShardCount;
				auto lock = GetLock(m_locks[idx]);
				auto& shard = m_shards[idx];

				auto& work = shard[a_hash];
				++work.count;
				if (work.worker == nullptr)
					work.worker = a_data->PickWorker();
				else
					++a_data->stats.totalConflictCount;
				return work.worker;
			}

			void Finish(size_t a_hash)
			{
				const size_t idx = a_hash % ShardCount;
				auto lock = GetLock(m_locks[idx]);
				auto& shard = m_shards[idx];

				auto it = shard.find(a_hash);
				if (it == shard.end()) {
					asd_OnErr("unknown error");
					return;
				}
				auto& work = it->second;

				if (--work.count == 0)
					shard.erase(it);
			}

		private:
			const size_t ShardCount;
			std::vector<std::unordered_map<size_t, Work>> m_shards;
			std::vector<Mutex> m_locks;
		}; //WorkingMap


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


		ThreadPoolData(const ThreadPoolOption& a_option)
			: option(a_option)
		{
			RRSeq = 0;
		}

		~ThreadPoolData()
		{
			if (workerList)
				delete[] workerList;
		}

		Worker* PickWorker()
		{
			switch (option.PickAlgorithm) {
				case ThreadPoolOption::Pick::RoundRobin:
					return PickWorker_RoundRobin();

				case ThreadPoolOption::Pick::ShortestQueue:
				default:
					return PickWorker_ShortestQueue();
			}
		}

		Worker* PickWorker_RoundRobin()
		{
			const size_t beginIdx = ++RRSeq % workerCount;
			for (size_t i=0; i<workerCount; ++i) {
				Worker* worker = &workerList[(beginIdx+i) % workerCount];
				if (worker->run)
					return worker;
			}
			return nullptr;
		}

		Worker* PickWorker_ShortestQueue()
		{
			Worker* workerA = &workerList[0];
			for (size_t i=1; i<workerCount; ++i) {
				Worker* workerB = &workerList[i];
				if (workerB->run && workerA->publicQueue->size() > workerB->publicQueue->size())
					workerA = workerB;
			}
			return workerA;
		}


		// 작업 대기
		static Task_ptr PushTask(std::shared_ptr<ThreadPoolData>& a_data,
								 TaskObj& a_task)
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
				worker = a_data->workingMap.Reserve(a_task.hash, a_data.get());
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


		// 타이머 작업 예약
		static Task_ptr PushTimerTask(std::shared_ptr<ThreadPoolData>&& a_data,
									  Timer::TimePoint a_timePoint,
									  ThreadPoolData::TaskObj&& a_task)
		{
			if (a_data == nullptr)
				return nullptr;

			auto lock = GetSharedLock(a_data->lock);

			if (!a_data->run) {
				asd_OnErr("thread-pool was stopped");
				return nullptr;
			}

			auto timer = a_data->timer ? a_data->timer.get() : &Timer::GlobalInstance();
			return timer->Push(a_timePoint,
							   &ThreadPoolData::PushTask,
							   std::move(a_data),
							   std::move(a_task));
		}


		static Notifier NeedNotify(ThreadPoolData* a_data,
								   Worker* a_worker)
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


		static bool Ready(ThreadPoolData* a_data,
						  Worker* a_worker)
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
		static void Working(std::shared_ptr<ThreadPoolData> a_data,
							uint32_t a_workerIdx)
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
						a_data->workingMap.Finish(taskObj.hash);

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
		static void DeleteWorker(std::shared_ptr<ThreadPoolData> a_data,
								 Worker* a_worker)
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
		static void PushStatTask(std::shared_ptr<ThreadPoolData> a_data)
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
				a_data->timer->Push(at, std::move(task));
			}
			else {
				TaskObj taskObj;
				taskObj.seq = false;
				taskObj.task = std::move(task);
				PushTimerTask(std::move(a_data), at, std::move(taskObj));
			}
		}
	};





	ThreadPool::ThreadPool(const ThreadPoolOption& a_option)
	{
		Reset(a_option);
	}


	ThreadPool::~ThreadPool()
	{
		asd_BeginDestructor();
		Stop();
		asd_EndDestructor();
	}


	ThreadPoolStats ThreadPool::Reset(const ThreadPoolOption& a_option)
	{
		auto stats = Stop();
		auto data = std::make_shared<ThreadPoolData>(a_option);
		std::atomic_exchange(&m_data, data);
		return stats;
	}


	ThreadPoolStats ThreadPool::Stop()
	{
		auto data = std::atomic_exchange(&m_data, std::shared_ptr<ThreadPoolData>());
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
				auto& worker = data->workerList[i];
				auto workerLock = GetLock(worker.lock);
				worker.run = false;
				ThreadPoolData::NeedNotify(data.get(), &worker).Notify();
			}
			lock.unlock();
			std::this_thread::sleep_for(Timer::Millisec(1));
		}

		return data->stats;
	}


	void ThreadPool::Start()
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

		data->workerList = new ThreadPoolData::Worker[data->workerCount];

		for (uint32_t i=0; i<data->workerCount; ++i) {
			data->workerList[i].index = i;
			std::thread(&ThreadPoolData::Working, data, i).detach();
		}
		lock.unlock();

		if (data->option.UseEmbeddedTimer)
			data->timer.reset(new Timer);

		if (data->option.CollectStats)
			ThreadPoolData::PushStatTask(data);
	}


	ThreadPoolStats ThreadPool::GetStats() const
	{
		auto data = std::atomic_load(&m_data);
		if (data == nullptr)
			return ThreadPoolStats();

		auto lock = GetSharedLock(data->lock);
		auto stats = data->stats;
		lock.unlock_shared();
		return stats;
	}


	Task_ptr ThreadPool::PushTask(Task_ptr&& a_task)
	{
		ThreadPoolData::TaskObj taskObj;
		taskObj.seq = false;
		taskObj.task = std::move(a_task);
		auto data = std::atomic_load(&m_data);
		return ThreadPoolData::PushTask(data, taskObj);
	}


	Task_ptr ThreadPool::PushTask(Timer::TimePoint a_timepoint,
								  Task_ptr&& a_task)
	{
		ThreadPoolData::TaskObj taskObj;
		taskObj.seq = false;
		taskObj.task = std::move(a_task);
		auto data = std::atomic_load(&m_data);
		return ThreadPoolData::PushTimerTask(std::move(data),
											 a_timepoint,
											 std::move(taskObj));
	}


	Task_ptr ThreadPool::PushSeqTask(size_t a_hash,
									 Task_ptr&& a_task)
	{
		ThreadPoolData::TaskObj taskObj;
		taskObj.seq = true;
		taskObj.hash = a_hash;
		taskObj.task = std::move(a_task);
		auto data = std::atomic_load(&m_data);
		return ThreadPoolData::PushTask(data, taskObj);
	}


	Task_ptr ThreadPool::PushSeqTask(Timer::TimePoint a_timepoint,
									 size_t a_hash,
									 Task_ptr&& a_task)
	{
		ThreadPoolData::TaskObj taskObj;
		taskObj.seq = true;
		taskObj.hash = a_hash;
		taskObj.task = std::move(a_task);
		auto data = std::atomic_load(&m_data);
		return ThreadPoolData::PushTimerTask(std::move(data),
											 a_timepoint,
											 std::move(taskObj));
	}







	struct ScalableThreadPoolData
	{
		struct Worker
		{
			uint32_t tid = 0;
			Semaphore notify;
			bool signaled = false;
		};

		const ScalableThreadPoolOption option;
		Mutex lock;
		bool stop = false;
		std::unordered_map<Worker*, std::shared_ptr<Worker>> workers;
		std::deque<Worker*> waiters;
		SimpleQueue<Task_ptr> taskQueue;
		ThreadPoolStats stats;
		Timer::TimePoint beginScaleUpTime;
		uint32_t scaleUpCount = 0;
		ScalableThreadPoolData(const ScalableThreadPoolOption& a_option) : option(a_option) {}


		static Task_ptr PushTask(std::shared_ptr<ScalableThreadPoolData>& a_data,
								 Task_ptr& a_task)
		{
			auto data = a_data.get();
			double cpuUsage = CpuUsage();

			auto lock = GetLock(data->lock);

			if (data->stop) {
				asd_OnErr("thread-pool was stopped");
				return nullptr;
			}

			data->stats.Push();
			data->taskQueue.emplace_back(Task_ptr(a_task));

			auto worker = PopWaiter(a_data);
			if (worker) {
				worker->signaled = true;
				lock.unlock();
				worker->notify.Post();
				return a_task;
			}

			bool needScaleUp = [&]() {
				if (cpuUsage >= data->option.ScaleUpCpuUsage)
					return false;

				auto now = Timer::Now();
				if (data->scaleUpCount <= 0 || now - data->beginScaleUpTime > Timer::Millisec(1000)) {
					data->scaleUpCount = 1;
					data->beginScaleUpTime = now;
				}
				else if (data->scaleUpCount >= data->option.ScaleUpWorkerCountPerSec)
					return false;
				else
					data->scaleUpCount++;
				return true;
			}();
			if (needScaleUp)
				AddWorker(a_data);

			return a_task;
		}


		static bool AddWorker(std::shared_ptr<ScalableThreadPoolData>& a_data)
		{
			auto lock = GetLock(a_data->lock);

			if (a_data->stop) {
				asd_OnErr("thread-pool was stopped");
				return false;
			}

			if (a_data->workers.size() >= a_data->option.MaxWorkerCount)
				return false;

			auto worker = std::make_shared<Worker>();
			a_data->workers[worker.get()] = worker;

			lock.unlock();

			std::thread(&ScalableThreadPoolData::Working, a_data, worker).detach();
			return true;
		}


		static void DeleteWorker(std::shared_ptr<ScalableThreadPoolData>& a_data,
								 std::shared_ptr<Worker> a_worker)
		{
			auto lock = GetLock(a_data->lock);

			for (auto it=a_data->waiters.begin(); it!=a_data->waiters.end(); ) {
				if (*it == a_worker.get())
					it = a_data->waiters.erase(it);
				else
					++it;
			}

			a_data->workers.erase(a_worker.get());
		}


		static void Working(std::shared_ptr<ScalableThreadPoolData> a_data,
							std::shared_ptr<Worker> a_worker)
		{
			auto data = a_data.get();
			auto worker = a_worker.get();
			worker->tid = asd::GetCurrentThreadID();

			for (Task_ptr task;;) {
				if (task) {
					asd_BeginTry();
					task->Execute();
					asd_EndTryUnknown_Default();
					data->stats.Pop();
					task.reset();
				}

				bool needScaleDown = CpuUsage() >= data->option.ScaleDownCpuUsage;
				auto lock = GetLock(data->lock);

				bool signaled = worker->signaled;
				worker->signaled = false;

				if (data->taskQueue.size() > 0 && (signaled || !needScaleDown)) {
					task = std::move(data->taskQueue.front());
					data->taskQueue.pop_front();
					continue;
				}

				if (data->stop) {
					DeleteWorker(a_data, a_worker);
					return;
				}

				if (needScaleDown)
					data->waiters.emplace_front(worker);
				else
					data->waiters.emplace_back(worker);
				lock.unlock();

				uint32_t waitTimeMs = needScaleDown ? 0 : data->option.WorkerExpireTimeMs;
				while (!worker->notify.Wait(waitTimeMs)) {
					if (CheckExpired(a_data, a_worker))
						return;
				}
			}
		}


		static Worker* PopWaiter(std::shared_ptr<ScalableThreadPoolData>& a_data)
		{
			if (a_data->waiters.empty())
				return nullptr;
			auto ret = std::move(a_data->waiters.back());
			a_data->waiters.pop_back();
			return ret;
		}


		static bool CheckExpired(std::shared_ptr<ScalableThreadPoolData>& a_data,
								 std::shared_ptr<Worker>& a_worker)
		{
			auto lock = GetLock(a_data->lock);

			if (a_worker->signaled)
				return false;

			if (a_data->workers.size() <= a_data->option.MinWorkerCount)
				return false;

			DeleteWorker(a_data, a_worker);
			return true;
		}
	};


	ScalableThreadPool::ScalableThreadPool(const ScalableThreadPoolOption& a_option)
		: m_data(std::make_shared<ScalableThreadPoolData>(a_option))
	{
		CpuUsage(); // 인스턴스 초기화를 위해 호출

		auto lock = GetLock(m_data->lock);
		for (size_t i=0; i<m_data->option.MinWorkerCount; ++i)
			AddWorker();
	}


	bool ScalableThreadPool::AddWorker()
	{
		return ScalableThreadPoolData::AddWorker(m_data);
	}


	ThreadPoolStats ScalableThreadPool::Stop()
	{
		auto lock = GetLock(m_data->lock);

		m_data->stop = true;

		for (; m_data->workers.size()>0; lock.lock()) {
			for (auto& it : m_data->workers) {
				auto& worker = it.second;
				if (GetCurrentThreadID() == worker->tid)
					asd_RaiseException("self-deadlock");
			}

			ScalableThreadPoolData::Worker* worker;
			while (worker = ScalableThreadPoolData::PopWaiter(m_data))
				worker->notify.Post();
			lock.unlock();
			std::this_thread::sleep_for(Timer::Millisec(1));
		}

		return GetStats();
	}


	ScalableThreadPool::~ScalableThreadPool()
	{
		asd_BeginDestructor();
		Stop();
		asd_EndDestructor();
	}


	ThreadPoolStats ScalableThreadPool::GetStats()
	{
		auto lock = GetLock(m_data->lock);
		m_data->stats.sleepingThreadCount = m_data->waiters.size();
		m_data->stats.threadCount = m_data->workers.size();

		m_data->stats.Refresh();
		return m_data->stats;
	}


	Task_ptr ScalableThreadPool::PushTask(Task_ptr&& a_task)
	{
		auto data = std::atomic_load(&m_data);
		return ScalableThreadPoolData::PushTask(data, a_task);
	}


	Task_ptr ScalableThreadPool::PushTask(Timer::TimePoint a_timepoint,
										  Task_ptr&& a_task)
	{
		auto data = std::atomic_load(&m_data);
		return Timer::GlobalInstance().Push(a_timepoint,
											&ScalableThreadPoolData::PushTask,
											std::move(data),
											std::move(a_task));
	}
}

#pragma once
#include "asdbase.h"
#include "lock.h"
#include "timer.h"
#include "semaphore.h"
#include <functional>
#include <thread>
#include <queue>
#include <unordered_set>
#include <unordered_map>


namespace asd
{
	struct QueueStats
	{
		uint64_t totalPushCount = 0;
		uint64_t totalProcCount = 0;
		uint64_t totalConflictCount = 0;

		uint64_t curMinQueueLenth = 0;
		uint64_t curMaxQueueLenth = 0;

		uint64_t recentPushCount = 0;
		uint64_t recentTotalPushCount = 0;
		uint64_t recentMinQueueLenth = 0;
		uint64_t recentMaxQueueLenth = 0;

		Timer::Millisec recentPeriod = Timer::Millisec(0);
		Timer::TimePoint recentRefreshTime = Timer::Now();


		uint64_t WatingCount() const
		{
			return totalPushCount - totalProcCount;
		}

		double RecentWaitingTimeMs() const
		{
			double avgLen;
			if (recentMinQueueLenth <= recentMaxQueueLenth)
				avgLen = (recentMinQueueLenth + recentMaxQueueLenth) / 2.0;
			else
				avgLen = 0;

			double period = recentPeriod.count();
			if (period == 0)
				return std::numeric_limits<double>::max();

			return avgLen / (recentPushCount / period);
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
			recentTotalPushCount = totalProcCount;
			recentMinQueueLenth = curMinQueueLenth;
			recentMaxQueueLenth = curMaxQueueLenth;

			curMinQueueLenth = curMaxQueueLenth = WatingCount();
		}

		void Push()
		{
			totalPushCount++;
			curMaxQueueLenth = max(curMaxQueueLenth, WatingCount());
		}

		void Pop()
		{
			totalProcCount++;
			curMinQueueLenth = min(curMinQueueLenth, WatingCount());
		}
	};



	template <typename TASK_TYPE>
	class ThreadPoolData
	{
	protected:
		template <typename T>
		friend class ThreadPoolTemplate;

		using TaskObj = TASK_TYPE;
		using TaskQueue = std::queue<TaskObj>;

		struct Worker
		{
			Semaphore m_event;
			TaskQueue m_readyQueue;
		};

		virtual ~ThreadPoolData() asd_noexcept
		{
			if (m_refreshTask != nullptr)
				m_refreshTask->Cancel();
		}

		virtual bool TryPop(REF Worker& a_worker,
							OUT TaskObj& a_task) asd_noexcept
		{
			if (a_worker.m_readyQueue.size() > 0) {
				a_task = std::move(a_worker.m_readyQueue.front());
				a_worker.m_readyQueue.pop();
				return true;
			}
			if (m_waitQueue.size() > 0) {
				a_task = std::move(m_waitQueue.front());
				m_waitQueue.pop();
				return true;
			}
			return false;
		}

		virtual void OnExecute(REF TaskObj& a_task)
		{
			asd_RAssert(false, "not impl");
		}

		virtual void OnFinish(IN TaskObj& a_task) asd_noexcept
		{
		}

		bool Running(IN const Worker& a_worker) const asd_noexcept
		{
			if (m_run)
				return true;
			if (m_overtime == false)
				return false;
			if (m_waitQueue.size() > 0)
				return true;
			if (a_worker.m_readyQueue.size() > 0)
				return true;
			return false;
		}


		// 아래 모든 데이터들을 보호하는 락
		mutable Mutex m_lock;

		// 작업 큐
		TaskQueue m_waitQueue;

		// 동작 상태
		bool m_run = false;

		// 이 변수가 true이면 
		// 종료명령이 떨어져도 남은 작업을 모두 처리한 후 종료한다.
		bool m_overtime = true;

		// 작업을 수행하는 쓰레드 관련
		std::unordered_map<uint32_t, Worker*>	m_workers;
		std::unordered_set<Worker*>				m_standby;
		size_t									m_threadCount;
		std::vector<std::thread>				m_threads;
		Timer									m_timer;

		// 통계정보
		QueueStats m_stats;
		Task_ptr m_refreshTask;
	};


	template <typename THREAD_POOL_DATA>
	class ThreadPoolTemplate
	{
	protected:
		using DataImpl = THREAD_POOL_DATA;
		using ThisType = ThreadPoolTemplate<DataImpl>;
		using TaskObj = typename DataImpl::TaskObj;
		using TaskQueue = typename DataImpl::TaskQueue;
		using Data = ThreadPoolData<TaskObj>;


	public:
		// 초기화
		ThreadPoolTemplate(IN uint32_t a_threadCount = Get_HW_Concurrency())
		{
			Reset(a_threadCount);
		}


		ThisType& Reset(IN uint32_t a_threadCount = Get_HW_Concurrency())
		{
			Stop();
			m_data.reset(new DataImpl);
			m_data->m_threadCount = a_threadCount;
			return *this;
		}


		ThisType& Start()
		{
			auto data = std::atomic_load(&m_data);
			if (data == nullptr)
				asd_RaiseException("stoped thread pool");

			auto lock = GetLock(data->m_lock);
			if (data->m_run)
				asd_RaiseException("already running");

			data->m_run = true;
			data->m_threads.resize(m_data->m_threadCount);
			for (auto& t : data->m_threads) {
				t = std::thread([data]() mutable
				{
					auto lock = GetLock(data->m_lock);
					Data::Worker worker;
					data->m_workers.emplace(GetCurrentThreadID(), &worker);
					for (; data->Running(worker); lock.lock()) {
						lock.unlock();
						Poll_Internal(data.get(),
									  worker,
									  std::numeric_limits<uint32_t>::max(),
									  std::numeric_limits<size_t>::max());
					}
				});
			}

			PushStatTask(data);
			return *this;
		}


		// 작업을 기다리고 처리하는 함수
		//   a_timeoutMs       :  작업이 없을 때 최대로 대기하는 시간 (밀리초)
		//   a_procCountLimit  :  처리할 작업의 개수 제한
		size_t Poll(IN uint32_t a_timeoutMs = std::numeric_limits<uint32_t>::max(),
					IN size_t a_procCountLimit = std::numeric_limits<size_t>::max()) asd_noexcept
		{
			auto data = std::atomic_load(&m_data);
			if (data == nullptr)
				return 0;

			thread_local ObjectPool<Data::Worker, asd::NoLock, true> t_workers;
			auto worker = t_workers.Alloc();
			auto ret = Poll_Internal(data.get(),
									 *worker,
									 a_timeoutMs,
									 a_procCountLimit);
			t_workers.Free(worker);
			return ret;
		}


		// 종료
		// a_overtime이 true이면 남은 작업을 모두 처리할 때까지 기다린다.
		void Stop(IN bool a_overtime = true)
		{
			auto data = std::atomic_exchange(&m_data, std::shared_ptr<DataImpl>());
			if (data == nullptr)
				return;

			auto lock = GetLock(data->m_lock);
			if (data->m_run == false)
				return;

			if (data->m_workers.find(GetCurrentThreadID()) != data->m_workers.end())
				asd_RaiseException("deadlock");

			data->m_run = false;
			data->m_overtime = a_overtime;

			if (data->m_refreshTask != nullptr)
				data->m_refreshTask->Cancel(data->m_stats.recentPeriod.count() == 0);

			while (data->m_threads.size() > 0) {
				while (data->m_standby.size() > 0) {
					auto it = data->m_standby.begin();
					(*it)->m_event.Post();
					data->m_standby.erase(it);
				}
				auto thread = std::move(*data->m_threads.rbegin());
				data->m_threads.resize(data->m_threads.size() - 1);
				lock.unlock();
				thread.join();
				lock.lock();
			}

			m_lastStats = data->m_stats;
		}


		QueueStats GetStats() const
		{
			auto data = std::atomic_load(&m_data);
			if (data == nullptr)
				return m_lastStats;

			auto lock = GetLock(data->m_lock);
			auto stats = data->m_stats;
			lock.unlock();
			return stats;
		}


		virtual ~ThreadPoolTemplate() asd_noexcept
		{
			asd_BeginDestructor();
			Stop();
			asd_EndDestructor();
		}


	protected:
		static size_t Poll_Internal(REF Data* a_data,
									REF typename Data::Worker& a_worker,
									IN uint32_t a_timeoutMs,
									IN size_t a_procCountLimit) asd_noexcept
		{
			if (a_procCountLimit <= 0)
				return 0;

			size_t procCount = 0;
			for (auto lock=GetLock(a_data->m_lock); a_data->Running(a_worker); ) {
				// 작업 확인
				TaskObj task;
				if (a_data->TryPop(a_worker, task) == false) {
					if (a_data->m_waitQueue.size() > 0) {
						lock.unlock();
						std::this_thread::sleep_for(std::chrono::nanoseconds(0));
						lock.lock();
						continue;
					}
					else {
						a_data->m_standby.emplace(&a_worker);
						lock.unlock();
						bool on = a_worker.m_event.Wait(a_timeoutMs);
						lock.lock();
						if (on)
							continue;

						// 타임아웃
						a_data->m_standby.erase(&a_worker);
						break;
					}
				}

				// 작업 수행
				lock.unlock();
				asd_BeginTry();
				a_data->OnExecute(task);
				asd_EndTryUnknown_Default();

				lock.lock();
				a_data->OnFinish(task);
				a_data->m_stats.Pop();
				if (++procCount >= a_procCountLimit)
					break;
			}
			return procCount;
		}


		// 작업 등록
		static void PushTask(REF std::shared_ptr<DataImpl>& a_data,
							 MOVE TaskObj& a_task) asd_noexcept
		{
			if (a_data == nullptr)
				return;

			auto lock = GetLock(a_data->m_lock);
			if (a_data->m_run == false)
				return;

			a_data->m_waitQueue.emplace(std::move(a_task));
			a_data->m_stats.Push();

			if (a_data->m_standby.size() > 0) {
				auto it = a_data->m_standby.begin();
				(*it)->m_event.Post();
				a_data->m_standby.erase(it);
			}
		}


		// 타이머 작업 등록
		static Task_ptr PushTimerTask(MOVE std::shared_ptr<DataImpl>&& a_data,
									  IN Timer::TimePoint a_timePoint,
									  MOVE TaskObj&& a_task) asd_noexcept
		{
			if (a_data == nullptr)
				return nullptr;

			auto lock = GetLock(a_data->m_lock);
			if (a_data->m_run == false)
				return nullptr;

			return a_data->m_timer.PushAt(a_timePoint,
										  &ThisType::PushTask,
										  std::move(a_data),
										  std::move(a_task));
		}


		// 통계
		static void PushStatTask(IN std::shared_ptr<DataImpl> a_data)
		{
			if (a_data == nullptr)
				return;

			if (a_data->m_run == false)
				return;

			a_data->m_refreshTask = a_data->m_timer.PushAfter(1000, [a_data]() mutable
			{
				auto lock = GetLock(a_data->m_lock);
				a_data->m_stats.Refresh();
				PushStatTask(a_data);
			});
		}


		std::shared_ptr<DataImpl> m_data;
		QueueStats m_lastStats;
	};



	class DefaultThreadPoolData : public ThreadPoolData<Task_ptr>
	{
		virtual void OnExecute(REF TaskObj& a_task)
		{
			a_task->Execute();
			a_task.reset();
		}
	};

	class ThreadPool : public ThreadPoolTemplate<DefaultThreadPoolData>
	{
	public:
		using BaseType = ThreadPoolTemplate<DefaultThreadPoolData>;

		using BaseType::BaseType;


		template <typename FUNC, typename... PARAMS>
		inline void Push(FUNC&& a_func,
						 PARAMS&&... a_params) asd_noexcept
		{
			auto task = CreateTask(std::forward<FUNC>(a_func),
								   std::forward<PARAMS>(a_params)...);
			auto data = std::atomic_load(&m_data);
			PushTask(data, task);
		}


		template <typename FUNC, typename... PARAMS>
		inline Task_ptr PushAfter(IN uint32_t a_afterMs,
								  FUNC&& a_func,
								  PARAMS&&... a_params) asd_noexcept
		{
			return PushAt(Timer::Now() + Timer::Millisec(a_afterMs),
						  std::forward<FUNC>(a_func),
						  std::forward<PARAMS>(a_params)...);
		}


		template <typename FUNC, typename... PARAMS>
		inline Task_ptr PushAt(IN Timer::TimePoint a_timepoint,
							   FUNC&& a_func,
							   PARAMS&&... a_params) asd_noexcept
		{
			auto task = CreateTask(std::forward<FUNC>(a_func),
								   std::forward<PARAMS>(a_params)...);
			auto data = std::atomic_load(&m_data);
			return PushTimerTask(std::move(data),
								 a_timepoint,
								 std::move(task));
		}


		virtual ~ThreadPool() asd_noexcept
		{
			asd_BeginDestructor();
			Stop();
			asd_EndDestructor();
		}
	};



	template <typename Key, typename... MapOpts>
	class SequentialThreadPoolData
		: public ThreadPoolData< std::tuple<Key, bool, Task_ptr> >
	{
		using BaseType = ThreadPoolData< std::tuple<Key, bool, Task_ptr> >;

		struct Work
		{
			size_t m_count = 1;
			Worker* m_worker;
			Work(Worker* worker = nullptr) : m_worker(worker) {}
		};
		std::unordered_map<Key, Work, MapOpts...> m_workingMap;

		virtual bool TryPop(REF Worker& a_worker,
							OUT TaskObj& a_task) asd_noexcept
		{
			if (a_worker.m_readyQueue.size() > 0) {
				a_task = std::move(a_worker.m_readyQueue.front());
				a_worker.m_readyQueue.pop();
				return true;
			}

			if (m_waitQueue.empty())
				return false;

			a_task = std::move(m_waitQueue.front());
			m_waitQueue.pop();

			if (std::get<1>(a_task) == false)
				return true;

			auto emplace = m_workingMap.emplace(std::get<0>(a_task), Work(&a_worker));
			if (emplace.second)
				return true;

			auto& work = emplace.first->second;
			asd_DAssert(work.m_worker != &a_worker);

			m_stats.totalConflictCount++;
			work.m_worker->m_readyQueue.emplace(std::move(a_task));
			work.m_count++;
			if (m_standby.erase(work.m_worker) == 1)
				work.m_worker->m_event.Post();
			return false;
		}

		virtual void OnExecute(REF TaskObj& a_task)
		{
			std::get<2>(a_task)->Execute();
		}

		virtual void OnFinish(IN TaskObj& a_task) asd_noexcept
		{
			if (std::get<1>(a_task) == false)
				return;

			auto it = m_workingMap.find(std::get<0>(a_task));
			asd_ChkErrAndRet(it == m_workingMap.end(), "unknown error");
			auto& work = it->second;
			if (--work.m_count == 0)
				m_workingMap.erase(it);
		}
	};

	// Key가 동일한 작업이 수행중인 경우 
	// 선작업이 끝날 때까지 실행을 보류하여 
	// 실행순서를 보장해주는 쓰레드풀
	template <typename Key, typename... MapOpts>
	class SequentialThreadPool 
		: public ThreadPoolTemplate< SequentialThreadPoolData<Key, MapOpts...> >
	{
	public:
		template <typename FUNC, typename... PARAMS>
		inline void Push(FUNC&& a_func,
						 PARAMS&&... a_params) asd_noexcept
		{
			auto task = CreateTask(std::forward<FUNC>(a_func),
								   std::forward<PARAMS>(a_params)...);
			auto taskObj = std::make_tuple(Key(), false, std::move(task));
			auto data = std::atomic_load(&m_data);
			PushTask(data, taskObj);
		}


		template <typename FUNC, typename... PARAMS>
		inline Task_ptr PushAfter(IN uint32_t a_afterMs,
								  FUNC&& a_func,
								  PARAMS&&... a_params) asd_noexcept
		{
			return PushAt(Timer::Now() + Timer::Millisec(a_afterMs),
						  std::forward<FUNC>(a_func),
						  std::forward<PARAMS>(a_params)...);
		}


		template <typename FUNC, typename... PARAMS>
		inline Task_ptr PushAt(IN Timer::TimePoint a_timepoint,
							   FUNC&& a_func,
							   PARAMS&&... a_params) asd_noexcept
		{
			auto task = CreateTask(std::forward<FUNC>(a_func),
								   std::forward<PARAMS>(a_params)...);
			auto data = std::atomic_load(&m_data);
			return PushTimerTask(std::move(data),
								 a_timepoint,
								 TaskObj(Key(), false, std::move(task)));
		}


		template <typename KEY, typename FUNC, typename... PARAMS>
		inline void PushSeq(KEY&& a_key,
							FUNC&& a_func,
							PARAMS&&... a_params) asd_noexcept
		{
			auto task = CreateTask(std::forward<FUNC>(a_func),
								   std::forward<PARAMS>(a_params)...);
			TaskObj taskObj(std::forward<KEY>(a_key), true, std::move(task));
			auto data = std::atomic_load(&m_data);
			PushTask(data, taskObj);
		}


		template <typename KEY, typename FUNC, typename... PARAMS>
		inline Task_ptr PushSeqAfter(IN uint32_t a_afterMs,
									 KEY&& a_key,
									 FUNC&& a_func,
									 PARAMS&&... a_params) asd_noexcept
		{
			return PushSeqAt(Timer::Now() + Timer::Millisec(a_afterMs),
							 std::forward<KEY>(a_key),
							 std::forward<FUNC>(a_func),
							 std::forward<PARAMS>(a_params)...);
		}


		template <typename KEY, typename FUNC, typename... PARAMS>
		inline Task_ptr PushSeqAt(IN Timer::TimePoint a_timepoint,
								  KEY&& a_key,
								  FUNC&& a_func,
								  PARAMS&&... a_params) asd_noexcept
		{
			auto task = CreateTask(std::forward<FUNC>(a_func),
								   std::forward<PARAMS>(a_params)...);
			auto data = std::atomic_load(&m_data);
			return PushTimerTask(std::move(data),
								 a_timepoint,
								 TaskObj(std::forward<KEY>(a_key), true, std::move(task)));
		}


		virtual ~SequentialThreadPool() asd_noexcept
		{
			asd_BeginDestructor();
			Stop();
			asd_EndDestructor();
		}

	}; // SequentialThreadPool
}


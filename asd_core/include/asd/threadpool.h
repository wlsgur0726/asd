#pragma once
#include "asdbase.h"
#include "lock.h"
#include "timer.h"
#include <functional>
#include <thread>
#include <queue>
#include <unordered_map>


namespace asd
{
	struct ThreadPoolData;
	class ThreadPool
	{
	public:
		typedef std::function<void(const std::exception&)>	ExceptionHandler;


	public:
		// 초기화
		ThreadPool(IN uint32_t a_threadCount = std::thread::hardware_concurrency());
		ThreadPool& Reset(IN uint32_t a_threadCount = std::thread::hardware_concurrency());
		ThreadPool&	Start();


		// 작업을 기다리고 처리하는 함수
		//   a_timeoutMs       :  작업이 없을 때 최대로 대기하는 시간 (밀리초)
		//   a_procCountLimit  :  처리할 작업의 개수 제한
		size_t Poll(IN uint32_t a_timeoutMs = std::numeric_limits<uint32_t>::max(),
					IN size_t a_procCountLimit = std::numeric_limits<size_t>::max()) asd_noexcept;


		// 작업 등록
		template <typename FUNC, typename... PARAMS>
		inline void Push(FUNC&& a_func,
						 PARAMS&&... a_params) asd_noexcept
		{
			auto task = CreateTask(std::forward<FUNC>(a_func),
								   std::forward<PARAMS>(a_params)...);
			PushTask(std::move(task));
		}

		void PushTask(MOVE Task_ptr&& a_task) asd_noexcept;


		// 타이머 작업 등록
		template <typename FUNC, typename... PARAMS>
		inline Task_ptr PushAfter(IN uint32_t a_afterMs,
								  FUNC&& a_func,
								  PARAMS&&... a_params) asd_noexcept
		{
			return PushAt(Now() + Milliseconds(a_afterMs),
						  std::forward<FUNC>(a_func),
						  std::forward<PARAMS>(a_params)...);
		}

		// 타이머 작업 등록
		template <typename FUNC, typename... PARAMS>
		inline Task_ptr PushAt(IN Timer::TimePoint a_timePoint,
							   FUNC&& a_func,
							   PARAMS&&... a_params) asd_noexcept
		{
			auto task = CreateTask(std::forward<FUNC>(a_func),
								   std::forward<PARAMS>(a_params)...);
			PushTimerTask(a_timePoint, task);
			return task;
		}

		Task_ptr PushTimerTask(IN Timer::TimePoint a_timePoint,
							   IN const Task_ptr& a_task) asd_noexcept;


		// 종료
		// a_overtime이 true이면 남은 작업을 모두 처리할 때까지 기다린다.
		void Stop(IN bool a_overtime = true);
		virtual ~ThreadPool() asd_noexcept;

		// operator
		ThreadPool(IN const ThreadPool&)				= delete;
		ThreadPool& operator = (IN const ThreadPool&)	= delete;
		ThreadPool(MOVE ThreadPool&&)					= default;
		ThreadPool& operator = (MOVE ThreadPool&&)		= default;


	private:
		std::shared_ptr<ThreadPoolData> m_data;

	};



	// Key가 동일한 작업이 수행중인 경우 
	// 선작업이 끝날 때까지 실행을 보류하여 
	// 실행순서를 보장해주는 쓰레드풀
	template <typename Key, typename... MapOpts>
	class SequentialThreadPool : public ThreadPool
	{
	private:
		using ThisType = SequentialThreadPool<Key, MapOpts...>;
		using SeqTask = std::pair<Key, Task_ptr>;
		using SeqTaskQueue = std::queue<SeqTask>;

	public:
		struct Report
		{
			uint64_t totalProcCount = 0;
			uint64_t conflictCount = 0;
			double GetConflictRate() const
			{
				if (totalProcCount == 0)
					return std::numeric_limits<double>::max();
				return conflictCount / (double)totalProcCount;
			}
		};


		using ThreadPool::ThreadPool;


		virtual ~SequentialThreadPool()
		{
			ThreadPool::Stop();
		}


		template <typename KEY, typename FUNC, typename... PARAMS>
		inline void PushSeq(KEY&& a_key,
							FUNC&& a_func,
							PARAMS&&... a_params) asd_noexcept
		{
			auto task = CreateTask(std::forward<FUNC>(a_func),
								   std::forward<PARAMS>(a_params)...);
			Push(std::mem_fn(&ThisType::OnSequentialTask),
				 this,
				 std::forward<KEY>(a_key),
				 std::move(task));
		}


		template <typename KEY, typename FUNC, typename... PARAMS>
		inline Task_ptr PushSeqAt(IN Timer::TimePoint a_timepoint,
								  KEY&& a_key,
								  FUNC&& a_func,
								  PARAMS&&... a_params) asd_noexcept
		{
			auto task = CreateTask(std::forward<FUNC>(a_func),
								   std::forward<PARAMS>(a_params)...);
			return PushAt(a_timepoint,
						  std::mem_fn(&ThisType::OnSequentialTask),
						  this,
						  std::forward<KEY>(a_key),
						  std::move(task));
		}


		template <typename KEY, typename FUNC, typename... PARAMS>
		inline Task_ptr PushSeqAfter(IN uint32_t a_afterMs,
									 KEY&& a_key,
									 FUNC&& a_func,
									 PARAMS&&... a_params) asd_noexcept
		{
			auto task = CreateTask(std::forward<FUNC>(a_func),
								   std::forward<PARAMS>(a_params)...);
			return PushAfter(a_afterMs,
							 std::mem_fn(&ThisType::OnSequentialTask),
							 this,
							 std::forward<KEY>(a_key),
							 std::move(task));
		}


		void OnSequentialTask(REF Key& a_key,
							  REF Task_ptr& a_task) asd_noexcept
		{
			thread_local SeqTaskQueue t_queue;

			// sequence check
			auto lock = GetLock(m_lock);
			auto emplace = m_workingMap.emplace(a_key, &t_queue);
			if (emplace.second == false) {
				// 동일한 작업이 이미 수행중인 경우
				// 해당 작업을 수행중인 쓰레드가 처리하도록 큐잉하고 종료
				auto& queue = *emplace.first->second;
				queue.emplace(SeqTask(std::move(a_key), std::move(a_task)));
				++m_report.conflictCount;
				lock.unlock();
				return;
			}

			for (;;) {
				// 1. 작업 수행
				lock.unlock();
				a_task->Execute();

				// 2. 작업 수행하는 도중 동일한 작업이 큐잉되었는지 확인
				lock.lock();
				++m_report.totalProcCount;
				if (t_queue.empty()) {
					// 동일한 작업이 없는 경우 등록을 해제하고 종료
					m_workingMap.erase(a_key);
					break;
				}

				// 3. 동일한 작업이 또 들어온 경우 해당 작업을 우선 처리
				auto nextTask = std::move(t_queue.front());
				t_queue.pop();
				a_key = std::move(nextTask.first);
				a_task = std::move(nextTask.second);
			}
		}


		Report GetReport() asd_noexcept
		{
			auto mtx = GetLock(m_lock);
			return m_report;
		}


	private:
		Report m_report;
		Mutex m_lock;
		std::unordered_map<Key, SeqTaskQueue*, MapOpts...> m_workingMap;


	}; // SequentialThreadPool
}


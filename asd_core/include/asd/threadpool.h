#pragma once
#include "asdbase.h"
#include "lock.h"
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
		typedef std::function<void()>						Task;
		typedef std::function<void(const std::exception&)>	ExceptionHandler;


	public:
		// 초기화
		ThreadPool(IN uint32_t a_threadCount = std::thread::hardware_concurrency());
		ThreadPool& Reset(IN uint32_t a_threadCount = std::thread::hardware_concurrency());
		ThreadPool&	SetExceptionHandler(IN const ExceptionHandler& a_eh);
		ThreadPool&	Start();

		// 작업을 기다리고 처리하는 함수
		//   a_timeoutMs       :  작업이 없을 때 최대로 대기하는 시간 (밀리초)
		//   a_procCountLimit  :  처리할 작업의 개수 제한. starvation 방지에 활용
		size_t Poll(IN uint32_t a_timeoutMs = std::numeric_limits<uint32_t>::max(),
					IN size_t a_procCountLimit = std::numeric_limits<size_t>::max());

		// 작업 등록
		ThreadPool& PushTask(MOVE Task&& a_task);
		ThreadPool& PushTask(IN const Task& a_task);

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
		ThreadPoolData* m_data = nullptr;


	};



	// Key의 Hash가 동일한 작업이 수행중인 경우 
	// 선작업이 끝날 때까지 실행을 보류하여 
	// 실행순서를 보장해주는 쓰레드풀
	template <typename Key,
			  typename Hash = std::hash<Key>>
	class SequentialThreadPool : public ThreadPool
	{
	public:
		typedef std::function<void(Key&)>		SequentialTask;
		typedef size_t							HashKey;


	private:
		typedef std::pair<Key, SequentialTask>	TaskObject;
		typedef std::queue<TaskObject>			TaskObjectQueue;
		struct AlreadyHashed
		{
			inline constexpr size_t operator()(const HashKey h) const
			{
				return h;
			}
		};

		typedef std::unordered_map<HashKey, TaskObjectQueue*, AlreadyHashed>	WorkingMap;
		WorkingMap	m_workingMap;
		Mutex		m_lock;


	public:
		using ThreadPool::ThreadPool;


		virtual ~SequentialThreadPool()
		{
			ThreadPool::Stop();
		}


		inline SequentialThreadPool& PushTask(IN const Key& a_key,
											  IN const SequentialTask& a_task)
		{
			return PushTask(std::move(Key(a_key)),
							std::move(SequentialTask(a_task)));
		}


		SequentialThreadPool& PushTask(MOVE Key&& a_key,
									   MOVE SequentialTask&& a_task)
		{
			assert(a_task != nullptr);
			const HashKey hashKey = Hash()(a_key);
			TaskObject temp;
			temp.first = std::move(a_key);
			temp.second = std::move(a_task);
			ThreadPool::PushTask([	this,
									hashKey,
									taskObj = std::move(temp) ]() mutable
			{
				thread_local TaskObjectQueue t_queue;

				// sequence check
				auto mtx = GetLock(m_lock);
				auto emplace_result = m_workingMap.emplace(hashKey, &t_queue);
				if (emplace_result.second == false) {
					// 동일한 작업이 이미 수행중인 경우
					// 해당 작업을 수행중인 쓰레드가 처리하도록 큐잉하고 종료
					emplace_result.first->second->push(std::move(taskObj));
					++m_report.conflictCount;
					mtx.unlock();
					return;
				}

				for (;;) {
					// 1. 작업 수행
					auto& key  = taskObj.first;
					auto& task = taskObj.second;
					mtx.unlock();
					assert(task != nullptr);
					task(key);

					// 2. 작업 수행하는 도중 동일한 작업이 큐잉되었는지 확인
					mtx.lock();
					++m_report.totalProcCount;
					if (t_queue.empty()) {
						// 동일한 작업이 없는 경우 등록을 해제하고 종료
						m_workingMap.erase(hashKey);
						break;
					}

					// 3. 동일한 작업이 또 들어온 경우 해당 작업을 우선 처리
					taskObj = std::move(t_queue.front());
					t_queue.pop();
				}
				// 완료
			});
			return *this;
		}


		struct Report
		{
			uint64_t	totalProcCount = 0;
			uint64_t	conflictCount = 0;
			double GetConflictRate() const
			{
				if (totalProcCount == 0)
					return std::numeric_limits<double>::max();
				return conflictCount / (double)totalProcCount;
			}
		} m_report;

		Report GetReport() asd_noexcept
		{
			auto mtx = GetLock(m_lock);
			return m_report;
		}


	}; // SequentialThreadPool
}


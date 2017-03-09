#include "asd_pch.h"
#include "asd/threadpool.h"
#include "asd/threadutil.h"
#include "asd/semaphore.h"
#include <atomic>
#include <queue>
#include <deque>
#include <vector>
#include <unordered_set>

namespace asd
{
	struct ThreadPoolData
	{
		ThreadPoolData(IN const uint32_t a_threadCount)
			: threadCount(a_threadCount)
		{
		}

		// 아래 모든 데이터들을 보호하는 락
		Mutex tpLock;

		// 작업 큐
		std::queue<ThreadPool::Task> taskQueue;

		// 동작 상태
		bool run = false;

		// 이 변수가 true이면 
		// 종료명령이 떨어져도 남은 작업을 모두 처리한 후 종료한다.
		bool overtime = true;

		// 작업을 수행하는 쓰레드 관련
		const uint32_t					threadCount;
		std::vector<std::thread>		threads;
		std::unordered_set<uint32_t>	workers;
		std::deque<Semaphore*>			waitingList;
	};



	ThreadPool::ThreadPool(IN uint32_t a_threadCount /*= std::thread::hardware_concurrency()*/)
	{
		Reset(a_threadCount);
	}



	ThreadPool& ThreadPool::Reset(IN uint32_t a_threadCount /*= std::thread::hardware_concurrency()*/)
	{
		Stop();
		m_data.reset(new ThreadPoolData(a_threadCount));
		m_data->threads.resize(m_data->threadCount);
		return *this;
	}



	inline size_t Poll(REF std::shared_ptr<ThreadPoolData>& a_data,
					   IN uint32_t a_timeoutMs,
					   IN size_t a_procCountLimit) asd_noexcept
	{
		if (a_data == nullptr)
			return 0;

		if (a_procCountLimit <= 0)
			return 0;

		thread_local Semaphore t_event;
		size_t procCount = 0;
		auto lock = GetLock(a_data->tpLock);

		// 이벤트 체크
		while (a_data->run || (a_data->overtime && !a_data->taskQueue.empty())) {
			if (a_data->taskQueue.empty()) {
				// 대기
				a_data->waitingList.emplace_back(&t_event);
				lock.unlock();
				bool ev = t_event.Wait(a_timeoutMs);
				lock.lock();
				if (ev)
					continue;

				// 타임아웃
				auto it = std::find(a_data->waitingList.begin(), a_data->waitingList.end(), &t_event);
				if (it != a_data->waitingList.end())
					a_data->waitingList.erase(it);
				break;
			}

			// 작업을 접수
			assert(a_data->taskQueue.empty() == false);
			ThreadPool::Task task(std::move(a_data->taskQueue.front()));
			a_data->taskQueue.pop();

			// 작업 수행
			lock.unlock();
			assert(task != nullptr);
			task();
			if (++procCount >= a_procCountLimit)
				break;

			lock.lock();
		}
		return procCount;
	}



	ThreadPool& ThreadPool::Start()
	{
		auto data = std::atomic_load(&m_data);
		if (data == nullptr)
			asd_RaiseException("stoped thread pool");

		auto lock = GetLock(data->tpLock);
		if (data->run)
			asd_RaiseException("already running");

		data->run = true;
		assert(data->threads.size() == data->threadCount);
		for (auto& t : data->threads) {
			t = std::thread([data]() mutable
			{
				auto lock = GetLock(data->tpLock);
				data->workers.emplace(GetCurrentThreadID());
				lock.unlock();
				while (data->run) {
					asd::Poll(data,
							  100,
							  std::numeric_limits<size_t>::max());
				}
			});
		}
		return *this;
	}



	size_t ThreadPool::Poll(IN uint32_t a_timeoutMs /*= std::numeric_limits<uint32_t>::max()*/,
							IN size_t a_procCountLimit /*= std::numeric_limits<size_t>::max()*/) asd_noexcept
	{
		auto data = std::atomic_load(&m_data);
		return asd::Poll(data,
						 a_timeoutMs,
						 a_procCountLimit);
	}



	inline void PushTask(REF std::shared_ptr<ThreadPoolData>& a_data,
						 MOVE ThreadPool::Task&& a_task) asd_noexcept
	{
		if (a_data == nullptr)
			return;

		assert(a_task != nullptr);
		auto lock = GetLock(a_data->tpLock);
		if (a_data->run == false)
			return;

		a_data->taskQueue.emplace(std::move(a_task));

		if (a_data->waitingList.size() > 0) {
			a_data->waitingList.front()->Post();
			a_data->waitingList.pop_front();
		}
	}



	ThreadPool& ThreadPool::PushTask(MOVE Task&& a_task) asd_noexcept
	{
		auto data = std::atomic_load(&m_data);
		asd::PushTask(data, std::move(a_task));
		return *this;
	}



	ThreadPool& ThreadPool::PushTask(IN const Task& a_task) asd_noexcept
	{
		auto data = std::atomic_load(&m_data);
		asd::PushTask(data, Task(a_task));
		return *this;
	}



	inline uint64_t PushTaskAt(REF std::shared_ptr<ThreadPoolData>& a_data,
							   IN Timer::TimePoint a_timePoint,
							   MOVE ThreadPool::Task&& a_task) asd_noexcept
	{
		if (a_data == nullptr)
			return 0;

		return Timer::PushAt(a_timePoint, [a_data, task=std::move(a_task)]() mutable
		{
			asd::PushTask(a_data, std::move(task));
		});
	}



	uint64_t ThreadPool::PushTaskAt(IN Timer::TimePoint a_timePoint,
									MOVE Task&& a_task) asd_noexcept
	{
		auto data = std::atomic_load(&m_data);
		return asd::PushTaskAt(data, a_timePoint, std::move(a_task));
	}



	uint64_t ThreadPool::PushTaskAt(IN Timer::TimePoint a_timePoint,
									IN const Task& a_task) asd_noexcept
	{
		auto data = std::atomic_load(&m_data);
		return asd::PushTaskAt(data, a_timePoint, Task(a_task));
	}



	uint64_t ThreadPool::PushTaskAfter(IN uint32_t a_afterMs,
									   MOVE Task&& a_task) asd_noexcept
	{
		return PushTaskAt(Timer::Now() + Timer::Milliseconds(a_afterMs),
						  std::move(a_task));
	}



	uint64_t ThreadPool::PushTaskAfter(IN uint32_t a_afterMs,
									   IN const Task& a_task) asd_noexcept
	{
		return PushTaskAt(Timer::Now() + Timer::Milliseconds(a_afterMs),
						  Task(a_task));
	}



	void ThreadPool::Stop(IN bool a_overtime /*= true*/)
	{
		auto data = std::atomic_exchange(&m_data, std::shared_ptr<ThreadPoolData>());
		if (data == nullptr)
			return;

		auto lock = GetLock(data->tpLock);
		if (data->run == false)
			return;

		if (data->workers.find(GetCurrentThreadID()) != data->workers.end())
			asd_RaiseException("deadlock");

		data->run = false;
		data->overtime = a_overtime;

		while (data->waitingList.size() > 0) {
			data->waitingList.front()->Post();
			data->waitingList.pop_front();
		}

		while (data->threads.size() > 0) {
			auto thread = std::move(*data->threads.rbegin());
			data->threads.resize(data->threads.size() - 1);
			lock.unlock();
			thread.join();
			lock.lock();
		}

		lock.unlock();
	}



	ThreadPool::~ThreadPool() asd_noexcept
	{
		asd_BeginDestructor();
		Stop();
		asd_EndDestructor();
	}

}
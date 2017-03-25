#include "asd_pch.h"
#include "asd/threadpool.h"
#include "asd/util.h"
#include "asd/semaphore.h"
#include "asd/objpool.h"
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
		std::queue<Task_ptr> taskQueue;

		// 동작 상태
		bool run = false;

		// 이 변수가 true이면 
		// 종료명령이 떨어져도 남은 작업을 모두 처리한 후 종료한다.
		bool overtime = true;

		// 작업을 수행하는 쓰레드 관련
		const uint32_t					threadCount;
		std::vector<std::thread>		threads;
		std::unordered_set<uint32_t>	workers;
		std::deque<Semaphore*>			standby;
		std::unique_ptr<Timer>			timer;
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



	inline size_t Poll_Internal(REF std::shared_ptr<ThreadPoolData>& a_data,
								IN uint32_t a_timeoutMs,
								IN size_t a_procCountLimit) asd_noexcept
	{
		if (a_data == nullptr)
			return 0;

		if (a_procCountLimit <= 0)
			return 0;

		thread_local ObjectPool<Semaphore> t_events;
		size_t procCount = 0;

		// 이벤트 체크
		auto lock = GetLock(a_data->tpLock);
		while (a_data->run || (a_data->overtime && !a_data->taskQueue.empty())) {
			if (a_data->taskQueue.empty()) {
				// 대기
				auto event = t_events.Alloc();
				a_data->standby.emplace_back(event);
				lock.unlock();
				bool on = event->Wait(a_timeoutMs);
				t_events.Free(event);
				lock.lock();
				if (on)
					continue;

				// 타임아웃
				auto it = std::find(a_data->standby.begin(), a_data->standby.end(), event);
				if (it != a_data->standby.end())
					a_data->standby.erase(it);
				break;
			}

			// 작업을 접수
			asd_DAssert(a_data->taskQueue.empty() == false);
			Task_ptr task(std::move(a_data->taskQueue.front()));
			a_data->taskQueue.pop();

			// 작업 수행
			lock.unlock();
			task->Execute();
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
		asd_DAssert(data->threads.size() == data->threadCount);
		for (auto& t : data->threads) {
			t = std::thread([data]() mutable
			{
				auto lock = GetLock(data->tpLock);
				data->workers.emplace(GetCurrentThreadID());
				lock.unlock();
				while (data->run) {
					asd::Poll_Internal(data,
									   std::numeric_limits<uint32_t>::max(),
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
		return asd::Poll_Internal(data,
								  a_timeoutMs,
								  a_procCountLimit);
	}



	void PushTask_Internal(REF std::shared_ptr<ThreadPoolData>& a_data,
						   MOVE Task_ptr& a_task) asd_noexcept
	{
		if (a_data == nullptr)
			return;
		if (a_task == nullptr)
			return;

		auto lock = GetLock(a_data->tpLock);
		if (a_data->run == false)
			return;

		a_data->taskQueue.emplace(std::move(a_task));

		if (a_data->standby.size() > 0) {
			a_data->standby.back()->Post();
			a_data->standby.pop_back();
		}
	}



	void ThreadPool::PushTask(MOVE Task_ptr&& a_task) asd_noexcept
	{
		auto data = std::atomic_load(&m_data);
		asd::PushTask_Internal(data, a_task);
	}



	inline Task_ptr PushTimerTask_Internal(REF std::shared_ptr<ThreadPoolData>& a_data,
										   IN Timer::TimePoint a_timePoint,
										   IN Task_ptr a_task) asd_noexcept
	{
		if (a_data == nullptr)
			return nullptr;
		if (a_task == nullptr)
			return nullptr;

		if (a_data->timer == nullptr)
			a_data->timer.reset(new Timer);

		if (a_data->timer->CurrentOffset() > a_timePoint) {
			Task_ptr ret = a_task;
			PushTask_Internal(a_data, a_task);
			return ret;
		}

		return a_data->timer->PushAt(a_timePoint,
									 &asd::PushTask_Internal,
									 std::move(a_data),
									 std::move(a_task));
	}



	Task_ptr ThreadPool::PushTimerTask(IN Timer::TimePoint a_timePoint,
									   IN const Task_ptr& a_task) asd_noexcept
	{
		auto data = std::atomic_load(&m_data);
		return asd::PushTimerTask_Internal(data, a_timePoint, a_task);
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

		while (data->standby.size() > 0) {
			data->standby.front()->Post();
			data->standby.pop_front();
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
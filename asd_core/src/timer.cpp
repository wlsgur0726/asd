#include "asd_pch.h"
#include "asd/timer.h"
#include "asd/classutil.h"
#include <map>


namespace asd
{
	class Scheduler : public Global<Scheduler>
	{
	public:
		Mutex m_lock;
		std::map<
			Timer::TimePoint,
			std::deque<Timer::Task_ptr>> m_taskList;

		Timer::TimePoint	m_now = Timer::Now();
		volatile bool		m_run = true;
		std::thread			m_thread = std::thread([this]() { SchedulerThread(); });

		void SchedulerThread() asd_noexcept
		{
			while (m_run) {
				auto scheduler_lock = GetLock(m_lock);
				for (auto it=m_taskList.begin(); it!=m_taskList.end(); it=m_taskList.erase(it)) {
					if (it->first > m_now)
						break;
					auto& queue = it->second;
					for (auto& task : queue) {
						auto timer = task->m_owner;
						auto timer_lock = GetLock(timer->m_lock);
						timer->m_ready.emplace_back(std::move(task));
						timer->m_event.Post();
					}
				}
				scheduler_lock.unlock();

				m_now += Timer::Milliseconds(1);
				std::this_thread::sleep_until(m_now);
			}
		}

		~Scheduler() asd_noexcept
		{
			m_run = false;
			m_thread.join();
		}
	};

	auto& g_scheduler = Scheduler::GlobalInstance();



	Timer::TimePoint Timer::Now() asd_noexcept
	{
		return std::chrono::high_resolution_clock::now();
	}


	uint64_t Timer::PushAt(IN TimePoint a_timepoint,
						   MOVE Callback&& a_task) asd_noexcept
	{
		// 새로운 Task 객체 얻기
		auto timer_lock = GetLock(m_lock);
		Task_ptr task;
		if (m_pool.empty())
			task.reset(new Task);
		else {
			task = std::move(*m_pool.rbegin());
			m_pool.pop_back();
		}

		// 새로운 핸들 발급
		uint64_t newHandle;
		do {
			newHandle = ++m_lastHandle;
		} while (0==newHandle || false==m_handles.emplace(newHandle, false).second);
		timer_lock.unlock();

		// 스케쥴러에 등록
		task->m_handle = newHandle;
		task->m_owner = this;
		task->m_callback = std::move(a_task);
		auto scheduler_lock = GetLock(g_scheduler.m_lock);
		g_scheduler.m_taskList[a_timepoint].emplace_back(std::move(task));
		return newHandle;
	}



	uint64_t Timer::PushAfter(IN uint32_t a_afterMs,
							  MOVE Callback&& a_task) asd_noexcept
	{
		auto now = std::chrono::high_resolution_clock::now();
		return PushAt(now + Milliseconds(a_afterMs),
					  std::move(a_task));
	}



	size_t Timer::Poll(IN uint32_t a_waitTimeoutMs /*= 0*/,
					   IN size_t a_limit /*= std::numeric_limits<size_t>::max()*/) asd_noexcept
	{
		size_t count;
		for (count=0; count<a_limit; ++count) {
			// 이벤트 대기
			try {
				if (m_event.Wait(0) == false) {
					if (count>0 || m_event.Wait(a_waitTimeoutMs) == false)
						break; // 한건이라도 수행했거나 타임아웃인 경우
				}
			}
			catch (asd::Exception& e) {
				asd_Assert(false, "fail m_event.Wait({}), {}", a_waitTimeoutMs, e.what());
				break;
			}

			// 큐의 맨 앞의것을 꺼내기
			auto timer_lock = GetLock(m_lock);
			asd_Assert(m_ready.size() > 0, "unknown logic error");
			auto task = std::move(m_ready.front());
			m_ready.pop_front();
			auto callback = std::move(task->m_callback);

			// 핸들을 해제하면서 취소된 이벤트인지 체크
			auto it = m_handles.find(task->m_handle);
			asd_Assert(it != m_handles.end(), "unknown logic error");
			bool cancel = it->second;
			m_handles.erase(it);

			// task 객체를 풀에 반납
			if (m_pool.size() < 1000)
				m_pool.emplace_back(std::move(task));

			// 콜백
			timer_lock.unlock();
			if (cancel == false)
				callback();
		}
		return count;
	}



	bool Timer::Cancel(IN uint64_t a_handle) asd_noexcept
	{
		auto timer_lock = GetLock(m_lock);
		auto it = m_handles.find(a_handle);
		if (it == m_handles.end())
			return false;
		it->second = true;
		return true;
	}



	Timer::~Timer() asd_noexcept
	{
		auto scheduler_lock = GetLock(g_scheduler.m_lock);
		for (auto& it : g_scheduler.m_taskList) {
			auto& queue = it.second;
			for (auto task_iter=queue.begin(); task_iter!=queue.end(); ) {
				auto& task = *task_iter;
				if (task->m_owner == this)
					task_iter = queue.erase(task_iter);
				else
					++task_iter;
			}
		}
	}


}

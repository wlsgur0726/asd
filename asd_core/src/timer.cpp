#include "stdafx.h"
#include "asd/timer.h"

#if !asd_Platform_Windows
#include <pthread.h>

#endif

namespace asd
{
#if asd_Platform_Windows
	void BeginGlobalTimerThread()
	{
		HANDLE curThread = ::GetCurrentThread();
		::SetThreadPriority(curThread, THREAD_PRIORITY_TIME_CRITICAL);
		//::timeBeginPeriod(1);
	}

#else
	void BeginGlobalTimerThread()
	{
		sched_param p;
		p.sched_priority = 1;
		::pthread_setschedparam(pthread_self(), SCHED_RR, &p);
	}

#endif


	Timer::TimePoint Timer::Now()
	{
		return std::chrono::high_resolution_clock::now();
	}


	Timer::Millisec Timer::Diff(IN TimePoint a_before,
								IN TimePoint a_after)
	{
		return std::chrono::duration_cast<Millisec>(a_after - a_before);
	}


	Timer::Timer()
	{
		m_thread = std::thread([this]()
		{
			if (this == &Global<Timer>::Instance())
				BeginGlobalTimerThread();

			m_offset = Now();
			PollLoop();
		});
	}


	Timer::TimePoint Timer::CurrentOffset()
	{
		return m_offset - Millisec(1);
	}


	void Timer::PollLoop()
	{
		std::vector<std::deque<Task_ptr>> taskList;
		taskList.reserve(100);
		for (bool run;;) {
			for (auto lock=GetLock(m_lock); (run=m_run); lock.lock()) {
				for (auto it=m_taskList.begin(); it!=m_taskList.end(); ) {
					if (it->first > m_offset)
						break;
					taskList.emplace_back(std::move(it->second));
					it = m_taskList.erase(it);
				}
				lock.unlock();

				if (taskList.empty())
					break;

				for (auto& queue : taskList) {
					for (auto& task : queue) {
						asd_BeginTry();
						task->Execute();
						asd_EndTryUnknown_Default();
					}
				}

				taskList.clear();
			}

			if (!run)
				break;

			m_offset += Millisec(1);
			std::this_thread::sleep_until(m_offset);
		}
	}


	void Timer::PushTask(IN TimePoint a_timepoint,
						 IN const Task_ptr& a_task)
	{
		if (a_task == nullptr)
			return;

		auto lock = GetLock(m_lock);
		m_taskList[a_timepoint].emplace_back(a_task);
	}


	Timer::~Timer()
	{
		asd_BeginDestructor();

		auto lock = GetLock(m_lock);
		m_run = false;
		lock.unlock();

		m_thread.join();

		asd_EndDestructor();
	}

}

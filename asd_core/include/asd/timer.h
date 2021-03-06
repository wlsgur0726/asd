﻿#pragma once
#include "asdbase.h"
#include "task.h"
#include <functional>
#include <chrono>
#include <map>
#include <deque>


namespace asd
{
	// 원하는 시각에 수행시킬 task를 등록한다.
	// 등록한 task는 전용 쓰레드에서 실행된다.
	// 병목이 발생하기 쉬우므로 시그널발생, 다른 task큐잉 등
	// 부하가 적은 알람 계열 작업만 넣어야 한다.
	class Timer : public Global<Timer>
	{
	public:
		using Millisec	= std::chrono::milliseconds;
		using TimePoint	= std::chrono::high_resolution_clock::time_point;

		// 시차 계산
		static Millisec Diff(TimePoint a_before,
							 TimePoint a_after);

		// 현재시간
		static TimePoint Now();


		Timer();

		// 어디까지 실행했는지
		TimePoint CurrentOffset();

		// a_timepoint 시점에 수행할 task를 큐잉
		// 큐잉된 task 리턴 (nullptr이면 실패)
		template <typename FUNC, typename... PARAMS>
		inline Task_ptr Push(TimePoint a_timepoint,
							 FUNC&& a_func,
							 PARAMS&&... a_params)
		{
			auto task = CreateTask(std::forward<FUNC>(a_func),
								   std::forward<PARAMS>(a_params)...);
			PushTask(a_timepoint, task);
			return task;
		}

		// a_afterMs 후에 수행할 task를 큐잉
		// 큐잉된 task 리턴 (nullptr이면 실패)
		template <typename DURATION, typename FUNC, typename... PARAMS>
		inline Task_ptr Push(DURATION a_after,
							 FUNC&& a_func,
							 PARAMS&&... a_params)
		{
			auto task = CreateTask(std::forward<FUNC>(a_func),
								   std::forward<PARAMS>(a_params)...);
			PushTask(Now() + a_after, task);
			return task;
		}

		virtual ~Timer();

	private:
		void PushTask(TimePoint a_timepoint,
					  const Task_ptr& a_task);

		void PollLoop();

		Mutex m_lock;
		bool m_run = true;
		std::map<TimePoint, std::deque<Task_ptr>> m_taskList;
		TimePoint m_offset;
		std::thread m_thread;
	};
}

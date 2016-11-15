#pragma once
#include "asdbase.h"
#include <functional>
#include <chrono>


namespace asd
{
	// 원하는 시각에 수행시킬 task를 등록한다.
	// 등록한 task는 전용 쓰레드에서 실행된다.
	// 병목이 발생하기 쉬우므로 시그널발생, 다른 task큐잉 등
	// 부하가 적은 알람 계열 작업만 넣어야 한다.
	namespace Timer
	{
		typedef std::function<void()>							Callback;
		typedef std::chrono::milliseconds						Milliseconds;
		typedef std::chrono::high_resolution_clock::time_point	TimePoint;

		// 현재시간
		TimePoint Now() asd_noexcept;

		// 어디까지 실행했는지
		TimePoint CurrentOffset() asd_noexcept;

		// a_timepoint 시점에 수행할 task를 큐잉
		// 핸들 리턴 (0이면 실패)
		uint64_t PushAt(IN TimePoint a_timepoint,
						MOVE Callback&& a_task) asd_noexcept;

		// a_ms 후에 수행할 task를 큐잉
		inline uint64_t PushAfter(IN uint32_t a_afterMs,
								  MOVE Callback&& a_task) asd_noexcept
		{
			return PushAt(Now() + Milliseconds(a_afterMs),
						  std::move(a_task));
		}

		// 아직 실행되지 않은 task를 취소
		// 아직 실행되지 않은 유효한 핸들이면 true 리턴
		bool Cancel(IN uint64_t a_handle) asd_noexcept;
	}
}

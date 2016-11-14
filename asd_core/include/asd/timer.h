#pragma once
#include "asdbase.h"
#include "lock.h"
#include "semaphore.h"
#include <functional>
#include <chrono>
#include <deque>
#include <unordered_map>


namespace asd
{
	class Scheduler;

	class Timer
	{
	public:
		typedef std::function<void()>							Callback;
		typedef std::chrono::milliseconds						Milliseconds;
		typedef std::chrono::high_resolution_clock::time_point	TimePoint;

		// 현재시간
		static TimePoint Now() asd_noexcept;

		// a_timepoint 시점에 수행할 이벤트를 큐잉
		// 핸들 리턴 (0이면 실패)
		uint64_t PushAt(IN TimePoint a_timepoint,
						MOVE Callback&& a_task) asd_noexcept;

		// a_ms 후에 수행할 이벤트를 큐잉
		// 핸들 리턴 (0이면 실패)
		uint64_t PushAfter(IN uint32_t a_afterMs,
						   MOVE Callback&& a_task) asd_noexcept;

		// 시간이 된 이벤트가 하나도 없으면 a_waitTimeoutMs 밀리초 동안 대기
		// 이벤트가 하나라도 있으면 최대 a_limit 개수만큼 순서대로 수행하고 바로 리턴
		// 리턴값은 수행한 이벤트 개수
		size_t Poll(IN uint32_t a_waitTimeoutMs = 0,
					IN size_t a_limit = std::numeric_limits<size_t>::max()) asd_noexcept;

		// 아직 실행되지 않은 이벤트를 취소
		// 유효한 핸들이면 true 리턴
		bool Cancel(IN uint64_t a_handle) asd_noexcept;

		// 이 객체가 삭제될 때
		// 아직 실행되지 않은 모든 이벤트들은 취소됩니다.
		virtual ~Timer() asd_noexcept;


	private:
		friend class asd::Scheduler;
		struct Task
		{
			Timer*		m_owner;
			uint64_t	m_handle;
			Callback	m_callback;
		};
		typedef std::unique_ptr<Task> Task_ptr;
		typedef bool Cancel_t;

		Mutex m_lock;
		uint64_t m_lastHandle = 0;
		Semaphore m_event;
		std::deque<Task_ptr> m_pool;
		std::deque<Task_ptr> m_ready;
		std::unordered_map<uint64_t, Cancel_t> m_handles;

	};
}

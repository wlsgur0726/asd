#pragma once
#include "asdbase.h"
#include "classutil.h"
#include "sysutil.h"
#include "threadutil.h"


namespace asd
{
	template <typename T1, typename T2>
	constexpr T1 max(const T1& a_1,
					 const T2& a_2)
	{
		return a_1 >= a_2 ? a_1 : (T1)a_2;
	}
	template <typename T, typename... ARGS>
	constexpr T max(const T& a_1,
					const ARGS&... a_args)
	{
		return max(a_1, max(a_args...));
	}

	template <typename T1, typename T2>
	constexpr T1 min(const T1& a_1,
					 const T2& a_2)
	{
		return a_1 < a_2 ? a_1 : (T1)a_2;
	}
	template <typename T, typename... ARGS>
	constexpr T min(const T& a_1,
					const ARGS&... a_args)
	{
		return min(a_1, min(a_args...));
	}


	template <typename Task>
	struct _FinallyTask
	{
		bool m_call = false;
		const Task m_task;

		_FinallyTask(Task&& a_task)
			: m_task(std::move(a_task))
		{
			m_call = true;
		}

		_FinallyTask(_FinallyTask&& a_task)
			: m_task(std::move(a_task.m_task))
		{
			m_call = true;
			a_task.m_call = false;
		}

		~_FinallyTask()
		{
			if (m_call)
				m_task();
		}
	};

	template <typename Task>
	inline _FinallyTask<Task> FinallyTask(Task&& a_task)
	{
		return _FinallyTask<Task>(std::move(a_task));
	}


	const std::vector<uint16_t>& GetPrimeNumbers();

	inline uint16_t FindNearPrimeNumber(uint16_t a_val)
	{
		auto& list = asd::GetPrimeNumbers();
		auto it = std::lower_bound(list.begin(), list.end(), a_val);
		if (it == list.end())
			return *list.rbegin();
		return *it;
	}
}

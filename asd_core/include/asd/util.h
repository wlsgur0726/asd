﻿#pragma once
#include "asdbase.h"
#include "classutil.h"
#include "sysutil.h"
#include "threadutil.h"


namespace asd
{
	template <typename T>
	constexpr const T& max(IN const T& a_1,
						   IN const T& a_2)
	{
		return a_1 >= a_2 ? a_1 : a_2;
	}
	template <typename T, typename... ARGS>
	constexpr const T& max(IN const T& a_1,
						   IN const ARGS&... a_args)
	{
		return max(a_1, max(a_args...));
	}

	template <typename T>
	constexpr const T& min(IN const T& a_1,
						   IN const T& a_2)
	{
		return a_1 < a_2 ? a_1 : a_2;
	}
	template <typename T, typename... ARGS>
	constexpr const T& min(IN const T& a_1,
						   IN const ARGS&... a_args)
	{
		return min(a_1, min(a_args...));
	}


	template <typename Task>
	struct _FinallyTask
	{
		bool m_call = false;
		const Task m_task;

		_FinallyTask(MOVE Task&& a_task) asd_noexcept
			: m_task(std::move(a_task))
		{
			m_call = true;
		}

		_FinallyTask(MOVE _FinallyTask&& a_task) asd_noexcept
			: m_task(std::move(a_task.m_task))
		{
			m_call = true;
			a_task.m_call = false;
		}

		~_FinallyTask() asd_noexcept
		{
			if (m_call)
				m_task();
		}
	};

	template <typename Task>
	inline _FinallyTask<Task> FinallyTask(MOVE Task&& a_task) asd_noexcept
	{
		return _FinallyTask<Task>(std::move(a_task));
	}

#define asd_RegisterFinallyTask(task) const auto ___asd_finally___ = asd::FinallyTask(task);



}
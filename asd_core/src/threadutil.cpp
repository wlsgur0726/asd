#include "stdafx.h"
#include "asd/threadutil.h"
#include "asd/lock.h"
#include <atomic>
#include <cstdlib>
#include <ctime>
#include <unordered_map>
#include <mutex>


#if !asd_Platform_Windows
#	include <sys/types.h>
#	include <sys/syscall.h>
#	include <unistd.h>
#	include <pthread.h>
#
#endif

namespace asd
{
	struct ThreadManager
	{
		Mutex m_lock;
		std::unordered_map<uint32_t, uint32_t> m_map;

		void Register()
		{
			uint32_t seq = GetCurrentThreadSequence();
			uint32_t tid = GetCurrentThreadID();
			auto lock = GetLock(m_lock);
			m_map[seq] = tid;
		}

		size_t Count()
		{
			auto lock = GetLock(m_lock);
			return m_map.size();
		}

		void Unregister()
		{
			uint32_t seq = GetCurrentThreadSequence();
			auto lock = GetLock(m_lock);
			m_map.erase(seq);
		}

	} g_threadManager;

	struct ThreadInit
	{
		ThreadInit()	{ g_threadManager.Register(); }
		~ThreadInit()	{ g_threadManager.Unregister(); }
	};
	thread_local ThreadInit t_threadInit;



	uint32_t GetCurrentThreadID()
	{
		thread_local uint32_t t_tid = 0;
		if (t_tid == 0) {
#if asd_Platform_Windows
			t_tid = ::GetCurrentThreadId();
#else
			t_tid = ::syscall(SYS_gettid);
#endif
		}
		return t_tid;
	}



	uint32_t GetCurrentThreadSequence()
	{
		static std::atomic<uint32_t> g_sequence;
		thread_local uint32_t t_sequence = g_sequence++;
		return t_sequence;
	}



	uint32_t Get_HW_Concurrency()
	{
		static uint32_t s_HW_Concurrency;
		static std::once_flag s_init;
		std::call_once(s_init, []()
		{
			s_HW_Concurrency = std::thread::hardware_concurrency();
			if (s_HW_Concurrency == 0)
				s_HW_Concurrency = 1;
		});
		return s_HW_Concurrency;
	}



	void KillThread(uint32_t a_threadSequence)
	{
		auto lock = GetLock(g_threadManager.m_lock);
		auto it = g_threadManager.m_map.find(a_threadSequence);
		if (it == g_threadManager.m_map.end())
			return;

#if asd_Platform_Windows
		auto handle = ::OpenThread(THREAD_TERMINATE, FALSE, it->second);
		if (handle == NULL)
			return;
		::TerminateThread(handle, 1);
		::CloseHandle(handle);

#else
		::pthread_cancel(it->second);

#endif
	}



	size_t GetAliveThreadCount()
	{
		return g_threadManager.Count();
	}
}
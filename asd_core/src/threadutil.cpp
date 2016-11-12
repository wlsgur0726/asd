#include "asd_pch.h"
#include "asd/threadutil.h"
#include "asd/lock.h"
#include <atomic>
#include <cstdlib>
#include <ctime>
#include <unordered_map>

#if !asd_Platform_Windows
#	include <sys/types.h>
#	include <sys/syscall.h>
#	include <unistd.h>
#	include <pthread.h>
#
#endif

namespace asd
{
#if asd_Platform_Windows
	typedef uint32_t	tid_t;
#else
	typedef pthread_t	tid_t;
#endif

	struct ThreadManager
	{
		Mutex m_lock;
		std::unordered_map<uint32_t, tid_t> m_map;

		void Register() asd_noexcept
		{
			uint32_t seq = GetCurrentThreadSequence();
#if asd_Platform_Windows
			tid_t tid = GetCurrentThreadID();
#else
			tid_t tid = ::pthread_self();
#endif
			auto lock = GetLock(m_lock);
			m_map[seq] = tid;
		}

		void Unregister() asd_noexcept
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



	uint32_t GetCurrentThreadID() asd_noexcept
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



	uint32_t GetCurrentThreadSequence() asd_noexcept
	{
		static std::atomic<uint32_t> g_sequence;
		thread_local uint32_t t_sequence = g_sequence++;
		return t_sequence;
	}



	uint32_t Get_HW_Concurrency() asd_noexcept
	{
		static uint32_t t_HW_Concurrency = std::thread::hardware_concurrency();
		return t_HW_Concurrency;
	}



	void KillThread(IN uint32_t a_threadSequence) asd_noexcept
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



	void srand() asd_noexcept
	{
		thread_local int t_seed;
		intptr_t a = (intptr_t)&t_seed;
		time_t b = time(nullptr);
		t_seed = (int)(a^b);
		if (GetCurrentThreadSequence() % 2 == 0)
			t_seed = t_seed;
		std::srand(t_seed);
	}
}
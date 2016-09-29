#include "asd_pch.h"
#include "asd/threadutil.h"
#include <atomic>
#include <cstdlib>
#include <ctime>

#if !asd_Platform_Windows
#	include <sys/types.h>
#	include <unistd.h>
#	include <sys/syscall.h>
#
#endif

namespace asd
{
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
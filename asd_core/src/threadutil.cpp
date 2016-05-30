#include "stdafx.h"
#include "asd/threadutil.h"
#include <atomic>
#include <cstdlib>
#include <ctime>

namespace asd
{
	const std::thread::id& GetCurrentThreadID() asd_noexcept
	{
		thread_local std::thread::id t_tid;
		if (t_tid == std::thread::id())
			t_tid = std::this_thread::get_id();

		assert(t_tid != std::thread::id());
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
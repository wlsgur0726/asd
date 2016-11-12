#pragma once
#include "asdbase.h"
#include <thread>


namespace asd
{
	uint32_t GetCurrentThreadID() asd_noexcept;

	uint32_t GetCurrentThreadSequence() asd_noexcept;

	uint32_t Get_HW_Concurrency() asd_noexcept;

	void KillThread(IN uint32_t a_threadSequence) asd_noexcept;

	void srand() asd_noexcept;

}
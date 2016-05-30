#pragma once
#include "asd/asdbase.h"
#include <thread>


namespace asd
{
	const std::thread::id& GetCurrentThreadID() asd_noexcept;

	uint32_t GetCurrentThreadSequence() asd_noexcept;

	uint32_t Get_HW_Concurrency() asd_noexcept;

	void srand() asd_noexcept;

}
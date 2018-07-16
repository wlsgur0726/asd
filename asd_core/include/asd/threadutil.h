#pragma once
#include "asdbase.h"
#include <thread>


namespace asd
{
	uint32_t GetCurrentThreadID();

	uint32_t GetCurrentThreadSequence();

	uint32_t Get_HW_Concurrency();

	size_t GetAliveThreadCount();

	void KillThread(IN uint32_t a_threadSequence);

}
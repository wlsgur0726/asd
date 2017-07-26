#pragma once
#include "asdbase.h"
#include "timer.h"

namespace asd
{
	// 1.0 = 100%
	double CpuUsage() asd_noexcept;
	Timer::Millisec GetCpuUsageCheckCycle() asd_noexcept;
	Timer::Millisec SetCpuUsageCheckCycle(IN Timer::Millisec a_cycle) asd_noexcept;
}

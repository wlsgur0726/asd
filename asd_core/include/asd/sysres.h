#pragma once
#include "asdbase.h"
#include "timer.h"

namespace asd
{
	// 1.0 = 100%
	double CpuUsage();
	Timer::Millisec GetCpuUsageCheckCycle();
	Timer::Millisec SetCpuUsageCheckCycle(IN Timer::Millisec a_cycle);
}

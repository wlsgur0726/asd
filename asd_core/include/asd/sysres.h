#pragma once
#include "asdbase.h"
#include "timer.h"

namespace asd
{
	// 1.0 = 100%
	double CpuUsage();
	Timer::Millisec GetCpuUsageCheckInterval();
	Timer::Millisec SetCpuUsageCheckInterval(Timer::Millisec a_cycle);
}

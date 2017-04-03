#include "stdafx.h"
#include "test.h"
#include "asd/sysres.h"
#include <conio.h>
#include <thread>

void Test()
{
	return;
	volatile bool run = true;
	std::thread printer([&]()
	{
		while (run) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			double cpu = asd::CpuUsage();
			printf("%lf\n", cpu);
		}
	});

	std::vector<std::thread> workers;
	for (int i=0; i<2; ++i) {
		workers.emplace_back(std::thread([&]()
		{
			uint64_t cnt = 0;
			while (run) {
				++cnt;
			}
			printf("cnt:%llu\n", cnt);
		}));
	}

	for (int c=0; c!='q'; c=_getch());
	run = false;
	printer.join();
	for (auto& t : workers)
		t.join();
	exit(0);
}

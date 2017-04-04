#include "stdafx.h"
#include "asd/string.h"
#include "asd/timer.h"
#include "asd/lock.h"
#include <atomic>
#include <map>


namespace asdtest_timer
{
	inline int64_t Tick(asd::Timer::TimePoint now = asd::Timer::Now())
	{
		const asd::Timer::TimePoint tp;
		auto e = now - tp;
		return std::chrono::duration_cast<std::chrono::milliseconds>(e).count();
	}


	TEST(Timer, Event)
	{
		auto& globalTimer = asd::Global<asd::Timer>::Instance();

		const int TestTimeMs = 1000;

		std::map<int64_t, std::vector<int>> eventHistory;
		std::set<int> expect_events;
		std::vector<asd::Task_ptr> cancel;
		cancel.reserve(TestTimeMs);

		const auto Start = asd::Timer::Now();

		for (int i=TestTimeMs; i>10; --i) {
			auto task = globalTimer.PushAfter(i, [i, &eventHistory]()
			{
				eventHistory[Tick()].emplace_back(i);
			});
			ASSERT_NE(task, nullptr);
			if (i % 2 == 0)
				cancel.emplace_back(task);
			else
				expect_events.emplace(i);
		}

		for (auto task : cancel)
			task->Cancel();

		globalTimer.PushAt(Start - asd::Timer::Millisec(1), [&eventHistory]()
		{
			eventHistory[Tick()].emplace_back(-1);
		});

		const auto Wait = asd::Timer::Now() + asd::Timer::Millisec(TestTimeMs);
		while (Wait >= globalTimer.CurrentOffset())
			std::this_thread::sleep_for(std::chrono::milliseconds(10));

		const int64_t Tolerance = 2;
		bool first = true;
		for (auto& it : eventHistory) {
#if !asd_Debug
			EXPECT_GE(Tolerance, it.second.size());
#endif
			for (int value : it.second) {
				auto term = it.first - Tick(Start);
				if (first) {
					first = false;
					EXPECT_EQ(-1, value);
#if !asd_Debug
					EXPECT_GE(Tolerance, term);
#endif
					continue;
				}

				ASSERT_GT(expect_events.size(), 0);
				int expect = *expect_events.begin();
				expect_events.erase(expect);
#if !asd_Debug
				EXPECT_EQ(expect, value);
				auto diff = term - expect;
				// 0 <= diff <= Tolerance
				EXPECT_LE(0, diff);
				EXPECT_GE(Tolerance, diff);
#endif
			}
		}
		EXPECT_EQ(expect_events.size(), 0);
	}
}

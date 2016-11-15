#include "stdafx.h"
#include "asd/string.h"
#include "asd/timer.h"
#include "asd/lock.h"
#include <atomic>
#include <map>


namespace asdtest_testtemplate
{
	inline int64_t Tick(asd::Timer::TimePoint now = asd::Timer::Now())
	{
		const asd::Timer::TimePoint tp;
		auto e = now - tp;
		return std::chrono::duration_cast<std::chrono::milliseconds>(e).count();
	}


	TEST(Timer, Event)
	{
		const int TestTimeMs = 1000;

		std::map<int64_t, std::vector<int>> eventHistory;
		std::set<int> expect_events;
		std::vector<uint64_t> cancel;
		cancel.reserve(TestTimeMs);

		const auto Start = asd::Timer::Now();

		for (int i=TestTimeMs; i>10; --i) {
			uint64_t handle = asd::Timer::PushAfter(i, [i, &eventHistory]()
			{
				eventHistory[Tick()].push_back(i);
			});
			ASSERT_NE(handle, 0);
			if (i % 2 == 0)
				cancel.push_back(handle);
			else
				expect_events.emplace(i);
		}

		for (auto handle : cancel)
			EXPECT_TRUE(asd::Timer::Cancel(handle));

		asd::Timer::PushAt(Start - asd::Timer::Milliseconds(1), [&eventHistory]()
		{
			eventHistory[Tick()].push_back(-1);
		});

		const auto Wait = asd::Timer::Now() + asd::Timer::Milliseconds(TestTimeMs);
		while (Wait >= asd::Timer::CurrentOffset())
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
				// 0 <= diff <= 2
				EXPECT_LE(0, diff);
				EXPECT_GE(Tolerance, diff);
#endif
			}
		}
		EXPECT_EQ(expect_events.size(), 0);
	}
}

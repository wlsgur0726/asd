#include "stdafx.h"
#include "asd/string.h"
#include "asd/timer.h"
#include <atomic>
#include <map>


namespace asdtest_testtemplate
{
	typedef std::map<
		int64_t,
		std::vector<int>
	> History;

	asd::Mutex g_lock;
	History g_eventHistory;

	struct EventHistory : public History
	{
		virtual ~EventHistory()
		{
			auto lock = asd::GetLock(g_lock);
			for (auto& it : *this) {
				for (int value : it.second)
					g_eventHistory[it.first].push_back(value);
			}
		}
	};
	thread_local EventHistory t_eventHistory;


	inline int64_t Tick(asd::Timer::TimePoint now = asd::Timer::Now())
	{
		const asd::Timer::TimePoint tp;
		auto e = now - tp;
		return std::chrono::duration_cast<std::chrono::milliseconds>(e).count();
	}


	TEST(Timer, Event)
	{
		asd::Timer timer;
		std::atomic<bool> run;

		run = true;
		std::vector<std::thread> threads;
		for (auto t=std::thread::hardware_concurrency(); t>0; --t) {
			threads.emplace_back(std::thread([&]()
			{
				while (run) {
					timer.Poll(1);
				}
			}));
		}

		const int TestTimeMs = 1000;
		std::set<int> expect_events;
		std::vector<uint64_t> cancel;
		cancel.reserve(TestTimeMs);

		const auto Start = asd::Timer::Now();

		for (int i=TestTimeMs; i>10; --i) {
			uint64_t handle = timer.PushAfter(i, [i]()
			{
				t_eventHistory[Tick()].push_back(i);
			});
			ASSERT_NE(handle, 0);
			if (i % 2 == 0)
				cancel.push_back(handle);
			else
				expect_events.emplace(i);
		}

		for (auto handle : cancel)
			EXPECT_TRUE(timer.Cancel(handle));

		timer.PushAt(Start - asd::Timer::Milliseconds(1), []()
		{
			t_eventHistory[Tick()].push_back(-1);
		});

		std::this_thread::sleep_until(Start + std::chrono::milliseconds(TestTimeMs*2));

		run = false;
		for (auto& t : threads)
			t.join();

		const int64_t Tolerance = 2;
		auto lock = asd::GetLock(g_lock);
		bool first = true;
		for (auto& it : g_eventHistory) {
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

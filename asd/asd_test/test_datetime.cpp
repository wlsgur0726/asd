#include "stdafx.h"
#include "asd/datetime.h"
#include <thread>

namespace asdtest_datetime
{
	TEST(DateTime, Compare)
	{
		const tm T1 = *asd::localtime();
		asd::Date d1 = T1;
		asd::Time t1 = T1;
		asd::DateTime dt1 = T1;
		EXPECT_GT(d1, asd::Date());
		EXPECT_GT(t1, asd::Time());
		EXPECT_GT(dt1, asd::DateTime());

		std::this_thread::sleep_for(std::chrono::seconds(1));

		const tm T2 = *asd::localtime();
		asd::Time t2 = T2;
		asd::DateTime dt2 = T2;
		EXPECT_GT(t2, t1);
		EXPECT_GT(dt2, dt1);
	}


	TEST(DateTime, DayOfTheWeek)
	{
		tm t = *asd::localtime();
		asd::Date d = t;
		EXPECT_EQ(t.tm_wday, (int)d.DayOfTheWeek());
		EXPECT_EQ(asd::Date(1, 1, 1).DayOfTheWeek(), asd::DayOfTheWeek::Monday);
	}

}

#include "stdafx.h"
#include "asd/datetime.h"
#include <thread>
#include <cstring>

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


	template <typename asdType, typename ConvType>
	void ConvertTest()
	{
		asdType asdt1 = asd::DateTime::Now();
		ConvType conv1 = asdt1;
		asdType asdt2 = conv1;
		ConvType conv2 = asdt2;
		EXPECT_EQ(0, memcmp(&asdt1, &asdt2, sizeof(asdType)));
		EXPECT_EQ(0, memcmp(&conv1, &conv2, sizeof(ConvType)));
	}

	TEST(DateTime, ConvertTest_time_t)
	{
		ConvertTest<asd::Date, time_t>();
		ConvertTest<asd::Time, time_t>();
		ConvertTest<asd::DateTime, time_t>();
	}

	TEST(DateTime, ConvertTest_tm)
	{
		ConvertTest<asd::Date, tm>();
		ConvertTest<asd::Time, tm>();
		ConvertTest<asd::DateTime, tm>();
	}

	TEST(DateTime, ConvertTest_std_chrono_system_clock)
	{
		ConvertTest<asd::Date, std::chrono::system_clock::time_point>();
		ConvertTest<asd::Time, std::chrono::system_clock::time_point>();
		ConvertTest<asd::DateTime, std::chrono::system_clock::time_point>();
	}

	TEST(DateTime, ConvertTest_SQL_TIMESTAMP_STRUCT)
	{
		ConvertTest<asd::Date, SQL_TIMESTAMP_STRUCT>();
		ConvertTest<asd::Time, SQL_TIMESTAMP_STRUCT>();
		ConvertTest<asd::DateTime, SQL_TIMESTAMP_STRUCT>();
	}
}

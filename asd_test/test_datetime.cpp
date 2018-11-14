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
		EXPECT_EQ(t.tm_wday, (int)d.GetDayOfTheWeek());
		EXPECT_EQ(asd::Date(1, 1, 1).GetDayOfTheWeek(), asd::DayOfTheWeek::Monday);
	}

	template <typename T>
	bool Equal(const T& a, const T& b) { return a == b; }

	template <>
	bool Equal<tm>(const tm& a, const tm& b)
	{
		return a.tm_year == b.tm_year
			&& a.tm_mon == b.tm_mon
			&& a.tm_yday == b.tm_yday
			&& a.tm_mday == b.tm_mday
			&& a.tm_wday == b.tm_wday
			&& a.tm_hour == b.tm_hour
			&& a.tm_min == b.tm_min
			&& a.tm_sec == b.tm_sec
			&& a.tm_isdst == b.tm_isdst;
	}

	template <>
	bool Equal<asd::SQL_TIMESTAMP_STRUCT>(const asd::SQL_TIMESTAMP_STRUCT& a, const asd::SQL_TIMESTAMP_STRUCT& b)
	{
		return a.year == b.year
			&& a.month == b.month
			&& a.day == b.day
			&& a.hour == b.hour
			&& a.minute == b.minute
			&& a.second == b.second
			&& a.fraction == b.fraction;
	}

	template <typename asdType, typename ConvType>
	void ConvertTest()
	{
		asdType asdt1 = asd::DateTime::Now();
		ConvType conv1 = asdt1;
		asdType asdt2 = conv1;
		ConvType conv2 = asdt2;

		EXPECT_TRUE(Equal(asdt1, asdt2));
		EXPECT_TRUE(Equal(conv1, conv2));
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
		ConvertTest<asd::Date, asd::SQL_TIMESTAMP_STRUCT>();
		ConvertTest<asd::Time, asd::SQL_TIMESTAMP_STRUCT>();
		ConvertTest<asd::DateTime, asd::SQL_TIMESTAMP_STRUCT>();
	}
}

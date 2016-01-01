#include "stdafx.h"
#include "asd/datetime.h"

namespace asd
{
	tm* localtime(IN const time_t* a_time) noexcept
	{
		thread_local tm t_tm;
#if asd_Platform_Windows
		if (localtime_s(&t_tm, a_time) == 0)
			return &t_tm;
		else
			return nullptr;
#else
		return localtime_r(a_time, &t_tm);
#endif
	}


	tm* gmtime(IN const time_t* a_time) noexcept
	{
		thread_local tm t_tm;
#if asd_Platform_Windows
		if (gmtime_s(&t_tm, a_time) == 0)
			return &t_tm;
		else
			return nullptr;
#else
		return gmtime_r(a_time, &t_tm);
#endif
	}



	//                        1   2   3   4   5   6   7   8   9   10  11  12
	const int Days[13] = {-1, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};


	// 1년 1월 1일로부터 a_year년 1월 1일 사이의 윤일 개수를 구한다.
	inline int GetLeapDayCount(IN int a_year)
	{
		assert(a_year != 0);

		int y;
		if (a_year > 0)
			y = a_year - 1;
		else
			y = a_year - 3;

		const int a = y / 4;
		const int b = y / 100;
		const int c = y / 400;
		return a - (b - c);
	}


	// a_year1년 1월 1일과 a_year2년 1월 1일 사이의 윤일 개수를 구한다.
	inline int GetLeapDayCount(IN int a_year1,
							   IN int a_year2)
	{
		if (a_year1 == a_year2)
			return 0;

		if (a_year1 > a_year2)
			std::swap(a_year1, a_year2);

		assert(a_year2 > a_year1);
		return GetLeapDayCount(a_year2) - GetLeapDayCount(a_year1);
	}


	// 윤년인지 여부
	inline bool IsLeapYear(IN int a_year)
	{
		assert(a_year != 0);
		if (a_year < 0)
			a_year -= 3;
		
		if (a_year % 4 != 0)
			return false;
		if (a_year % 100 == 0) {
			if (a_year % 400 != 0)
				return false;
		}
		return true;
	}


	// a_month월의 마지막 일을 구한다.
	inline int GetLastDay(IN int a_month,
						  IN bool a_isLeapYear)
	{
		assert(a_month>=1 && a_month<=12);
		int r = Days[a_month];
		if (a_isLeapYear && a_month==2) {
			assert(r + 1 == 29);
			return r + 1;
		}
		return r;
	}


	// 1월 1일부터 a_month월 a_day일 까지 지난 일 수를 구한다.
	inline int GetDayOffset(IN int a_month,
							IN int a_day,
							IN bool a_isLeapYear)
	{
		assert(a_month>=1 && a_month<=12);
		assert(a_day>=1 && a_day<=GetLastDay(a_month, a_isLeapYear));

		int r = 0;
		const int m = a_month - 1;
		for (int i=1; i<=m; ++i)
			r += GetLastDay(i, a_isLeapYear);
		r += a_day - 1;
		assert(r >= 0);
		return r;
	}


	// 1년 1월 1일로부터 a_year년 a_month월 a_day일 까지 경과한 일 수를 구한다.
	inline int GetDayOffset(IN int a_year,
							IN int a_month,
							IN int a_day)
	{
		assert(a_year != 0);
		assert(a_month>=1 && a_month<=12);
		assert(a_day>=1 && a_day<=31);

		bool isLeapYear = IsLeapYear(a_year);
		int dayCnt = GetDayOffset(a_month, a_day, isLeapYear);
		int ret;
		if (a_year > 0) {
			int leapDayCnt = GetLeapDayCount(a_year);
			ret = (a_year - 1) * 365 + leapDayCnt + dayCnt;
		}
		else {
			int leapDayCnt = a_year==-1 ? 0 : GetLeapDayCount(a_year + 1);
			ret = (a_year + 1) * 365 + leapDayCnt;
			ret -= (365 + (isLeapYear ? 1 : 0)) - dayCnt;
		}
		return ret;
	}


	// 1년 1월 1일(월요일)로부터 a_days일 만큼 지난 날짜의 요일을 구한다.
	inline DayOfTheWeek GetDayOfTheWeek(IN int a_days)
	{
		return (DayOfTheWeek)(((a_days % 7) + 1) % 7);
	}


	// 입력받은 날짜의 요일을 구한다.
	inline DayOfTheWeek GetDayOfTheWeek(IN int a_year,
										IN int a_month,
										IN int a_day)
	{
		return GetDayOfTheWeek(GetDayOffset(a_year, a_month, a_day));
	}


	inline MString ToString(IN const char* a_format,
							IN const tm& a_tm)
	{
		assert(a_format != nullptr);
		MString ret;
		const size_t Limit = 1024 * 1024;
		size_t bufsize = 64;

		do {
			ret.Resize(bufsize);
			size_t len = std::strftime(ret.GetData(),
									   bufsize,
									   a_format,
									   &a_tm);
			if (len > 0) {
				ret.Resize(len, true);
				assert(ret.GetLength() == len);
				break;
			}
			assert(len == 0);
			bufsize *= 2;
		} while (bufsize <= Limit);
		return ret;
	}



	inline void CheckParam_Year(IN int a_year)
	{
		const int LimitMax = (1 << 23) - 1;
		const int LimitMin = ~LimitMax;
		if (a_year > LimitMax) {
			asd_RaiseException("overflow parameter : %d", a_year);
		}
		else if (a_year < LimitMin) {
			asd_RaiseException("underflow parameter : %d", a_year);
		}
	}


	inline void CheckParam_Month(IN int a_month)
	{
		if (a_month < 1 || a_month > 12)
			asd_RaiseException("invalid parameter : %d", a_month);
	}


	inline void CheckParam_Day(IN int a_day)
	{
		if (a_day < 1 || a_day > 31)
			asd_RaiseException("invalid parameter : %d", a_day);
	}


	inline void CheckParam_Hour(IN int a_hour)
	{
		if (a_hour < 0 || a_hour >= 24)
			asd_RaiseException("invalid parameter : %d", a_hour);
	}


	inline void CheckParam_Minute(IN int a_minute)
	{
		if (a_minute < 0 || a_minute >= 60)
			asd_RaiseException("invalid parameter : %d", a_minute);
	}


	inline void CheckParam_Second(IN int a_second)
	{
		if (a_second < 0 || a_second > 60)
			asd_RaiseException("invalid parameter : %d", a_second);
	}


	inline void CheckParam_Millisecond(IN int a_millisecond)
	{
		if (a_millisecond < 0 || a_millisecond >= 1000)
			asd_RaiseException("invalid parameter : %d", a_millisecond);
	}



#define asd_Define_ConvertFunction_From(Class, Type, ParamName)		\
	Class::Class(IN const Type& a_src)								\
	{																\
		From(a_src);												\
	}																\
																	\
	Class& Class::operator = (IN const Type& a_src)					\
	{																\
		return From(a_src);											\
	}																\
																	\
	Class& Class::From(IN const Type& ParamName)					\


#define asd_Define_ConvertFunction_To(Class, Type, ParamName)		\
	Class::operator Type() const noexcept							\
	{																\
		Type r;														\
		memset(&r, 0, sizeof(r));									\
		return To(r);												\
	}																\
																	\
	Type& Class::To(OUT Type& a_dst) const noexcept					\



	Date::Date(IN int a_year /*= 1*/,
			   IN int a_month /*= 1*/,
			   IN int a_day /*= 1*/)
	{
		Init(a_year, a_month, a_day);
	}


	Date& Date::Init(IN int a_year /*= 1*/,
					 IN int a_month /*= 1*/,
					 IN int a_day /*= 1*/)
	{
		Year(a_year);
		Month(a_month);
		Day(a_day);
		return *this;
	}


	int Date::Year() const noexcept
	{
		return m_year;
	}


	Date& Date::Year(IN int a_year)
	{
		CheckParam_Year(a_year);
		m_year = a_year;
		assert(Year() == a_year);
		return *this;
	}


	int Date::Month() const noexcept
	{
		return m_month;
	}


	Date& Date::Month(IN int a_month)
	{
		CheckParam_Month(a_month);
		m_month = a_month;
		assert(Month() == a_month);
		return *this;
	}
	

	int Date::Day() const noexcept
	{
		return m_day;
	}


	Date& Date::Day(IN int a_day)
	{
		CheckParam_Day(a_day);
		m_day = a_day;
		assert(Day() == a_day);
		return *this;
	}


	DayOfTheWeek Date::DayOfTheWeek() const noexcept
	{
		return GetDayOfTheWeek(Year(), Month(), Day());
	}


	MString Date::ToString(const char* a_format /*= "%Y-%m-%d"*/) const noexcept
	{
		if (a_format == nullptr)
			a_format = "%Y-%m-%d";
		return asd::ToString(a_format, *this);
	}


	int Date::Compare(IN const Date& a_left,
					  IN const Date& a_right) noexcept
	{
		int cmp1;
		int cmp2;

		cmp1 = a_left.Year();
		cmp2 = a_right.Year();
		if (cmp1 < cmp2)
			return -1;
		else if (cmp1 > cmp2)
			return 1;

		cmp1 = a_left.Month();
		cmp2 = a_right.Month();
		if (cmp1 < cmp2)
			return -1;
		else if (cmp1 > cmp2)
			return 1;

		cmp1 = a_left.Day();
		cmp2 = a_right.Day();
		if (cmp1 < cmp2)
			return -1;
		else if (cmp1 > cmp2)
			return 1;

		return 0;
	}


	asd_Define_ConvertFunction_From(Date, tm, a_src)
	{
		Init(a_src.tm_year + 1900,
			 a_src.tm_mon + 1,
			 a_src.tm_mday);
		return *this;
	}

	asd_Define_ConvertFunction_To(Date, tm, a_dst)
	{
		const int y = Year();
		const int m = Month();
		const int d = Day();
		a_dst.tm_year = y - 1900;
		a_dst.tm_mon = m - 1;
		a_dst.tm_mday = d;
		a_dst.tm_wday = GetDayOfTheWeek(y, m, d);
		a_dst.tm_yday = GetDayOffset(m, d, IsLeapYear(y));
		return a_dst;
	}


	asd_Define_ConvertFunction_From(Date, SQL_DATE_STRUCT, a_src)
	{
		Init(a_src.year,
			 a_src.month,
			 a_src.day);
		return *this;
	}

	asd_Define_ConvertFunction_To(Date, SQL_DATE_STRUCT, a_dst)
	{
		a_dst.year = Year();
		a_dst.month = Month();
		a_dst.day = Day();
		return a_dst;
	}


	asd_Define_ConvertFunction_From(Date, SQL_TIMESTAMP_STRUCT, a_src)
	{
		Init(a_src.year,
			 a_src.month,
			 a_src.day);
		return *this;
	}

	asd_Define_ConvertFunction_To(Date, SQL_TIMESTAMP_STRUCT, a_dst)
	{
		a_dst.year = Year();
		a_dst.month = Month();
		a_dst.day = Day();
		return a_dst;
	}



	Time::Time(IN int a_hour /*= 0*/,
			   IN int a_minute /*= 0*/,
			   IN int a_second /*= 0*/,
			   IN int a_millisecond /*= 0*/)
	{
		Init(a_hour, a_minute, a_second, a_millisecond);
	}


	Time& Time::Init(IN int a_hour /*= 0*/,
					 IN int a_minute /*= 0*/,
					 IN int a_second /*= 0*/,
					 IN int a_millisecond /*= 0*/)
	{
		Hour(a_hour);
		Minute(a_minute);
		Second(a_second);
		Millisecond(a_millisecond);
		return *this;
	}


	int Time::Hour() const noexcept
	{
		return m_hour;
	}


	Time& Time::Hour(IN int a_hour)
	{
		CheckParam_Hour(a_hour);
		m_hour = a_hour;
		assert(Hour() == a_hour);
		return *this;
	}


	int Time::Minute() const noexcept
	{
		return m_minute;
	}


	Time& Time::Minute(IN int a_minute)
	{
		CheckParam_Minute(a_minute);
		m_minute = a_minute;
		assert(Minute() == a_minute);
		return *this;
	}


	int Time::Second() const noexcept
	{
		return m_second;
	}


	Time& Time::Second(IN int a_second)
	{
		CheckParam_Second(a_second);
		m_second = a_second;
		assert(Second() == a_second);
		return *this;
	}


	int Time::Millisecond() const noexcept
	{
		return m_millisecond;
	}


	Time& Time::Millisecond(IN int a_millisecond)
	{
		CheckParam_Millisecond(a_millisecond);
		m_millisecond = a_millisecond;
		assert(Millisecond() == a_millisecond);
		return *this;
	}


	MString Time::ToString(const char* a_format /*= "%H:%M:%S"*/) const noexcept
	{
		if (a_format == nullptr) {
			return asd::ToString(a_format, *this);
		}
		else {
			return MString("%02d:%02d:%02d.%04d",
						   Hour(), Minute(), Second(), Millisecond());
		}
	}


	int Time::Compare(IN const Time& a_left,
					  IN const Time& a_right) noexcept
	{
		int cmp1;
		int cmp2;

		cmp1 = a_left.Hour();
		cmp2 = a_right.Hour();
		if (cmp1 < cmp2)
			return -1;
		else if (cmp1 > cmp2)
			return 1;

		cmp1 = a_left.Minute();
		cmp2 = a_right.Minute();
		if (cmp1 < cmp2)
			return -1;
		else if (cmp1 > cmp2)
			return 1;

		cmp1 = a_left.Second();
		cmp2 = a_right.Second();
		if (cmp1 < cmp2)
			return -1;
		else if (cmp1 > cmp2)
			return 1;

		cmp1 = a_left.Millisecond();
		cmp2 = a_right.Millisecond();
		if (cmp1 < cmp2)
			return -1;
		else if (cmp1 > cmp2)
			return 1;

		return 0;
	}


	asd_Define_ConvertFunction_From(Time, tm, a_src)
	{
		Init(a_src.tm_hour,
			 a_src.tm_min,
			 a_src.tm_sec);
		return *this;
	}

	asd_Define_ConvertFunction_To(Time, tm, a_dst)
	{
		a_dst.tm_hour = Hour();
		a_dst.tm_min = Minute();
		a_dst.tm_sec = Millisecond();
		return a_dst;
	}


	asd_Define_ConvertFunction_From(Time, SQL_TIME_STRUCT, a_src)
	{
		Init(a_src.hour,
			 a_src.minute,
			 a_src.second);
		return *this;
	}

	asd_Define_ConvertFunction_To(Time, SQL_TIME_STRUCT, a_dst)
	{
		a_dst.hour = Hour();
		a_dst.minute = Minute();
		a_dst.second = Second();
		return a_dst;
	}


	// https://msdn.microsoft.com/ko-kr/library/ms714556(v=vs.85).aspx 에서 
	// Ctrl + F로 Fraction 검색
	const int Fraction_Per_Millisec = 1000000;
	asd_Define_ConvertFunction_From(Time, SQL_TIMESTAMP_STRUCT, a_src)
	{
		Init(a_src.hour,
			 a_src.minute,
			 a_src.second,
			 a_src.fraction / Fraction_Per_Millisec);
		return *this;
	}

	asd_Define_ConvertFunction_To(Time, SQL_TIMESTAMP_STRUCT, a_dst)
	{
		a_dst.hour = Hour();
		a_dst.minute = Minute();
		a_dst.second = Second();
		a_dst.fraction = Millisecond() * Fraction_Per_Millisec;
		return a_dst;
	}



	DateTime::DateTime(IN int a_year /*= 1*/,
					   IN int a_month /*= 1*/,
					   IN int a_day /*= 1*/,
					   IN int a_hour /*= 0*/,
					   IN int a_minute /*= 0*/,
					   IN int a_second /*= 0*/,
					   IN int a_millisecond /*= 0*/)
	{
		Init(a_year, a_month, a_day, a_hour, a_minute, a_second, a_millisecond);
	}


	DateTime& DateTime::Init(IN int a_year /*= 1*/,
							 IN int a_month /*= 1*/,
							 IN int a_day /*= 1*/,
							 IN int a_hour /*= 0*/,
							 IN int a_minute /*= 0*/,
							 IN int a_second /*= 0*/,
							 IN int a_millisecond /*= 0*/)
	{
		m_date.Init(a_year, a_month, a_day);
		m_time.Init(a_hour, a_minute, a_second, a_millisecond);
		return *this;
	}


	int DateTime::Year() const noexcept
	{
		return m_date.Year();
	}


	DateTime& DateTime::Year(IN int a_year)
	{
		m_date.Year(a_year);
		return *this;
	}


	int DateTime::Month() const noexcept
	{
		return m_date.Month();
	}


	DateTime& DateTime::Month(IN int a_month)
	{
		m_date.Month(a_month);
		return *this;
	}


	int DateTime::Day() const noexcept
	{
		return m_date.Day();
	}


	DateTime& DateTime::Day(IN int a_day)
	{
		m_date.Day(a_day);
		return *this;
	}


	int DateTime::Hour() const noexcept
	{
		return m_time.Hour();
	}


	DateTime& DateTime::Hour(IN int a_hour)
	{
		m_time.Hour(a_hour);
		return *this;
	}


	int DateTime::Minute() const noexcept
	{
		return m_time.Minute();
	}


	DateTime& DateTime::Minute(IN int a_minute)
	{
		m_time.Minute(a_minute);
		return *this;
	}


	int DateTime::Second() const noexcept
	{
		return m_time.Second();
	}


	DateTime& DateTime::Second(IN int a_second)
	{
		m_time.Second(a_second);
		return *this;
	}


	int DateTime::Millisecond() const noexcept
	{
		return m_time.Millisecond();
	}


	DateTime& DateTime::Millisecond(IN int a_millisecond)
	{
		m_time.Millisecond(a_millisecond);
		return *this;
	}


	DayOfTheWeek DateTime::DayOfTheWeek() const noexcept
	{
		return m_date.DayOfTheWeek();
	}


	MString DateTime::ToString(const char* a_format /*= "%Y-%m-%d %H:%M:%S"*/) const noexcept
	{
		if (a_format == nullptr) {
			return MString("%s %s",
						   m_date.ToString().data(),
						   m_time.ToString().data());
		}
		return asd::ToString(a_format, *this);
	}


	int DateTime::Compare(IN const DateTime& a_left,
						  IN const DateTime& a_right) noexcept
	{
		int cmp = Date::Compare(a_left.m_date, a_right.m_date);
		if (cmp != 0)
			return cmp;

		return Time::Compare(a_left.m_time, a_right.m_time);
	}


	asd_Define_ConvertFunction_From(DateTime, tm, a_src)
	{
		m_date = a_src;
		m_time = a_src;
		return *this;
	}

	asd_Define_ConvertFunction_To(DateTime, tm, a_dst)
	{
		m_date.To(a_dst);
		m_time.To(a_dst);
		return a_dst;
	}


	asd_Define_ConvertFunction_From(DateTime, SQL_TIMESTAMP_STRUCT, a_src)
	{
		m_date = a_src;
		m_time = a_src;
		return *this;
	}

	asd_Define_ConvertFunction_To(DateTime, SQL_TIMESTAMP_STRUCT, a_dst)
	{
		m_date.To(a_dst);
		m_time.To(a_dst);
		return a_dst;
	}


	asd_Define_ConvertFunction_From(DateTime, SQL_DATE_STRUCT, a_src)
	{
		m_date = a_src;
		return *this;
	}

	asd_Define_ConvertFunction_To(DateTime, SQL_DATE_STRUCT, a_dst)
	{
		return m_date.To(a_dst);
	}


	asd_Define_ConvertFunction_From(DateTime, Date, a_src)
	{
		m_date = a_src;
		return *this;
	}

	asd_Define_ConvertFunction_To(DateTime, Date, a_dst)
	{
		a_dst = m_date;
		return a_dst;
	}


	asd_Define_ConvertFunction_From(DateTime, SQL_TIME_STRUCT, a_src)
	{
		m_time = a_src;
		return *this;
	}

	asd_Define_ConvertFunction_To(DateTime, SQL_TIME_STRUCT, a_dst)
	{
		return m_time.To(a_dst);
	}


	asd_Define_ConvertFunction_From(DateTime, Time, a_src)
	{
		m_time = a_src;
		return *this;
	}

	asd_Define_ConvertFunction_To(DateTime, Time, a_dst)
	{
		a_dst = m_time;
		return a_dst;
	}



	Date Date::Now()
	{
		return *localtime();
	}

	Time Time::Now()
	{
		return *localtime();
	}

	DateTime DateTime::Now()
	{
		return *localtime();
	}



#if asd_Debug
	struct UnitTest
	{
		struct TestCase { virtual void Run() = 0; };
		static std::vector<TestCase*> TestList;
		UnitTest()
		{
			for (auto test : TestList)
				test->Run();
			TestList.clear();
		}
	};
	std::vector<UnitTest::TestCase*> UnitTest::TestList;

#define asd_Define_UnitTest(TestName)								\
	struct UnitTest_ ## TestName : public UnitTest::TestCase		\
	{																\
		UnitTest_ ## TestName()										\
		{															\
			UnitTest::TestList.push_back(this);						\
		}															\
		virtual void Run() override;								\
	};																\
	const UnitTest_ ## TestName g_unitTest_ ## TestName;			\
	void UnitTest_ ## TestName::Run()								\


	asd_Define_UnitTest(GetLastDay)
	{
		for (int m=1; m<=12; ++m) {
			assert(GetLastDay(m, false) == Days[m]);
			if (m != 2)
				assert(GetLastDay(m, true) == Days[m]);
		}
		assert(GetLastDay(2, true) == 29);
	}


	asd_Define_UnitTest(LeapDay)
	{
		const int TestCount = 8000;
		for (int y=1; y<=TestCount; ++y) {
			if (IsLeapYear(y)) {
				if (y % 400 == 0)
					assert(true);
				else {
					assert(y % 100 != 0);
					assert(y % 4 == 0);
				}
			}
			int dayOffset = GetDayOffset(y, 1, 1);
			int pos = GetLeapDayCount(y);
			int neg = GetLeapDayCount(-y);
			assert(pos >= 0 && neg <= 0);
			assert(dayOffset == ((y-1)*365) + pos);
			assert(GetLeapDayCount(y, -y) == pos - neg);
			assert(GetLeapDayCount(-y, y) == pos - neg);
		}

		for (int y=-1; y>=-TestCount; --y) {
			if (IsLeapYear(y)) {
				int t = y - 3;
				if (t % 400 == 0)
					assert(true);
				else {
					assert(t % 100 != 0 &&
						   t % 4 == 0);
				}
			}
			int dayOffset = GetDayOffset(y, 1, 1);
			assert(dayOffset == (y*365) + GetLeapDayCount(y));
		}
	}

	const UnitTest g_unitTest;

#endif
}

#include "stdafx.h"
#include "asd/datetime.h"
#include "asd/classutil.h"

namespace asd
{
#if asd_Platform_Windows
	// https://social.msdn.microsoft.com/Forums/ko-KR/4aabf49f-7a5d-4f51-be5e-197c511c42ce/5394551221-49345548895064049436-localtimes-gmtimes51032?forum=visualcplusko
	struct FixTimezone
	{
		FixTimezone()
		{
			::_tzset();
		}
	} g_fixTimezone;
#endif


	time_t* TlsNow()
	{
		thread_local time_t t_time;
		t_time = ::time(nullptr);
		return &t_time;
	}


	tm* localtime(const time_t* a_time /*= nullptr*/)
	{
		thread_local tm t_tm;

		if (a_time == nullptr)
			a_time = TlsNow();

#if asd_Platform_Windows
		int err = ::localtime_s(&t_tm, a_time);
		if (err)
			asd_OnErr("localtime_s error, {}", err);
		return &t_tm;
#else
		return ::localtime_r(a_time, &t_tm);
#endif
	}


	tm* gmtime(const time_t* a_time /*= nullptr*/)
	{
		thread_local tm t_tm;

		if (a_time == nullptr)
			a_time = TlsNow();

#if asd_Platform_Windows
		int err = ::gmtime_s(&t_tm, a_time);
		if (err)
			asd_OnErr("gmtime_s error, {}", err);
		return &t_tm;
#else
		return ::gmtime_r(a_time, &t_tm);
#endif
	}



	struct CurrentTimeZone : public ThreadLocal<CurrentTimeZone>
	{
		int OffsetSec;
		CurrentTimeZone()
		{
			static_assert(std::numeric_limits<time_t>::min() < 0, "time_t is unsigned type");

			const time_t now = ::time(nullptr);
			const time_t gt = ::mktime(asd::gmtime(&now));
			const time_t diff = now - gt;
			OffsetSec = (int)diff;
		}
	};

	int GetCurrentTimeZone_Sec()
	{
		return CurrentTimeZone::ThreadLocalInstance().OffsetSec;
	}

	int GetCurrentTimeZone_Hour()
	{
		return GetCurrentTimeZone_Sec() / 60 / 60;
	}



	//                        1   2   3   4   5   6   7   8   9   10  11  12
	const int Days[13] = {-1, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};


	// 1년 1월 1일로부터 a_year년 1월 1일 사이의 윤일 개수를 구한다.
	inline int GetLeapDayCount(int a_year)
	{
		asd_DAssert(a_year != 0);

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
	inline int GetLeapDayCount(int a_year1,
							   int a_year2)
	{
		if (a_year1 == a_year2)
			return 0;

		if (a_year1 > a_year2)
			std::swap(a_year1, a_year2);

		asd_DAssert(a_year2 > a_year1);
		return GetLeapDayCount(a_year2) - GetLeapDayCount(a_year1);
	}


	// 윤년인지 여부
	inline bool IsLeapYear(int a_year)
	{
		asd_DAssert(a_year != 0);
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
	inline int GetLastDay(int a_month,
						  bool a_isLeapYear)
	{
		asd_DAssert(a_month>=1 && a_month<=12);
		int r = Days[a_month];
		if (a_isLeapYear && a_month==2) {
			asd_DAssert(r + 1 == 29);
			return r + 1;
		}
		return r;
	}


	// 1월 1일부터 a_month월 a_day일 까지 지난 일 수를 구한다.
	inline int GetDayOffset(int a_month,
							int a_day,
							bool a_isLeapYear)
	{
		asd_DAssert(a_month>=1 && a_month<=12);
		asd_DAssert(a_day>=1 && a_day<=GetLastDay(a_month, a_isLeapYear));

		int r = 0;
		const int m = a_month - 1;
		for (int i=1; i<=m; ++i)
			r += GetLastDay(i, a_isLeapYear);
		r += a_day - 1;
		asd_DAssert(r >= 0);
		return r;
	}


	// 1년 1월 1일로부터 a_year년 a_month월 a_day일 까지 경과한 일 수를 구한다.
	inline int GetDayOffset(int a_year,
							int a_month,
							int a_day)
	{
		asd_DAssert(a_year != 0);
		asd_DAssert(a_month>=1 && a_month<=12);
		asd_DAssert(a_day>=1 && a_day<=31);

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
	inline DayOfTheWeek GetDayOfTheWeek(int a_days)
	{
		if (a_days >= 0)
			return (DayOfTheWeek)(((a_days % 7) + 1) % 7);
		return (DayOfTheWeek)((7 - ( (-a_days) % 7 ) + 1) % 7);
	}


	// 입력받은 날짜의 요일을 구한다.
	inline DayOfTheWeek GetDayOfTheWeek(int a_year,
										int a_month,
										int a_day)
	{
		return GetDayOfTheWeek(GetDayOffset(a_year, a_month, a_day));
	}


	inline MString ToString(const char* a_format,
							const tm& a_tm)
	{
		asd_DAssert(a_format != nullptr);
		MString ret;
		const size_t Limit = 1024 * 1024;
		size_t bufsize = 64;

		do {
			ret.resize(bufsize);
			size_t len = std::strftime(ret.data(),
									   bufsize,
									   a_format,
									   &a_tm);
			if (len > 0) {
				ret.resize(len);
				asd_DAssert(ret.length() == len);
				break;
			}
			asd_DAssert(len == 0);
			bufsize *= 2;
		} while (bufsize <= Limit);
		return ret;
	}



	inline void CheckParam_Year(int a_year)
	{
		const int LimitMax = (1 << 23) - 1;
		const int LimitMin = ~LimitMax;
		if (a_year > LimitMax) {
			asd_RaiseException("overflow parameter : {}", a_year);
		}
		else if (a_year < LimitMin) {
			asd_RaiseException("underflow parameter : {}", a_year);
		}
		else if (a_year == 0) {
			asd_RaiseException("invalid parameter : {}", a_year);
		}
	}


	inline void CheckParam_Month(int a_month)
	{
		if (a_month < 1 || a_month > 12)
			asd_RaiseException("invalid parameter : {}", a_month);
	}


	inline void CheckParam_Day(int a_day)
	{
		if (a_day < 1 || a_day > 31)
			asd_RaiseException("invalid parameter : {}", a_day);
	}


	inline void CheckParam_Hour(int a_hour)
	{
		if (a_hour < 0 || a_hour >= 24)
			asd_RaiseException("invalid parameter : {}", a_hour);
	}


	inline void CheckParam_Minute(int a_minute)
	{
		if (a_minute < 0 || a_minute >= 60)
			asd_RaiseException("invalid parameter : {}", a_minute);
	}


	inline void CheckParam_Second(int a_second)
	{
		if (a_second < 0 || a_second > 60)
			asd_RaiseException("invalid parameter : {}", a_second);
	}


	inline void CheckParam_Millisecond(int a_millisecond)
	{
		if (a_millisecond < 0 || a_millisecond >= 1000)
			asd_RaiseException("invalid parameter : {}", a_millisecond);
	}



#define asd_Define_ConvertFunction_From(Class, Type, ParamName)		\
	Class::Class(const Type& a_src)									\
	{																\
		From(a_src);												\
	}																\
																	\
	Class& Class::operator = (const Type& a_src)					\
	{																\
		return From(a_src);											\
	}																\
																	\
	Class& Class::From(const Type& ParamName)						\


#define asd_Define_ConvertFunction_To(Class, Type, ParamName)		\
	Class::operator Type() const									\
	{																\
		Type r;														\
		memset(&r, 0, sizeof(r));									\
		return To(r);												\
	}																\
																	\
	Type& Class::To(Type& a_dst /*Out*/) const						\



	Date::Date(int a_year /*= 1*/,
			   int a_month /*= 1*/,
			   int a_day /*= 1*/)
	{
		Init(a_year, a_month, a_day);
	}


	Date& Date::Init(int a_year /*= 1*/,
					 int a_month /*= 1*/,
					 int a_day /*= 1*/)
	{
		Year(a_year);
		Month(a_month);
		Day(a_day);
		return *this;
	}


	int Date::Year() const
	{
		return m_year;
	}


	Date& Date::Year(int a_year)
	{
		CheckParam_Year(a_year);
		m_year = a_year;
		asd_DAssert(Year() == a_year);
		return *this;
	}


	int Date::Month() const
	{
		return m_month;
	}


	Date& Date::Month(int a_month)
	{
		CheckParam_Month(a_month);
		m_month = a_month;
		asd_DAssert(Month() == a_month);
		return *this;
	}
	

	int Date::Day() const
	{
		return m_day;
	}


	Date& Date::Day(int a_day)
	{
		CheckParam_Day(a_day);
		m_day = a_day;
		asd_DAssert(Day() == a_day);
		return *this;
	}


	DayOfTheWeek Date::GetDayOfTheWeek() const
	{
		return asd::GetDayOfTheWeek(Year(), Month(), Day());
	}


	MString Date::ToString(const char* a_format /*= "%Y-%m-%d"*/) const
	{
		if (a_format == nullptr)
			a_format = "%Y-%m-%d";
		return asd::ToString(a_format, *this);
	}


	int Date::Compare(const Date& a_left,
					  const Date& a_right)
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


	inline void Date_to_tm(const Date& a_src,
						   tm& a_dst /*Out*/)
	{
		const int y = a_src.Year();
		const int m = a_src.Month();
		const int d = a_src.Day();
		a_dst.tm_year = y - 1900;
		a_dst.tm_mon = m - 1;
		a_dst.tm_mday = d;
		a_dst.tm_wday = static_cast<int>(GetDayOfTheWeek(y, m, d));
		a_dst.tm_yday = GetDayOffset(m, d, IsLeapYear(y));
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
		std::memset(&a_dst, 0, sizeof(a_dst));
		Date_to_tm(*this, a_dst);
		return a_dst;
	}


	asd_Define_ConvertFunction_From(Date, time_t, a_src)
	{
		const tm* temp = asd::localtime(&a_src);
		return From(*temp);;
	}

	asd_Define_ConvertFunction_To(Date, time_t, a_dst)
	{
		tm temp;
		To(temp);
		a_dst = mktime(&temp);
		return a_dst;
	}


	asd_Define_ConvertFunction_From(Date, std::chrono::system_clock::time_point, a_src)
	{
		const time_t temp = std::chrono::system_clock::to_time_t(a_src);
		return From(temp);;
	}

	asd_Define_ConvertFunction_To(Date, std::chrono::system_clock::time_point, a_dst)
	{
		time_t temp;
		To(temp);
		a_dst = std::chrono::system_clock::from_time_t(temp);
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



	Time::Time(int a_hour /*= 0*/,
			   int a_minute /*= 0*/,
			   int a_second /*= 0*/,
			   int a_millisecond /*= 0*/)
	{
		Init(a_hour, a_minute, a_second, a_millisecond);
	}


	Time& Time::Init(int a_hour /*= 0*/,
					 int a_minute /*= 0*/,
					 int a_second /*= 0*/,
					 int a_millisecond /*= 0*/)
	{
		Hour(a_hour);
		Minute(a_minute);
		Second(a_second);
		Millisecond(a_millisecond);
		return *this;
	}


	int Time::Hour() const
	{
		return m_hour;
	}


	Time& Time::Hour(int a_hour)
	{
		CheckParam_Hour(a_hour);
		m_hour = a_hour;
		asd_DAssert(Hour() == a_hour);
		return *this;
	}


	int Time::Minute() const
	{
		return m_minute;
	}


	Time& Time::Minute(int a_minute)
	{
		CheckParam_Minute(a_minute);
		m_minute = a_minute;
		asd_DAssert(Minute() == a_minute);
		return *this;
	}


	int Time::Second() const
	{
		return m_second;
	}


	Time& Time::Second(int a_second)
	{
		CheckParam_Second(a_second);
		m_second = a_second;
		asd_DAssert(Second() == a_second);
		return *this;
	}


	int Time::Millisecond() const
	{
		return m_millisecond;
	}


	Time& Time::Millisecond(int a_millisecond)
	{
		CheckParam_Millisecond(a_millisecond);
		m_millisecond = a_millisecond;
		asd_DAssert(Millisecond() == a_millisecond);
		return *this;
	}


	MString Time::ToString(const char* a_format /*= "%H:%M:%S"*/) const
	{
		if (a_format == nullptr) {
			return asd::ToString(a_format, *this);
		}
		else {
			return MString::Format("{:02d}:{:02d}:{:02d}.{:04d}",
								   Hour(), Minute(), Second(), Millisecond());
		}
	}


	int Time::Compare(const Time& a_left,
					  const Time& a_right)
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


	inline void Time_to_tm(const Time& a_src,
						   tm& a_dst /*Out*/)
	{
		a_dst.tm_hour = a_src.Hour();
		a_dst.tm_min = a_src.Minute();
		a_dst.tm_sec = a_src.Second();
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
		std::memset(&a_dst, 0, sizeof(a_dst));
		Time_to_tm(*this, a_dst);
		return a_dst;
	}


	asd_Define_ConvertFunction_From(Time, time_t, a_src)
	{
		const tm* temp = asd::localtime(&a_src);
		return From(*temp);
	}

	asd_Define_ConvertFunction_To(Time, time_t, a_dst)
	{
		a_dst = 24 * 60 * 60
			+ Hour() * 60 * 60
			+ Minute() * 60
			+ Second()
			- GetCurrentTimeZone_Sec();
		return a_dst;
	}


	asd_Define_ConvertFunction_From(Time, std::chrono::system_clock::time_point, a_src)
	{
		const time_t temp = std::chrono::system_clock::to_time_t(a_src);
		return From(temp);;
	}

	asd_Define_ConvertFunction_To(Time, std::chrono::system_clock::time_point, a_dst)
	{
		time_t temp;
		To(temp);
		a_dst = std::chrono::system_clock::from_time_t(temp);
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



	DateTime::DateTime(int a_year /*= 1*/,
					   int a_month /*= 1*/,
					   int a_day /*= 1*/,
					   int a_hour /*= 0*/,
					   int a_minute /*= 0*/,
					   int a_second /*= 0*/,
					   int a_millisecond /*= 0*/)
	{
		Init(a_year, a_month, a_day, a_hour, a_minute, a_second, a_millisecond);
	}


	DateTime& DateTime::Init(int a_year /*= 1*/,
							 int a_month /*= 1*/,
							 int a_day /*= 1*/,
							 int a_hour /*= 0*/,
							 int a_minute /*= 0*/,
							 int a_second /*= 0*/,
							 int a_millisecond /*= 0*/)
	{
		m_date.Init(a_year, a_month, a_day);
		m_time.Init(a_hour, a_minute, a_second, a_millisecond);
		return *this;
	}


	int DateTime::Year() const
	{
		return m_date.Year();
	}


	DateTime& DateTime::Year(int a_year)
	{
		m_date.Year(a_year);
		return *this;
	}


	int DateTime::Month() const
	{
		return m_date.Month();
	}


	DateTime& DateTime::Month(int a_month)
	{
		m_date.Month(a_month);
		return *this;
	}


	int DateTime::Day() const
	{
		return m_date.Day();
	}


	DateTime& DateTime::Day(int a_day)
	{
		m_date.Day(a_day);
		return *this;
	}


	int DateTime::Hour() const
	{
		return m_time.Hour();
	}


	DateTime& DateTime::Hour(int a_hour)
	{
		m_time.Hour(a_hour);
		return *this;
	}


	int DateTime::Minute() const
	{
		return m_time.Minute();
	}


	DateTime& DateTime::Minute(int a_minute)
	{
		m_time.Minute(a_minute);
		return *this;
	}


	int DateTime::Second() const
	{
		return m_time.Second();
	}


	DateTime& DateTime::Second(int a_second)
	{
		m_time.Second(a_second);
		return *this;
	}


	int DateTime::Millisecond() const
	{
		return m_time.Millisecond();
	}


	DateTime& DateTime::Millisecond(int a_millisecond)
	{
		m_time.Millisecond(a_millisecond);
		return *this;
	}


	DayOfTheWeek DateTime::GetDayOfTheWeek() const
	{
		return m_date.GetDayOfTheWeek();
	}


	MString DateTime::ToString(const char* a_format /*= "%Y-%m-%d %H:%M:%S"*/) const
	{
		if (a_format == nullptr) {
			return MString::Format("{} {}",
								   m_date.ToString(),
								   m_time.ToString());
		}
		return asd::ToString(a_format, *this);
	}


	int DateTime::Compare(const DateTime& a_left,
						  const DateTime& a_right)
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
		std::memset(&a_dst, 0, sizeof(a_dst));
		Date_to_tm(m_date, a_dst);
		Time_to_tm(m_time, a_dst);
		return a_dst;
	}


	asd_Define_ConvertFunction_From(DateTime, time_t, a_src)
	{
		const tm* temp = asd::localtime(&a_src);
		return From(*temp);;
	}

	asd_Define_ConvertFunction_To(DateTime, time_t, a_dst)
	{
		tm temp;
		To(temp);
		a_dst = mktime(&temp);
		return a_dst;
	}


	asd_Define_ConvertFunction_From(DateTime, std::chrono::system_clock::time_point, a_src)
	{
		const time_t temp = std::chrono::system_clock::to_time_t(a_src);
		return From(temp);;
	}

	asd_Define_ConvertFunction_To(DateTime, std::chrono::system_clock::time_point, a_dst)
	{
		time_t temp;
		To(temp);
		a_dst = std::chrono::system_clock::from_time_t(temp);
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
			UnitTest::TestList.emplace_back(this);					\
		}															\
		virtual void Run() override;								\
	};																\
	const UnitTest_ ## TestName g_unitTest_ ## TestName;			\
	void UnitTest_ ## TestName::Run()								\


	asd_Define_UnitTest(GetLastDay)
	{
		for (int m=1; m<=12; ++m) {
			asd_DAssert(GetLastDay(m, false) == Days[m]);
			if (m != 2)
				asd_DAssert(GetLastDay(m, true) == Days[m]);
		}
		asd_DAssert(GetLastDay(2, true) == 29);
	}


	asd_Define_UnitTest(LeapDay)
	{
		const int TestCount = 8000;
		for (int y=1; y<=TestCount; ++y) {
			if (IsLeapYear(y)) {
				if (y % 400 == 0)
					asd_DAssert(true);
				else {
					asd_DAssert(y % 100 != 0);
					asd_DAssert(y % 4 == 0);
				}
			}
			int dayOffset = GetDayOffset(y, 1, 1);
			int pos = GetLeapDayCount(y);
			int neg = GetLeapDayCount(-y);
			asd_DAssert(pos >= 0 && neg <= 0);
			asd_DAssert(dayOffset == ((y-1)*365) + pos);
			asd_DAssert(GetLeapDayCount(y, -y) == pos - neg);
			asd_DAssert(GetLeapDayCount(-y, y) == pos - neg);
		}

		for (int y=-1; y>=-TestCount; --y) {
			if (IsLeapYear(y)) {
				int t = y - 3;
				if (t % 400 != 0){
					asd_DAssert(t % 100 != 0 &&
								t % 4 == 0);
				}
			}
			int dayOffset = GetDayOffset(y, 1, 1);
			asd_DAssert(dayOffset == (y*365) + GetLeapDayCount(y));
		}
	}

	const UnitTest g_unitTest;

#endif
}

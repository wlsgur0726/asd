#include "stdafx.h"
#include "asd/time.h"

namespace asd
{
	const int32_t Mask_Year			= 0xFFFFFE00;
	const int32_t Mask_Month		= 0x000001E0;
	const int32_t Mask_Day			= 0x0000001F;
	const int32_t Mask_Hour			= 0xFE000000;
	const int32_t Mask_Minute		= 0x01FC0000;
	const int32_t Mask_Second		= 0x0003F800;
	const int32_t Mask_Millisecond	= 0x000007FF;
	static_assert(0xFFFFFFFF == (Mask_Year ^ Mask_Month ^ Mask_Day),
				  "invalid const value");
	static_assert(0xFFFFFFFF == (Mask_Hour ^ Mask_Minute ^ Mask_Second ^ Mask_Millisecond),
				  "invalid const value");


	const int Offset_Year = 9;
	const int Offset_Month = 5;
	const int Offset_Day = 0;
	const int Offset_Hour = 25;
	const int Offset_Minute = 18;
	const int Offset_Second = 11;
	const int Offset_Millisecond = 0;


	//                    1   2   3   4   5   6   7   8   9   10  11  12
	const int Days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};


	// 1년 1월 1일로부터 a_days일 만큼 지난 날짜의 요일을 구한다.
	inline DayOfTheWeek GetDayOfTheWeek(IN int a_days)
	{
		return (DayOfTheWeek)(((a_days % 7) + 1) % 7);
	}


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

		assert(a_year1 < a_year2);
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


	// 1월 1일부터 a_month월 a_day일 까지 지난 일 수를 구한다.
	inline int GetDayCount(IN int a_month,
						   IN int a_day,
						   IN bool a_isLeapYear)
	{
		assert(a_month>=1 && a_month<=12);
		assert(a_day>=1 && a_day<=31);

		int r = 0;
		const int m = a_month - 1;
		for (int i=0; i<m; ++i)
			r += Days[i];
		r += a_day - 1;

		if (a_isLeapYear && a_month > 2)
			++r;

		return r;
	}


	// 1년 1월 1일로부터 a_year년 a_month월 a_day일 까지 경과한 일 수를 구한다.
	inline int GetDayCount(IN int a_year,
						   IN int a_month,
						   IN int a_day)
	{
		assert(a_year != 0);
		assert(a_month>=1 && a_month<=12);
		assert(a_day>=1 && a_day<=31);

		
		bool isLeapYear = IsLeapYear(a_year);
		int dayCnt = GetDayCount(a_month, a_day, isLeapYear);
		int ret;
		if (a_year > 0) {
			int leapDayCnt = GetLeapDayCount(a_year);
			ret = (a_year - 1) * 365 + leapDayCnt + dayCnt;
		}
		else {
			int leapDayCnt = a_year==-1 ? 0 : GetLeapDayCount(a_year + 1);
			ret = (a_year + 1) * 365 - leapDayCnt;
			ret -= (365 - dayCnt) + (isLeapYear ? 1 : 0);
		}
		return ret;
	}



#ifndef asd_Debug
#	define asd_CheckCorruption(F1, F2, F3, F4, F5, F6)
#
#else
#	define asd_CheckCorruption(F1, F2, F3, F4, F5, F6)		\
	struct CheckCorruption									\
	{														\
		const Time& m_time;									\
		const int m_org_ ## F1;								\
		const int m_org_ ## F2;								\
		const int m_org_ ## F3;								\
		const int m_org_ ## F4;								\
		const int m_org_ ## F5;								\
		const int m_org_ ## F6;								\
															\
		CheckCorruption(REF const Time& a_time)				\
			: m_time(a_time)								\
			, m_org_ ## F1(a_time.F1())						\
			, m_org_ ## F2(a_time.F2())						\
			, m_org_ ## F3(a_time.F3())						\
			, m_org_ ## F4(a_time.F4())						\
			, m_org_ ## F5(a_time.F5())						\
			, m_org_ ## F6(a_time.F6())						\
		{													\
		}													\
															\
		~CheckCorruption()									\
		{													\
			assert(m_time.F1() == m_org_ ## F1);			\
			assert(m_time.F2() == m_org_ ## F2);			\
			assert(m_time.F3() == m_org_ ## F3);			\
			assert(m_time.F4() == m_org_ ## F4);			\
			assert(m_time.F5() == m_org_ ## F5);			\
			assert(m_time.F6() == m_org_ ## F6);			\
		}													\
	};														\
	CheckCorruption _checkCorruption(*this)					\

#endif


	inline void CheckParam_Year(IN int a_year)
	{
		const int LimitMax = 0x003FFFFF;
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
		if (a_second < 0 || a_second >= 60)
			asd_RaiseException("invalid parameter : %d", a_second);
	}


	inline void CheckParam_Millisecond(IN int a_millisecond)
	{
		if (a_millisecond < 0 || a_millisecond >= 1000)
			asd_RaiseException("invalid parameter : %d", a_millisecond);
	}



	Time::Time() asd_NoThrow
	{
	}


	Time::Time(IN int a_year,
			   IN int a_month,
			   IN int a_day /*= 1*/,
			   IN int a_hour /*= 0*/,
			   IN int a_minute /*= 0*/,
			   IN int a_second /*= 0*/,
			   IN int a_millisecond /*= 0*/)
		asd_Throws(Exception)
	{
		Init(a_year, a_month, a_day, a_hour, a_minute, a_second, a_millisecond);
	}


	void Time::Init(IN int a_year,
					IN int a_month,
					IN int a_day /*= 1*/,
					IN int a_hour /*= 0*/,
					IN int a_minute /*= 0*/,
					IN int a_second /*= 0*/,
					IN int a_millisecond /*= 0*/)
		asd_Throws(Exception)
	{
		CheckParam_Year(a_year);
		CheckParam_Month(a_month);
		CheckParam_Day(a_day);
		CheckParam_Hour(a_hour);
		CheckParam_Minute(a_minute);
		CheckParam_Second(a_second);
		CheckParam_Millisecond(a_millisecond);

		m_data[0] = (a_year << Offset_Year) | (a_month << Offset_Month) | a_day;
		m_data[1] = (a_hour << Offset_Hour) | (a_minute << Offset_Minute) | (a_second << Offset_Second) | a_millisecond;

		assert(Year() == a_year);
		assert(Month() == a_month);
		assert(Day() == a_day);
		assert(Hour() == a_hour);
		assert(Minute() == a_minute);
		assert(Second() == a_second);
		assert(Millisecond() == a_millisecond);
	}


	int Time::Year() const asd_NoThrow
	{
		const int y = m_data[0] >> Offset_Year;
		return y;
	}


	void Time::Year(IN int a_year)
		asd_Throws(Exception)
	{
		CheckParam_Year(a_year);
		asd_CheckCorruption(Month, Day, Hour, Minute, Second, Millisecond);

		const int32_t others = m_data[0] & ~Mask_Year;
		m_data[0] = (a_year << Offset_Year) | others;
		assert(Year() == a_year);
	}


	int Time::Month() const asd_NoThrow
	{
		const int m = (m_data[0] & Mask_Month) >> Offset_Month;
		assert(m >= 0);
		assert(m <= 12);
		return m;
	}


	void Time::Month(IN int a_month)
		asd_Throws(Exception)
	{
		CheckParam_Month(a_month);
		asd_CheckCorruption(Year, Day, Hour, Minute, Second, Millisecond);

		const int32_t others = m_data[0] & ~Mask_Month;
		m_data[0] = (a_month << Offset_Minute) | others;
		assert(Month() == a_month);
	}


	int Time::Day() const asd_NoThrow
	{
		const int d = m_data[0] & Mask_Day;
		assert(d >= 0);
		assert(d <= 31);
		return d;
	}


	void Time::Day(IN int a_day)
		asd_Throws(Exception)
	{
		CheckParam_Day(a_day);
		asd_CheckCorruption(Year, Month, Hour, Minute, Second, Millisecond);

		const int others = m_data[0] & ~Mask_Day;
		m_data[0] = a_day | others;
		assert(Day() == a_day);
	}


	int Time::Hour() const asd_NoThrow
	{
		const int h = m_data[1] >> Offset_Hour;
		assert(h >= 0);
		assert(h < 24);
		return h;
	}


	void Time::Hour(IN int a_hour)
		asd_Throws(Exception)
	{
		CheckParam_Hour(a_hour);
		asd_CheckCorruption(Year, Month, Day, Minute, Second, Millisecond);

		const int others = m_data[1] & ~Mask_Hour;
		m_data[1] = (a_hour << Offset_Hour) | others;
		assert(Hour() == a_hour);
	}


	int Time::Minute() const asd_NoThrow
	{
		const int m = (m_data[1] & Mask_Minute) >> Offset_Minute;
		assert(m >= 0);
		assert(m < 60);
		return m;
	}


	void Time::Minute(IN int a_minute)
		asd_Throws(Exception)
	{
		CheckParam_Minute(a_minute);
		asd_CheckCorruption(Year, Month, Day, Hour, Second, Millisecond);

		const int others = m_data[1] & ~Mask_Minute;
		m_data[1] = (a_minute << Offset_Minute) | others;
		assert(Minute() == a_minute);
	}


	int Time::Second() const asd_NoThrow
	{
		const int s = (m_data[1] & Mask_Second) >> Offset_Second;
		assert(s >= 0);
		assert(s < 60);
		return s;
	}


	void Time::Second(IN int a_second)
		asd_Throws(Exception)
	{
		CheckParam_Second(a_second);
		asd_CheckCorruption(Year, Month, Day, Hour, Minute, Millisecond);

		const int others = m_data[1] & ~Mask_Second;
		m_data[1] = (a_second << Offset_Second) | others;
		assert(Second() == a_second);
	}


	int Time::Millisecond() const asd_NoThrow
	{
		const int ms = m_data[1] & Mask_Millisecond;
		assert(ms >= 0);
		assert(ms < 1000);
		return ms;
	}


	void Time::Millisecond(IN int a_millisecond)
		asd_Throws(Exception)
	{
		CheckParam_Millisecond(a_millisecond);
		asd_CheckCorruption(Year, Month, Day, Hour, Minute, Second);

		const int others = m_data[1] & ~Mask_Millisecond;
		m_data[1] = a_millisecond | others;
		assert(Millisecond() == a_millisecond);
	}


	DayOfTheWeek Time::DayOfTheWeek() const asd_NoThrow
	{
		return GetDayOfTheWeek(GetDayCount(Year(), Month(), Day()));
	}


	MString Time::ToString(const char* a_format /*= nullptr*/) const asd_NoThrow
	{
		MString ret;
		if (a_format == nullptr) {
			ret.Format("%d-%02d-%02d %02d:%02d:%02d.%03d",
					   Year(),
					   Month(),
					   Day(),
					   Hour(),
					   Minute(),
					   Second(),
					   Millisecond());
		}
		else {
			const size_t Limit = 1024 * 1024;
			size_t bufsize = 64;
			tm t;
			To(t);
			do {
				ret.Resize(bufsize);
				size_t len = std::strftime(ret.GetData(),
										   bufsize,
										   a_format,
										   &t);
				if (len > 0) {
					ret.Resize(len, true);
					assert(ret.GetLength() == len);
					break;
				}
				assert(len == 0);
				bufsize *= 2;
			} while (bufsize <= Limit);
		}
		return ret;
	}


	int Time::Compare(IN const Time& a_left,
					  IN const Time& a_right) asd_NoThrow
	{
		if (a_left.m_data[0] < a_right.m_data[0])
			return -1;
		else if (a_left.m_data[0] > a_right.m_data[0])
			return 1;

		if (a_left.m_data[1] < a_right.m_data[1])
			return -1;
		else if (a_left.m_data[1] > a_right.m_data[1])
			return 1;

		return 0;
	}


#define asd_Define_ConvertFunction_From(Type, ParamName)	\
	Time::Time(IN const Type& a_src)						\
		asd_Throws(Exception)								\
	{														\
		From(a_src);										\
	}														\
															\
	Time& Time::operator = (IN const Type& a_src)			\
		asd_Throws(Exception)								\
	{														\
		return From(a_src);									\
	}														\
															\
	Time& Time::From(IN const Type& ParamName)				\
		asd_Throws(Exception)								\


#define asd_Define_ConvertFunction_To(Type, ParamName)		\
	Time::operator Type() const asd_NoThrow					\
	{														\
		Type r;												\
		return To(r);										\
	}														\
															\
	Type& Time::To(OUT Type& a_dst) const asd_NoThrow		\


	asd_Define_ConvertFunction_From(IN tm, a_src)
	{
		Init(a_src.tm_year + 1900,
			 a_src.tm_mon + 1,
			 a_src.tm_mday,
			 a_src.tm_hour,
			 a_src.tm_min,
			 a_src.tm_sec,
			 0);
		return *this;
	}

	asd_Define_ConvertFunction_To(OUT tm, a_dst)
	{
		memset(&a_dst, 0, sizeof(a_dst));
		a_dst.tm_year = Year() - 1900;
		a_dst.tm_mon = Month() - 1;
		a_dst.tm_mday = Day();
		a_dst.tm_hour = Hour();
		a_dst.tm_min = Minute();
		a_dst.tm_sec = Second();
		a_dst.tm_wday = DayOfTheWeek();
		a_dst.tm_yday = GetDayCount(a_dst.tm_mon + 1,
									a_dst.tm_mday,
									IsLeapYear(a_dst.tm_year));
		return a_dst;
	}


	asd_Define_ConvertFunction_From(SQL_TIMESTAMP_STRUCT, a_src)
	{
		Init(a_src.year,
			 a_src.month,
			 a_src.day,
			 a_src.hour,
			 a_src.minute,
			 a_src.second,
			 a_src.fraction / 1000000);
		return *this;
	}

	asd_Define_ConvertFunction_To(SQL_TIMESTAMP_STRUCT, a_dst)
	{
		memset(&a_dst, 0, sizeof(a_dst));
		a_dst.year = Year();
		a_dst.month = Month();
		a_dst.day = Day();
		a_dst.hour = Hour();
		a_dst.minute = Minute();
		a_dst.second = Second();
		a_dst.fraction = Millisecond() * 1000000;
		return a_dst;
	}
}

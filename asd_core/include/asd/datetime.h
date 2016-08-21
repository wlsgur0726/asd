#pragma once
#include "asdbase.h"
#include "exception.h"
#include <ctime>
#include <chrono>

namespace asd
{
	enum class DayOfTheWeek : int
	{
		Sunday = 0,
		Monday,
		Tuesday,
		Wednesday,
		Thursday,
		Friday,
		Saturday
	};


	tm* localtime(IN const time_t* a_time) asd_noexcept;
	inline tm* localtime() asd_noexcept
	{
		time_t t = time(nullptr);
		return asd::localtime(&t);
	}

	tm* gmtime(IN const time_t* a_time) asd_noexcept;
	inline tm* gmtime() asd_noexcept
	{
		time_t t = time(nullptr);
		return asd::gmtime(&t);
	}


	int GetCurrentTimeZone_Sec();

	int GetCurrentTimeZone_Hour();


#define asd_DateTime_Declare_ConvertFunction(Class, Type)	\
		Class& From(IN const Type& a_src);					\
															\
		Class(IN const Type& a_src);						\
															\
		Class& operator = (IN const Type& a_src);			\
															\
		Type& To(OUT Type& a_dst) const asd_noexcept;		\
															\
		operator Type() const asd_noexcept;					\



	struct SQL_TIMESTAMP_STRUCT
	{
		int16_t		year;
		uint16_t	month;
		uint16_t	day;
		uint16_t	hour;
		uint16_t	minute;
		uint16_t	second;
		uint32_t	fraction;
	};

	struct SQL_DATE_STRUCT
	{
		int16_t		year;
		uint16_t	month;
		uint16_t	day;
	};

	struct SQL_TIME_STRUCT
	{
		uint16_t	hour;
		uint16_t	minute;
		uint16_t	second;
	};



	class Date
	{
		int32_t m_year : 23;
		uint32_t m_month : 4;
		uint32_t m_day : 5;

	public:
		// today
		static Date Now();

		Date(IN int a_year = 1,
			 IN int a_month = 1,
			 IN int a_day = 1);

		Date& Init(IN int a_year = 1,
				   IN int a_month = 1,
				   IN int a_day = 1);

		int Year() const asd_noexcept;
		Date& Year(IN int a_year);

		int Month() const asd_noexcept;
		Date& Month(IN int a_month);

		int Day() const asd_noexcept;
		Date& Day(IN int a_day);

		DayOfTheWeek DayOfTheWeek() const asd_noexcept;

		MString ToString(const char* a_format = "%Y-%m-%d") const asd_noexcept;

		// 비교
		static int Compare(IN const Date& a_left,
						   IN const Date& a_right) asd_noexcept;
		asd_Define_CompareOperator(Compare, Date);

		// 다른 타입 지원
		asd_DateTime_Declare_ConvertFunction(Date, tm);
		asd_DateTime_Declare_ConvertFunction(Date, time_t);
		asd_DateTime_Declare_ConvertFunction(Date, std::chrono::system_clock::time_point);
		asd_DateTime_Declare_ConvertFunction(Date, SQL_DATE_STRUCT);
		asd_DateTime_Declare_ConvertFunction(Date, SQL_TIMESTAMP_STRUCT);
	};



	class Time
	{
		uint32_t m_hour : 7;
		uint32_t m_minute : 7;
		uint32_t m_second : 7;
		uint32_t m_millisecond : 11;

	public:
		// 현재시간
		static Time Now();

		Time(IN int a_hour = 0,
			 IN int a_minute = 0,
			 IN int a_second = 0,
			 IN int a_millisecond = 0);

		Time& Init(IN int a_hour = 0,
				   IN int a_minute = 0,
				   IN int a_second = 0,
				   IN int a_millisecond = 0);

		int Hour() const asd_noexcept;
		Time& Hour(IN int a_hour);

		int Minute() const asd_noexcept;
		Time& Minute(IN int a_minute);

		int Second() const asd_noexcept;
		Time& Second(IN int a_second);

		int Millisecond() const asd_noexcept;
		Time& Millisecond(IN int a_millisecond);

		MString ToString(const char* a_format = "%H:%M:%S") const asd_noexcept;

		// 비교
		static int Compare(IN const Time& a_left,
						   IN const Time& a_right) asd_noexcept;
		asd_Define_CompareOperator(Compare, Time);

		// 다른 타입 지원
		asd_DateTime_Declare_ConvertFunction(Time, tm);
		asd_DateTime_Declare_ConvertFunction(Time, time_t);
		asd_DateTime_Declare_ConvertFunction(Time, std::chrono::system_clock::time_point);
		asd_DateTime_Declare_ConvertFunction(Time, SQL_TIME_STRUCT);
		asd_DateTime_Declare_ConvertFunction(Time, SQL_TIMESTAMP_STRUCT);
	};



	class DateTime
	{
		Date m_date;
		Time m_time;

	public:
		// 현재시간
		static DateTime Now();

		DateTime(IN int a_year = 1,
				 IN int a_month = 1,
				 IN int a_day = 1,
				 IN int a_hour = 0,
				 IN int a_minute = 0,
				 IN int a_second = 0,
				 IN int a_millisecond = 0);

		DateTime& Init(IN int a_year = 1,
					   IN int a_month = 1,
					   IN int a_day = 1,
					   IN int a_hour = 0,
					   IN int a_minute = 0,
					   IN int a_second = 0,
					   IN int a_millisecond = 0);

		int Year() const asd_noexcept;
		DateTime& Year(IN int a_year);

		int Month() const asd_noexcept;
		DateTime& Month(IN int a_month);

		int Day() const asd_noexcept;
		DateTime& Day(IN int a_day);

		int Hour() const asd_noexcept;
		DateTime& Hour(IN int a_hour);

		int Minute() const asd_noexcept;
		DateTime& Minute(IN int a_minute);

		int Second() const asd_noexcept;
		DateTime& Second(IN int a_second);

		int Millisecond() const asd_noexcept;
		DateTime& Millisecond(IN int a_millisecond);

		DayOfTheWeek DayOfTheWeek() const asd_noexcept;

		MString ToString(const char* a_format = "%Y-%m-%d %H:%M:%S") const asd_noexcept;


		// 비교
		static int Compare(IN const DateTime& a_left,
						   IN const DateTime& a_right) asd_noexcept;
		asd_Define_CompareOperator(Compare, DateTime);


		// 다른 타입 지원
		asd_DateTime_Declare_ConvertFunction(DateTime, tm);
		asd_DateTime_Declare_ConvertFunction(DateTime, time_t);
		asd_DateTime_Declare_ConvertFunction(DateTime, std::chrono::system_clock::time_point);
		asd_DateTime_Declare_ConvertFunction(DateTime, SQL_DATE_STRUCT);
		asd_DateTime_Declare_ConvertFunction(DateTime, SQL_TIME_STRUCT);
		asd_DateTime_Declare_ConvertFunction(DateTime, SQL_TIMESTAMP_STRUCT);
		asd_DateTime_Declare_ConvertFunction(DateTime, Date);
		asd_DateTime_Declare_ConvertFunction(DateTime, Time);

	};
}

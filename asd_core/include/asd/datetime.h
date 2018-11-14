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


	tm* localtime(const time_t* a_time = nullptr);

	tm* gmtime(const time_t* a_time = nullptr);


	int GetCurrentTimeZone_Sec();

	int GetCurrentTimeZone_Hour();


#define asd_DateTime_Declare_ConvertFunction(Class, Type)	\
		Class& From(const Type& a_src);						\
															\
		Class(const Type& a_src);							\
															\
		Class& operator = (const Type& a_src);				\
															\
		Type& To(Type& a_dst /*Out*/) const;				\
															\
		operator Type() const;								\



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

		Date(int a_year = 1,
			 int a_month = 1,
			 int a_day = 1);

		Date& Init(int a_year = 1,
				   int a_month = 1,
				   int a_day = 1);

		int Year() const;
		Date& Year(int a_year);

		int Month() const;
		Date& Month(int a_month);

		int Day() const;
		Date& Day(int a_day);

		DayOfTheWeek GetDayOfTheWeek() const;

		MString ToString(const char* a_format = "%Y-%m-%d") const;

		// 비교
		static int Compare(const Date& a_left,
						   const Date& a_right);
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

		Time(int a_hour = 0,
			 int a_minute = 0,
			 int a_second = 0,
			 int a_millisecond = 0);

		Time& Init(int a_hour = 0,
				   int a_minute = 0,
				   int a_second = 0,
				   int a_millisecond = 0);

		int Hour() const;
		Time& Hour(int a_hour);

		int Minute() const;
		Time& Minute(int a_minute);

		int Second() const;
		Time& Second(int a_second);

		int Millisecond() const;
		Time& Millisecond(int a_millisecond);

		MString ToString(const char* a_format = "%H:%M:%S") const;

		// 비교
		static int Compare(const Time& a_left,
						   const Time& a_right);
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
	public:
		Date m_date;
		Time m_time;

		// 현재시간
		static DateTime Now();

		DateTime(int a_year = 1,
				 int a_month = 1,
				 int a_day = 1,
				 int a_hour = 0,
				 int a_minute = 0,
				 int a_second = 0,
				 int a_millisecond = 0);

		DateTime& Init(int a_year = 1,
					   int a_month = 1,
					   int a_day = 1,
					   int a_hour = 0,
					   int a_minute = 0,
					   int a_second = 0,
					   int a_millisecond = 0);

		int Year() const;
		DateTime& Year(int a_year);

		int Month() const;
		DateTime& Month(int a_month);

		int Day() const;
		DateTime& Day(int a_day);

		int Hour() const;
		DateTime& Hour(int a_hour);

		int Minute() const;
		DateTime& Minute(int a_minute);

		int Second() const;
		DateTime& Second(int a_second);

		int Millisecond() const;
		DateTime& Millisecond(int a_millisecond);

		DayOfTheWeek GetDayOfTheWeek() const;

		MString ToString(const char* a_format = "%Y-%m-%d %H:%M:%S") const;


		// 비교
		static int Compare(const DateTime& a_left,
						   const DateTime& a_right);
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

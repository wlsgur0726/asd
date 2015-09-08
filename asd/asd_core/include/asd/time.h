#pragma once
#include "asd/asdbase.h"
#include "asd/exception.h"
#include <ctime>
#include <sqltypes.h>

namespace asd
{
	enum DayOfTheWeek
	{
		Sunday = 0,
		Monday,
		Tuesday,
		Wednesday,
		Thursday,
		Friday,
		Saturday
	};

	class Time
	{
		//              3                   2                   1                   0
		//            1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 
		//           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		// m_data[0] |                    year (23)                | mon(4)| day (5) |
		//           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		// m_data[1] |   hour (7)  |  minute (7) |  second (7) |   millisecond (11)  |
		//           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		int32_t m_data[2] = {0, 0};

	public:
		Time() asd_NoThrow;


		Time(IN int a_year,
			 IN int a_month,
			 IN int a_day = 1,
			 IN int a_hour = 0,
			 IN int a_minute = 0,
			 IN int a_second = 0,
			 IN int a_millisecond = 0) 
			asd_Throws(Exception);


		// 값 조회/셋팅
		void Init(IN int a_year,
				  IN int a_month,
				  IN int a_day = 1,
				  IN int a_hour = 0,
				  IN int a_minute = 0,
				  IN int a_second = 0,
				  IN int a_millisecond = 0)
			asd_Throws(Exception);

		int Year() const asd_NoThrow;
		void Year(IN int a_year)
			asd_Throws(Exception);

		int Month() const asd_NoThrow;
		void Month(IN int a_month)
			asd_Throws(Exception);

		int Day() const asd_NoThrow;
		void Day(IN int a_day)
			asd_Throws(Exception);

		int Hour() const asd_NoThrow;
		void Hour(IN int a_hour)
			asd_Throws(Exception);

		int Minute() const asd_NoThrow;
		void Minute(IN int a_minute)
			asd_Throws(Exception);

		int Second() const asd_NoThrow;
		void Second(IN int a_second)
			asd_Throws(Exception);

		int Millisecond() const asd_NoThrow;
		void Millisecond(IN int a_millisecond)
			asd_Throws(Exception);

		DayOfTheWeek DayOfTheWeek() const asd_NoThrow;

		MString ToString(const char* a_format = nullptr) const asd_NoThrow;


		// 비교
		static int Compare(IN const Time& a_left,
						   IN const Time& a_right) asd_NoThrow;

		asd_Define_CompareOperator(Compare, Time);


		// 다른 타입 지원
#define asd_Time_Declare_ConvertFunction(Type)				\
		Time& From(IN const Type& a_src)					\
			asd_Throws(Exception);							\
															\
		Time(IN const Type& a_src)							\
			asd_Throws(Exception);							\
															\
		Time& operator = (IN const Type& a_src)				\
			asd_Throws(Exception);							\
															\
		Type& To(OUT Type& a_dst) const asd_NoThrow;		\
															\
		operator Type() const asd_NoThrow;					\
		
		asd_Time_Declare_ConvertFunction(tm);
		asd_Time_Declare_ConvertFunction(SQL_TIMESTAMP_STRUCT);

	};
}

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
		Time() noexcept;


		Time(IN int a_year,
			 IN int a_month,
			 IN int a_day = 1,
			 IN int a_hour = 0,
			 IN int a_minute = 0,
			 IN int a_second = 0,
			 IN int a_millisecond = 0) 
			noexcept(false);


		// 값 조회/셋팅
		void Init(IN int a_year,
				  IN int a_month,
				  IN int a_day = 1,
				  IN int a_hour = 0,
				  IN int a_minute = 0,
				  IN int a_second = 0,
				  IN int a_millisecond = 0)
			noexcept(false);

		int Year() const noexcept;
		void Year(IN int a_year)
			noexcept(false);

		int Month() const noexcept;
		void Month(IN int a_month)
			noexcept(false);

		int Day() const noexcept;
		void Day(IN int a_day)
			noexcept(false);

		int Hour() const noexcept;
		void Hour(IN int a_hour)
			noexcept(false);

		int Minute() const noexcept;
		void Minute(IN int a_minute)
			noexcept(false);

		int Second() const noexcept;
		void Second(IN int a_second)
			noexcept(false);

		int Millisecond() const noexcept;
		void Millisecond(IN int a_millisecond)
			noexcept(false);

		DayOfTheWeek DayOfTheWeek() const noexcept;

		MString ToString(const char* a_format = nullptr) const noexcept;


		// 비교
		static int Compare(IN const Time& a_left,
						   IN const Time& a_right) noexcept;

		asd_Define_CompareOperator(Compare, Time);


		// 다른 타입 지원
#define asd_Time_Declare_ConvertFunction(Type)				\
		Time& From(IN const Type& a_src)					\
			noexcept(false);								\
															\
		Time(IN const Type& a_src)							\
			noexcept(false);								\
															\
		Time& operator = (IN const Type& a_src)				\
			noexcept(false);								\
															\
		Type& To(OUT Type& a_dst) const noexcept;			\
															\
		operator Type() const noexcept;						\
		
		asd_Time_Declare_ConvertFunction(tm);
		asd_Time_Declare_ConvertFunction(SQL_TIMESTAMP_STRUCT);

	};
}

#pragma once
#include "asdbase.h"
#include "sharedarray.h"
#include <string>

namespace asd
{
	struct SQL_TIMESTAMP_STRUCT;
	struct SQL_DATE_STRUCT;
	struct SQL_TIME_STRUCT;
	class Time;
	class Date;
	class DateTime;

	template<typename CHARTYPE>
	class BasicString;
	typedef BasicString<char>		MString;	// MultiByte String
	typedef BasicString<wchar_t>	WString;	// Wide String



	struct Caster
	{
#define asd_Caster_Declare_CastOperator(Type)								\
			virtual operator Type() const;						/* (1) */	\
																			\
			virtual operator Type*() const;						/* (2) */	\
																			\
			virtual operator std::shared_ptr<Type>() const;		/* (3) */	\
																			\
			virtual operator std::unique_ptr<Type>() const;		/* (4) */	\

		// (1) : 사본을 복사하여 리턴한다. 
		//       해당 파라메터가 만약 null값인 경우 NullDataException이 발생한다.
		//
		// (2) : thread local의 임시객체에 값을 복사하고 그 포인터를 리턴한다.
		//       해당 파라메터가 만약 null값인 경우  nullptr을 리턴한다.
		//       thread local 객체의 포인터이므로 리턴받은 직후 복사를 하던지 해야 할 것이다.
		//
		// (3),(4) : new로 생성한 객체에 값을 복사하고 스마트포인터에 붙여서 리턴한다.


		// 지원 타입 목록
#define asd_Caster_Declare_CastOperatorList													\
			asd_Caster_Declare_CastOperator(int8_t);					/*  TINYINT   */	\
			asd_Caster_Declare_CastOperator(short);						/*  SMALLINT  */	\
			asd_Caster_Declare_CastOperator(int);						/*  INT       */	\
			asd_Caster_Declare_CastOperator(int64_t);					/*  BIGINT    */	\
			asd_Caster_Declare_CastOperator(float);						/*  FLOAT     */	\
			asd_Caster_Declare_CastOperator(double);					/*  DOUBLE    */	\
			asd_Caster_Declare_CastOperator(bool);						/*  BIT       */	\
			asd_Caster_Declare_CastOperator(asd::SQL_TIMESTAMP_STRUCT);	/*  DATETIME  */	\
			asd_Caster_Declare_CastOperator(asd::SQL_DATE_STRUCT);		/*  DATE      */	\
			asd_Caster_Declare_CastOperator(asd::SQL_TIME_STRUCT);		/*  TIME      */	\
			asd_Caster_Declare_CastOperator(std::string);				/*  VARCHAR   */	\
			asd_Caster_Declare_CastOperator(std::wstring);				/*  WVARCHAR  */	\
			asd_Caster_Declare_CastOperator(MString);					/*  VARCHAR   */	\
			asd_Caster_Declare_CastOperator(WString);					/*  WVARCHAR  */	\
			asd_Caster_Declare_CastOperator(SharedVector<uint8_t>);		/*  BLOB      */	\
			asd_Caster_Declare_CastOperator(std::vector<uint8_t>);		/*  BLOB      */	\
			asd_Caster_Declare_CastOperator(tm);						/*  DATETIME  */	\
			asd_Caster_Declare_CastOperator(Date);						/*  DATE      */	\
			asd_Caster_Declare_CastOperator(Time);						/*  TIME      */	\
			asd_Caster_Declare_CastOperator(DateTime);					/*  DATETIME  */	\

		asd_Caster_Declare_CastOperatorList;

	};
}

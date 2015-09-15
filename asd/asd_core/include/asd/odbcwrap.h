#pragma once
#include "asd/asdbase.h"
#include "asd/time.h"
#include <functional>
#include <vector>

namespace asd
{
	struct DBDiagInfo
	{
		char m_state[6] = {0, 0, 0, 0, 0, 0};
		int m_nativeError = 0;
		MString m_message;

		MString ToString() const asd_NoThrow;
	};

	typedef std::vector<DBDiagInfo> DBDiagInfoList;



	class DBException : public Exception
	{
	public:
		DBDiagInfoList m_diagInfoList;
		DBException(IN const DBDiagInfoList& a_diagInfoList) asd_NoThrow;
	};



	class NullDataException : public Exception
	{
		using Exception::Exception;
	};



	// DB상의 타입
	enum SQLType
	{
		CHAR = 0,
		VARCHAR,
		WVARCHAR,
		BINARY,
		LONGVARBINARY,
		BIT,
		NUMERIC,
		DECIMAL,
		INTEGER,
		BIGINT,
		SMALLINT,
		TINYINT,
		FLOAT,
		DOUBLE,
		DATE,
		TIME,
		TIMESTAMP,
		UNKNOWN_TYPE
	};



	struct DBConnectionHandle;
	typedef std::shared_ptr<DBConnectionHandle> DBConnectionHandle_ptr;
	class DBConnection
	{
		friend class DBStatement;
		DBConnectionHandle_ptr m_handle;

	public:
		void Open(IN const char* a_constr)
			asd_Throws(DBException);

		void BeginTran()
			asd_Throws(DBException);

		void CommitTran()
			asd_Throws(DBException);

		void RollbackTran()
			asd_Throws(DBException);

		void Close()
			asd_Throws(DBException);

		virtual ~DBConnection()
			asd_NoThrow;
	};



	struct DBStatementHandle;
	typedef std::shared_ptr<DBStatementHandle> DBStatementHandle_ptr;
	class DBStatement
	{
		DBStatementHandle_ptr m_handle;

	public:
		DBStatement()
			asd_NoThrow;


		DBStatement(REF DBConnection& a_conHandle)
			asd_Throws(DBException);


		void Init(REF DBConnection& a_conHandle)
			asd_Throws(DBException);



		// 쿼리를 준비시킨다.
		void Prepare(IN const char* a_query)
			asd_Throws(DBException);



		// Prameter 관련 함수들 설명
		//   - 공통
		//     - 값을 넘길때 값이 담긴 변수의 포인터를 넘기는데 nullptr을 주면 DB에 null값을 전달한다.
		//     - BindInOutParam만 예외적으로 null 입력 여부를 판별하는 변수가 있다.
		//     - a_columnType  : DB 상의 타입. UNKNOWN_TYPE이면 Execute() 직전에 정보를 쿼리하여 적용한다.
		//                       (SQLBindParameter()의 5번째 인자)
		//     - a_columnSize  : Output 결과를 받을 버퍼의 크기 (byte 단위, SQLBindParameter()의 6번째 인자)
		//     - a_columnScale : 자릿수 관련 정보. (SQLBindParameter()의 7번째 인자)
		//
		//   - Set 계열 
		//     - 함수 호출 시 내부적으로 임시 버퍼를 만들고 그곳에 값을 복사한다.
		//     - Output 또는 InOut의 경우 Execute() 리턴 후 GetParam() 함수로 결과를 받을 수 있다.
		//
		//   - Bind 계열
		//     - 특정 변수를 바인드해두면 Execute() 호출 시점에 해당 변수의 값이 적용된다.
		//     - Output의 경우 Execute()가 리턴 된 후 해당 변수에 출력 결과가 담긴다.

		// 입력인자 셋팅
		template <typename T>
		void SetInParam(IN uint16_t a_paramNumber,
						IN const T& a_value,
						IN SQLType a_columnType = SQLType::UNKNOWN_TYPE,
						IN uint32_t a_columnSize = 0,
						IN uint16_t a_columnScale = 0)
			asd_Throws(DBException);

		// null값을 입력한다.
		template <typename T>
		void SetInParam_NullInput(IN uint16_t a_paramNumber,
								  IN SQLType a_columnType = SQLType::UNKNOWN_TYPE,
								  IN uint32_t a_columnSize = 0,
								  IN uint16_t a_columnScale = 0)
			asd_Throws(DBException);


		// 입력인자 바인딩
		template <typename T>
		void BindInParam(IN uint16_t a_paramNumber,
						 REF T* a_value,
						 IN SQLType a_columnType = SQLType::UNKNOWN_TYPE,
						 IN uint32_t a_columnSize = 0,
						 IN uint16_t a_columnScale = 0)
			asd_Throws(DBException);



		// 출력인자 셋팅
		template <typename T>
		void SetOutParam(IN uint16_t a_paramNumber,
						 IN SQLType a_columnType = SQLType::UNKNOWN_TYPE,
						 IN uint32_t a_columnSize = 0,
						 IN uint16_t a_columnScale = 0)
			asd_Throws(DBException);

		// 출력인자를 무시한다.
		template <typename T>
		void SetOutParam_NullInput(IN uint16_t a_paramNumber,
								   IN SQLType a_columnType = SQLType::UNKNOWN_TYPE,
								   IN uint32_t a_columnSize = 0,
								   IN uint16_t a_columnScale = 0)
			asd_Throws(DBException);


		// 출력인자 바인딩
		// a_varptr에 nullptr 입력 시 출력인자를 무시하겠다는 의미로 적용된다.
		template <typename T>
		void BindOutParam(IN uint16_t a_paramNumber,
						  REF T* a_varptr,
						  IN SQLType a_columnType = SQLType::UNKNOWN_TYPE,
						  IN uint32_t a_columnSize = 0,
						  IN uint16_t a_columnScale = 0)
			asd_Throws(DBException);



		// 입출력인자 셋팅
		template <typename T>
		void SetInOutParam(IN uint16_t a_paramNumber,
						   IN const T& a_value,
						   IN SQLType a_columnType = SQLType::UNKNOWN_TYPE,
						   IN uint32_t a_columnSize = 0,
						   IN uint16_t a_columnScale = 0)
			asd_Throws(DBException);

		// null값을 입력한다.
		template <typename T>
		void SetInOutParam_NullInput(IN uint16_t a_paramNumber,
									 IN SQLType a_columnType = SQLType::UNKNOWN_TYPE,
									 IN uint32_t a_columnSize = 0,
									 IN uint16_t a_columnScale = 0)
			asd_Throws(DBException);


		// 입출력인자 바인딩
		//  - a_nullInput이 true이면 입력값으로 null값을 전달한다.
		//  - a_nullInput이 false이면 a_ver에 담긴 값을 입력값으로 전달한다.
		template <typename T>
		void BindInOutParam(IN uint16_t a_paramNumber,
							REF T& a_var,
							IN bool a_nullInput,
							IN SQLType a_columnType = SQLType::UNKNOWN_TYPE,
							IN uint32_t a_columnSize = 0,
							IN uint16_t a_columnScale = 0)
			asd_Throws(DBException);



		// 매 Fetch마다 콜백되는 함수.
		// a_resultNumber와 a_recordNumber는 1부터 시작한다.
		typedef std::function<void(IN int a_resultNumber,
								   IN int a_recordNumber)> FetchCallback;
		
		// Prepare된 쿼리를 실행하고 Fetch한다.
		int64_t Execute(IN FetchCallback a_callback)
			asd_Throws(DBException);

		// a_query로 입력받은 쿼리를 바로 실행하고 Fetch한다.
		int64_t Execute(IN const char* a_query,
						IN FetchCallback a_callback)
			asd_Throws(DBException);



		// 셋팅 혹은 바인딩된 파라메터를 모두 제거
		void ClearParam()
			asd_Throws(DBException);



		// 핸들을 닫는다.
		void Close()
			asd_Throws(DBException);


		virtual ~DBStatement()
			asd_NoThrow;



		// 결과 조회.
		// FetchCallback 내에서, 혹은 Execute()리턴 후 사용한다.
		MString GetColumnName(IN uint16_t a_columnIndex)
			asd_Throws(Exception, DBException);


		uint16_t GetColumnCount() const asd_NoThrow;


		template <typename T>
		T* GetData(IN const char* a_columnName,
				   OUT T& a_return)
			asd_Throws(DBException);


		template <typename T>
		T* GetData(IN uint16_t a_columnIndex,
				   OUT T& a_return)
			asd_Throws(DBException);


		template <typename T>
		T* GetParam(IN uint16_t a_paramNumber,
					OUT T& a_return)
			asd_Throws(DBException);


		template <typename T>
		bool IsNullParam(IN uint16_t a_columnIndex)
			asd_Throws(Exception);


		template <typename T>
		bool IsNullParam(IN T* a_boundPtr)
			asd_Throws(Exception);


		struct Caster
		{
#define asd_DBStatement_Declare_CastOperator(Type)							\
			virtual operator Type()						/* (1) */			\
				asd_Throws(DBException, NullDataException);					\
																			\
			virtual operator Type*()					/* (2) */			\
				asd_Throws(DBException);									\
																			\
			virtual operator std::shared_ptr<Type>()	/* (3) */			\
				asd_Throws(DBException);									\
																			\
			virtual operator std::unique_ptr<Type>()	/* (4) */			\
				asd_Throws(DBException);									\

			// (1) : 사본을 복사하여 리턴한다. 
			//       해당 파라메터가 만약 null값인 경우 NullDataException이 발생한다.
			//
			// (2) : thread local의 임시객체에 값을 복사하고 그 포인터를 리턴한다.
			//       해당 파라메터가 만약 null값인 경우  nullptr을 리턴한다.
			//       thread local 객체의 포인터이므로 리턴받은 직후 복사를 하던지 해야 할 것이다.
			//
			// (3),(4) : new로 생성한 객체에 값을 복사하고 스마트포인터에 붙여서 리턴한다.


			// 지원 타입 목록
#define asd_DBStatement_Declare_CastOperatorList											\
			asd_DBStatement_Declare_CastOperator(int8_t);					/* TINYINT  */	\
			asd_DBStatement_Declare_CastOperator(short);					/* SMALLINT */	\
			asd_DBStatement_Declare_CastOperator(int);						/* INT      */	\
			asd_DBStatement_Declare_CastOperator(int64_t);					/* BIGINT   */	\
			asd_DBStatement_Declare_CastOperator(float);					/* FLOAT    */	\
			asd_DBStatement_Declare_CastOperator(double);					/* DOUBLE   */	\
			asd_DBStatement_Declare_CastOperator(bool);						/* BIT      */	\
			asd_DBStatement_Declare_CastOperator(SQL_TIMESTAMP_STRUCT);		/* DATETIME */	\
			asd_DBStatement_Declare_CastOperator(SQL_DATE_STRUCT);			/* DATE     */	\
			asd_DBStatement_Declare_CastOperator(SQL_TIME_STRUCT);			/* TIME     */	\
			asd_DBStatement_Declare_CastOperator(MString);					/* VARCHAR  */	\
			asd_DBStatement_Declare_CastOperator(WString);					/* WVARCHAR */	\
			asd_DBStatement_Declare_CastOperator(std::string);				/* VARCHAR  */	\
			asd_DBStatement_Declare_CastOperator(std::wstring);				/* WVARCHAR */	\
			asd_DBStatement_Declare_CastOperator(SharedArray<uint8_t>);		/* BLOB     */	\
			asd_DBStatement_Declare_CastOperator(std::vector<uint8_t>);		/* BLOB     */	\
			asd_DBStatement_Declare_CastOperator(tm);						/* DATETIME */	\
			asd_DBStatement_Declare_CastOperator(Time);						/* DATETIME */	\

			asd_DBStatement_Declare_CastOperatorList;
		};


		Caster& GetData(IN uint16_t a_columnIndex)
			asd_Throws(DBException);


		Caster& GetData(IN const char* a_columnName)
			asd_Throws(DBException);


		Caster& GetParam(IN uint16_t a_paramNumber)
			asd_Throws(DBException);
	};

}

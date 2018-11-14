#pragma once
#include "asdbase.h"
#include "datetime.h"
#include "caster.h"
#include <functional>
#include <vector>

namespace asd
{
	struct DBDiagInfo
	{
		char m_state[6] = {0, 0, 0, 0, 0, 0};
		int m_nativeError = 0;
		MString m_message;

		MString ToString() const;
	};

	typedef std::vector<DBDiagInfo> DBDiagInfoList;



	class DBException : public Exception
	{
	public:
		DBDiagInfoList m_diagInfoList;
		DBException(const DBDiagInfoList& a_diagInfoList,
					const char* a_lastFileName,
					int a_lastFileLine);
	};



	class NullDataException : public Exception
	{
		using Exception::Exception;
	};



	// DB상의 타입
	enum class SQLType : short
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

	template <typename T>
	struct ParamBinder
	{
		static const SQLType Type = SQLType::UNKNOWN_TYPE;
		static void* Buffer(T& a_var) { return &a_var; }
		int BufferLength(T& a_var) { return sizeof(a_var); }
	};

	template<>
	struct ParamBinder<int8_t> { static const SQLType Type = SQLType::TINYINT; };
	template<>
	struct ParamBinder<uint8_t> { static const SQLType Type = SQLType::TINYINT; };
	template<>
	struct ParamBinder<int16_t> { static const SQLType Type = SQLType::SMALLINT; };
	template<>
	struct ParamBinder<uint16_t> { static const SQLType Type = SQLType::SMALLINT; };
	template<>
	struct ParamBinder<int32_t> { static const SQLType Type = SQLType::INTEGER; };
	template<>
	struct ParamBinder<uint32_t> { static const SQLType Type = SQLType::INTEGER; };
	template<>
	struct ParamBinder<int64_t> { static const SQLType Type = SQLType::BIGINT; };
	template<>
	struct ParamBinder<uint64_t> { static const SQLType Type = SQLType::BIGINT; };
	template<>
	struct ParamBinder<float> { static const SQLType Type = SQLType::FLOAT; };
	template<>
	struct ParamBinder<double> { static const SQLType Type = SQLType::DOUBLE; };
	template<>
	struct ParamBinder<bool> { static const SQLType Type = SQLType::BIT; };
	template<>
	struct ParamBinder<std::string> { static const SQLType Type = SQLType::VARCHAR; };
	template<>
	struct ParamBinder<MString> { static const SQLType Type = SQLType::VARCHAR; };


	struct DBConnectionHandle;
	typedef std::shared_ptr<DBConnectionHandle> DBConnectionHandle_ptr;
	class DBConnection
	{
		friend class DBStatement;
		DBConnectionHandle_ptr m_handle;

	public:
		void Open(const char* a_constr);

		void BeginTran();

		void CommitTran();

		void RollbackTran();

		void Close();

		inline void Disconnect() { Close(); }

		virtual ~DBConnection();
	};



	struct DBStatementHandle;
	typedef std::shared_ptr<DBStatementHandle> DBStatementHandle_ptr;
	class DBStatement
	{
		DBStatementHandle_ptr m_handle;

	public:
		DBStatement();


		DBStatement(DBConnection& a_conHandle);


		void Init(DBConnection& a_conHandle);


		// 쿼리를 준비시킨다.
		void Prepare(const char* a_query);


		// Prameter 관련 함수들 설명
		//   - 공통
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
		void SetInParam(uint16_t a_paramNumber,
						const T& a_value,
						SQLType a_columnType = SQLType::UNKNOWN_TYPE,
						uint32_t a_columnSize = 0,
						uint16_t a_columnScale = 0);

		// null값을 입력한다.
		template <typename T>
		void SetInParam_NullInput(uint16_t a_paramNumber,
								  SQLType a_columnType = SQLType::UNKNOWN_TYPE,
								  uint32_t a_columnSize = 0,
								  uint16_t a_columnScale = 0);


		// 입력인자 바인딩
		template <typename T>
		void BindInParam(uint16_t a_paramNumber,
						 T* a_value,
						 SQLType a_columnType = SQLType::UNKNOWN_TYPE,
						 uint32_t a_columnSize = 0,
						 uint16_t a_columnScale = 0);



		// 출력인자 셋팅
		template <typename T>
		void SetOutParam(uint16_t a_paramNumber,
						 SQLType a_columnType = SQLType::UNKNOWN_TYPE,
						 uint32_t a_columnSize = 0,
						 uint16_t a_columnScale = 0);

		// 출력인자를 무시한다.
		template <typename T>
		void SetOutParam_NullInput(uint16_t a_paramNumber,
								   SQLType a_columnType = SQLType::UNKNOWN_TYPE,
								   uint32_t a_columnSize = 0,
								   uint16_t a_columnScale = 0);


		// 출력인자 바인딩
		// a_varptr에 nullptr 입력 시 출력인자를 무시하겠다는 의미로 적용된다.
		template <typename T>
		void BindOutParam(uint16_t a_paramNumber,
						  T* a_varptr,
						  SQLType a_columnType = SQLType::UNKNOWN_TYPE,
						  uint32_t a_columnSize = 0,
						  uint16_t a_columnScale = 0);



		// 입출력인자 셋팅
		template <typename T>
		void SetInOutParam(uint16_t a_paramNumber,
						   const T& a_value,
						   SQLType a_columnType = SQLType::UNKNOWN_TYPE,
						   uint32_t a_columnSize = 0,
						   uint16_t a_columnScale = 0);

		// null값을 입력한다.
		template <typename T>
		void SetInOutParam_NullInput(uint16_t a_paramNumber,
									 SQLType a_columnType = SQLType::UNKNOWN_TYPE,
									 uint32_t a_columnSize = 0,
									 uint16_t a_columnScale = 0);


		// 입출력인자 바인딩
		//  - a_nullInput이 true이면 입력값으로 null값을 전달한다.
		//  - a_nullInput이 false이면 a_ver에 담긴 값을 입력값으로 전달한다.
		template <typename T>
		void BindInOutParam(uint16_t a_paramNumber,
							T& a_var,
							bool a_nullInput,
							SQLType a_columnType = SQLType::UNKNOWN_TYPE,
							uint32_t a_columnSize = 0,
							uint16_t a_columnScale = 0);



		// 매 Fetch마다 콜백되는 함수.
		// a_resultNumber와 a_recordNumber는 1부터 시작한다.
		typedef std::function<void(int a_resultNumber,
								   int a_recordNumber)> FetchCallback;
		
		// Prepare된 쿼리를 실행하고 Fetch한다.
		int64_t Execute(FetchCallback a_callback);

		// a_query로 입력받은 쿼리를 바로 실행하고 Fetch한다.
		int64_t Execute(const char* a_query,
						FetchCallback a_callback);



		// 셋팅 혹은 바인딩된 파라메터를 모두 제거
		void ClearParam();


		// 핸들을 닫는다.
		void Close();


		virtual ~DBStatement();


		// 결과 조회.
		// FetchCallback 내에서, 혹은 Execute()리턴 후 사용한다.
		MString GetColumnName(uint16_t a_columnIndex);


		uint16_t GetColumnCount() const;


		template <typename T>
		T* GetData(const char* a_columnName,
				   T& a_return /*Out*/);


		template <typename T>
		T* GetData(uint16_t a_columnIndex,
				   T& a_return /*Out*/);


		template <typename T>
		T* GetParam(uint16_t a_paramNumber,
					T& a_return /*Out*/);


		template <typename T>
		bool IsNullParam(uint16_t a_columnIndex);


		template <typename T>
		bool IsNullParam(T* a_boundPtr);


		Caster& GetData(uint16_t a_columnIndex);


		Caster& GetData(const char* a_columnName);


		Caster& GetParam(uint16_t a_paramNumber);
	};
}

﻿#include "stdafx.h"
#include "asd/odbcwrap.h"
#include "asd/util.h"
#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>
#include <unordered_map>


// sqltypes.h의 타입들과 asd에서 선언한 타입이 안전하게 동일 취급 가능한지 체크
#define asd_Check_Size(T) static_assert(sizeof(::T) == sizeof(asd::T), "different size")
#define asd_Check_Member(T, M) static_assert(offsetof(::T, M) == offsetof(asd::T, M), "different offset");

asd_Check_Size(SQL_TIMESTAMP_STRUCT);
asd_Check_Member(SQL_TIMESTAMP_STRUCT, year);
asd_Check_Member(SQL_TIMESTAMP_STRUCT, month);
asd_Check_Member(SQL_TIMESTAMP_STRUCT, day);
asd_Check_Member(SQL_TIMESTAMP_STRUCT, hour);
asd_Check_Member(SQL_TIMESTAMP_STRUCT, minute);
asd_Check_Member(SQL_TIMESTAMP_STRUCT, second);
asd_Check_Member(SQL_TIMESTAMP_STRUCT, fraction);

asd_Check_Size(SQL_DATE_STRUCT);
asd_Check_Member(SQL_DATE_STRUCT, year);
asd_Check_Member(SQL_DATE_STRUCT, month);
asd_Check_Member(SQL_DATE_STRUCT, day);

asd_Check_Size(SQL_TIME_STRUCT);
asd_Check_Member(SQL_TIME_STRUCT, hour);
asd_Check_Member(SQL_TIME_STRUCT, minute);
asd_Check_Member(SQL_TIME_STRUCT, second);



#define asd_CheckError(...) CheckError(__FILE__, __LINE__, __VA_ARGS__)

namespace asd
{
	MString DBDiagInfo::ToString() const
	{
		return MString::Format("State={},NativeError={},Message={}",
							   m_state,
							   m_nativeError,
							   m_message);
	}



	DBException::DBException(const DBDiagInfoList& a_diagInfoList,
							 const char* a_lastFileName,
							 int a_lastFileLine)
	{
		m_what.Format("DBException({}:{})", a_lastFileName, a_lastFileLine);
		int index = 1;
		for (auto& diagInfo : a_diagInfoList) {
			m_diagInfoList.emplace_back(diagInfo);
			m_what << "\n [" << (index++) << "] " << diagInfo.ToString();
		}
	}



	struct TypeTableInitializer
	{
		SQLSMALLINT m_enumToCode[(short)SQLType::UNKNOWN_TYPE + 1];
		std::unordered_map<SQLSMALLINT, SQLType> m_codeToEnum;

		TypeTableInitializer()
		{
#define asd_InitTypeTable(ENUM)														\
			{																		\
				const SQLSMALLINT TypeCode = SQL_ ## ENUM;							\
				asd_DAssert(m_codeToEnum.find(TypeCode) == m_codeToEnum.end());		\
				m_codeToEnum[TypeCode] = SQLType::ENUM;								\
				m_enumToCode[(short)SQLType::ENUM] = TypeCode;						\
			}																		\

			asd_InitTypeTable(CHAR);
			asd_InitTypeTable(VARCHAR);
			asd_InitTypeTable(WVARCHAR);
			asd_InitTypeTable(BINARY);
			asd_InitTypeTable(LONGVARBINARY);
			asd_InitTypeTable(BIT);
			asd_InitTypeTable(NUMERIC);
			asd_InitTypeTable(DECIMAL);
			asd_InitTypeTable(INTEGER);
			asd_InitTypeTable(BIGINT);
			asd_InitTypeTable(SMALLINT);
			asd_InitTypeTable(TINYINT);
			asd_InitTypeTable(FLOAT);
			asd_InitTypeTable(DOUBLE);
			asd_InitTypeTable(DATE);
			asd_InitTypeTable(TIME);
			asd_InitTypeTable(TIMESTAMP);
			asd_InitTypeTable(UNKNOWN_TYPE);
		}

		inline SQLSMALLINT operator[] (SQLType a_enum) const
		{
			return m_enumToCode[(short)a_enum];
		}

		inline SQLType operator[] (SQLSMALLINT a_code) const
		{
			auto it = m_codeToEnum.find(a_code);
			if (it == m_codeToEnum.end())
				asd_RaiseException("SQLTypeCode({}) is not suported", a_code);
			return it->second;
		}
	};
	const TypeTableInitializer TypeTable;



	/***** OdbcHandle **************************************************************************/

	struct OdbcHandle
	{
		SQLHANDLE m_handle = SQL_NULL_HANDLE;
		SQLSMALLINT m_handleType = 0;
		DBDiagInfoList m_diagInfoList;


		OdbcHandle()
		{
		}


		OdbcHandle(OdbcHandle&& a_rval)
		{
			*this = std::move(a_rval);
		}


		virtual OdbcHandle& operator=(OdbcHandle&& a_rval)
		{
			m_handle = a_rval.m_handle;
			m_handleType = a_rval.m_handleType;
			m_diagInfoList = std::move(a_rval.m_diagInfoList);
			a_rval.m_handle = SQL_NULL_HANDLE;
			return *this;
		}


		void Init(SQLSMALLINT a_handleType,
				  OdbcHandle* a_inputHandle)
		{
			Close();

			// 유효한 파라메터인지 검사
			SQLHANDLE inputHandle;
			if (a_inputHandle == nullptr) {
				asd_DAssert(a_handleType == SQL_HANDLE_ENV);
				inputHandle = SQL_NULL_HANDLE;
			}
			else {
				asd_DAssert(a_handleType != SQL_HANDLE_ENV);
				if (a_handleType == SQL_HANDLE_DBC)
					asd_DAssert(a_inputHandle->m_handleType == SQL_HANDLE_ENV);
				else {
					asd_DAssert(a_handleType==SQL_HANDLE_DESC || a_handleType==SQL_HANDLE_STMT);
					asd_DAssert(a_inputHandle->m_handleType == SQL_HANDLE_DBC);
				}
				inputHandle = a_inputHandle->m_handle;
			}

			// 할당
			SQLRETURN r = ::SQLAllocHandle(a_handleType,
										   inputHandle,
										   &m_handle);
			asd_CheckError(r);

			// 환경핸들인 경우 환경설정
			if (a_handleType == SQL_HANDLE_ENV) {
				r = ::SQLSetEnvAttr(m_handle,
									SQL_ATTR_ODBC_VERSION,
									(SQLPOINTER)SQL_OV_ODBC3,
									SQL_IS_INTEGER);
				asd_CheckError(r);
			}

			// 마무리
			m_handleType = a_handleType;
			asd_DAssert(m_handle != SQL_NULL_HANDLE);
		}


		// 에러처리 루틴. 리턴값 : true=처리완료, false=미처리
		typedef std::function<bool(SQLRETURN,
								   const DBDiagInfo&)> ErrProc;

		static bool IgnoreWarning(SQLRETURN a_retval,
								  const DBDiagInfo&)
		{
			if (a_retval == SQL_SUCCESS_WITH_INFO)
				return true;
			return false;
		};

		// 에러 여부를 검사 및 처리하고 미처리된 에러가 남은 경우 exception을 발생시킨다.
		void CheckError(const char* a_lastFileName,
						int a_lastFileLine,
						SQLRETURN a_retval,
						ErrProc a_errproc = IgnoreWarning)
		{
			if (a_retval != SQL_SUCCESS) {
				SetLastErrorList();
				if (a_errproc != nullptr) {
					for (auto it=m_diagInfoList.begin(); it!=m_diagInfoList.end(); ) {
						if (a_errproc(a_retval, *it))
							it = m_diagInfoList.erase(it);
						else
							++it;
					}
				}
				if (m_diagInfoList.size() > 0)
					throw DBException(m_diagInfoList, a_lastFileName, a_lastFileLine);
			}
		}


		void SetLastErrorList()
		{
			m_diagInfoList.clear();

			for (int i=1; ; ++i) {
				SQLCHAR state[6] = {0, 0, 0, 0, 0, 0};
				SQLINTEGER nativeError;
				SQLCHAR errMsg[SQL_MAX_MESSAGE_LENGTH];
				SQLSMALLINT errMsgLen;

				auto r = SQLGetDiagRec(m_handleType,
									   m_handle,
									   i,
									   state,
									   &nativeError,
									   errMsg,
									   sizeof(errMsg),
									   &errMsgLen);
				if (r == SQL_NO_DATA)
					break;
				else if (r!=SQL_SUCCESS && r!=SQL_SUCCESS_WITH_INFO) {
					asd_RaiseException("fail SQLGetDiagRec(), return:{}", r);
				}

				DBDiagInfo diagInfo;
				errMsg[errMsgLen] = '\0';
				std::memcpy(diagInfo.m_state,
							state,
							sizeof(diagInfo.m_state));
				diagInfo.m_nativeError = nativeError;
				diagInfo.m_message = (const char*)errMsg;
				m_diagInfoList.emplace_back(diagInfo);
			}
		}


		void Close()
		{
			if (m_handle != SQL_NULL_HANDLE) {
				asd_CheckError(::SQLFreeHandle(m_handleType, m_handle));
				m_handle = SQL_NULL_HANDLE;
				m_handleType = 0;
				m_diagInfoList.clear();
			}
		}


		virtual ~OdbcHandle()
		{
			asd_BeginDestructor();
			Close();
			asd_EndDestructor();
		}


	private:
		OdbcHandle(const OdbcHandle&) = delete;
		OdbcHandle& operator=(const OdbcHandle&) = delete;

	};




	/***** DBConnection **************************************************************************/

	struct DBConnectionHandle : public OdbcHandle
	{
		OdbcHandle m_envHandle;
		bool m_connected = false;
		bool m_autoCommit = true;

		void Init()
		{
			CloseConnectoin();
			m_envHandle.Init(SQL_HANDLE_ENV, nullptr);
			OdbcHandle::Init(SQL_HANDLE_DBC, &m_envHandle);
		}


		void Open(const char* a_constr)
		{
			Init();
			SQLSMALLINT t;
			SQLRETURN r = ::SQLDriverConnect(m_handle,
											 nullptr,
											 (SQLCHAR*)a_constr,
											 SQL_NTS,
											 nullptr,
											 0,
											 &t,
											 SQL_DRIVER_NOPROMPT);
			asd_CheckError(r);
			m_connected = true;
		}


		void SetAutoCommit(bool a_auto)
		{
#pragma warning(disable:4312)
			SQLRETURN r = ::SQLSetConnectAttr(m_handle,
											  SQL_ATTR_AUTOCOMMIT,
											  reinterpret_cast<SQLPOINTER>(a_auto ? SQL_AUTOCOMMIT_ON : SQL_AUTOCOMMIT_OFF),
											  SQL_IS_INTEGER);
#pragma warning(default:4312)
			asd_CheckError(r);
			m_autoCommit = a_auto;
		}


		void EndTran(bool a_commit)
		{
			asd_CheckError(::SQLEndTran(m_handleType,
										m_handle,
										a_commit ? SQL_COMMIT : SQL_ROLLBACK));
			SetAutoCommit(true);
		}


		void CloseConnectoin()
		{
			if (m_autoCommit == false)
				EndTran(false);

			if (m_connected) {
				asd_CheckError(::SQLDisconnect(m_handle));
				m_connected = false;
			}
			Close();
			m_envHandle.Close();
		}


		virtual ~DBConnectionHandle()
		{
			asd_BeginDestructor();
			CloseConnectoin();
			asd_EndDestructor();
		}
	};



	void DBConnection::Open(const char* a_constr)
	{
		m_handle.reset(new DBConnectionHandle);
		m_handle->Open(a_constr);
	}


	void DBConnection::BeginTran()
	{
		m_handle->SetAutoCommit(false);
	}


	void DBConnection::CommitTran()
	{
		m_handle->EndTran(true);
	}


	void DBConnection::RollbackTran()
	{
		m_handle->EndTran(false);
	}


	void DBConnection::Close()
	{
		if (m_handle != nullptr) {
			m_handle->CloseConnectoin();
			m_handle = nullptr;
		}
	}


	DBConnection::~DBConnection()
	{
		asd_BeginDestructor();
		Close();
		asd_EndDestructor();
	}



	/***** DBStatement **************************************************************************/

	struct DBStatementHandle : public OdbcHandle
	{
		struct Parameter
		{
			virtual void GetBindInfo(DBStatementHandle& a_requester,
									 SQLUSMALLINT a_paramNumber,
									 SQLSMALLINT& a_direction /*Out*/,
									 SQLSMALLINT& a_appType /*Out*/,
									 SQLSMALLINT& a_dbType /*Out*/,
									 SQLULEN& a_bufferSize /*Out*/,
									 SQLSMALLINT& a_columnScale /*Out*/,
									 void*& a_buffer /*Out*/,
									 SQLLEN*& a_indicator /*Out*/) = 0;

			virtual void AfterFetch() {}

			virtual void GetBoundObj(void*& a_objPtr /*Out*/,
									 SQLLEN& a_indicator /*Out*/) const = 0;

			virtual ~Parameter() {}

		};
		typedef std::shared_ptr<Parameter> Parameter_ptr;

		DBConnectionHandle_ptr m_conHandle;

		// 인덱스:컬럼명 맵
		std::vector<MString> m_columnNameList;

		// 컬럼명:인덱스 맵
		std::unordered_map<MString,
						   SQLSMALLINT,
						   MString::Hash_CaseInsensitive,
						   MString::EqualTo_CaseInsensitive> m_columnIndexMap;

		// Prepare문 또는 Stored Procedure의 Parameter
		std::unordered_map<SQLUSMALLINT, Parameter_ptr> m_paramMap_byParamNum;
		std::unordered_map<void*, Parameter_ptr> m_paramMap_byBoundPtr;


		void Init(DBConnectionHandle_ptr a_conHandle)
		{
			CloseStatement();

			asd_DAssert(a_conHandle != nullptr);
			m_conHandle = a_conHandle;
			OdbcHandle::Init(SQL_HANDLE_STMT, m_conHandle.get());
		}


		void Prepare(const char* a_query)
		{
			m_paramMap_byParamNum.clear();
			m_paramMap_byBoundPtr.clear();
			asd_CheckError(::SQLPrepare(m_handle,
										(SQLCHAR*)a_query,
										SQL_NTS));
		}


		void GetParamInfo(SQLUSMALLINT a_paramNumber,
						  SQLSMALLINT* a_columnType /*Out*/,
						  SQLULEN* a_columnSize /*Out*/,
						  SQLSMALLINT* a_scale /*Out*/,
						  SQLSMALLINT* a_nullable /*Out*/)
		{
			asd_CheckError(::SQLDescribeParam(m_handle,
											  a_paramNumber,
											  a_columnType,
											  a_columnSize,
											  a_scale,
											  a_nullable));

			if (a_columnSize != nullptr) {
				asd_DAssert(a_columnType != nullptr);
				if (*a_columnType==SQL_WCHAR || *a_columnType==SQL_WVARCHAR)
					*a_columnSize *= 4;
			}
		}


		void PrepareParameter(SQLUSMALLINT a_paramNumber,
							  void* a_bindKey,
							  Parameter_ptr& a_param)
		{
			m_paramMap_byParamNum[a_paramNumber] = a_param;
			if (a_bindKey != nullptr)
				m_paramMap_byBoundPtr[a_bindKey] = a_param;
		}


		void BindParameter()
		{
			for (auto& param : m_paramMap_byParamNum) {
				SQLSMALLINT direction;
				SQLSMALLINT appType;
				SQLSMALLINT dbType;
				SQLULEN bufferSize;
				SQLSMALLINT columnsScale;
				void* buffer;
				SQLLEN* indicator;

				param.second->GetBindInfo(*this,
										  param.first,
										  direction,
										  appType,
										  dbType,
										  bufferSize,
										  columnsScale,
										  buffer,
										  indicator);

				asd_CheckError(::SQLBindParameter(m_handle,
												  param.first,
												  direction,
												  appType,
												  dbType,
												  bufferSize,
												  columnsScale,
												  buffer,
												  bufferSize,
												  indicator));
			}
		}


		SQLLEN Execute(const char* a_query,
					   DBStatement::FetchCallback a_callback)
		{
			// 1. BindParameter, Execute
			if (a_query == nullptr) {
				BindParameter();
				asd_CheckError(::SQLExecute(m_handle));
			}
			else {
				if (m_paramMap_byParamNum.size() > 0)
					BindParameter();
				asd_CheckError(::SQLExecDirect(m_handle,
											   (SQLCHAR*)a_query,
											   SQL_NTS));
			}

			SQLLEN rows;
			asd_CheckError(::SQLRowCount(m_handle, &rows));

			// 2. Fetch Loop
			int result = 1;
			do {
				// 2-1. 컬럼명과 인덱스를 매핑한다.
				{
					// 2-1-1. 컬럼의 개수를 구한다.
					SQLSMALLINT colCount = -1;
					asd_CheckError(::SQLNumResultCols(m_handle, &colCount));
					asd_DAssert(colCount >= 0);

					// 2-1-2. 컬럼명 목록을 초기화한다.
					m_columnIndexMap.clear();
					m_columnNameList.clear();
					m_columnNameList.resize(colCount);

					// 2-1-3. 컬럼을 순회하면서 컬럼명과 인덱스를 매핑한다.
					for (SQLUSMALLINT i=1; i<=colCount; ++i) {
						MString colName = GetColumnName(i);
						if (colName.length() > 0)
							m_columnIndexMap[colName] = i;
					}
				}

				// 2-2. 일괄적으로 Fetch를 수행하면서 FetchCallback을 호출해준다.
				int record = 1;
				while (true) {
					SQLRETURN r = ::SQLFetch(m_handle);
					if (r == SQL_NO_DATA_FOUND)
						break; // 현재 Result에서 모든 Record를 Fetch했음.

					bool invalid = false;
					asd_CheckError(r, [&invalid](SQLRETURN a_ret,
												 const DBDiagInfo& a_err)
					{
						if (OdbcHandle::IgnoreWarning(a_ret, a_err))
							return true;

						if (0 == asd::strcmp("24000", a_err.m_state, false)) {
							// 커서가 열리지 않은 경우
							asd_DAssert(invalid == false);
							invalid = true;
							return true;
						}
						return false;
					});
					if (invalid)
						goto ExitFetchLoop;

					if (a_callback != nullptr)
						a_callback(result, record);
					++record;
				}
				++result;
			} while (MoreResult());

		ExitFetchLoop:
			// 3. 바인드했던 출력인자들에 대한 후속처리를 수행.
			for (auto& param : m_paramMap_byParamNum)
				param.second->AfterFetch();
			return rows;
		}


		bool MoreResult()
		{
			asd_DAssert(m_conHandle != nullptr);
			SQLRETURN r = ::SQLMoreResults(m_handle);
			if (r == SQL_NO_DATA)
				return false;

			asd_CheckError(r);
			return true;
		}


		MString GetColumnName(SQLUSMALLINT a_colIndex)
		{
			if (a_colIndex > m_columnNameList.size())
				asd_RaiseException("column[{}] does not exist", a_colIndex);

			auto& ret = m_columnNameList[a_colIndex - 1];
			if (ret.length() == 0) {
				SQLCHAR buf[SQL_MAX_MESSAGE_LENGTH];
				SQLSMALLINT retlen;
				asd_CheckError(::SQLColAttribute(m_handle,
												 a_colIndex,
												 SQL_DESC_NAME,
												 (SQLPOINTER)buf,
												 sizeof(buf),
												 &retlen,
												 nullptr));
				ret.append((const char*)buf, retlen);
			}
			return ret;
		}


		SQLUSMALLINT GetColumnIndex(const char* a_colName) const
		{
			const auto it = m_columnIndexMap.find(a_colName);
			if (it == m_columnIndexMap.end())
				return std::numeric_limits<SQLUSMALLINT>::max();
			return it->second;
		}


		SQLLEN GetData(SQLUSMALLINT a_colIndex,
					   SQLSMALLINT a_returnType,
					   void* a_buf /*Out*/,
					   SQLLEN a_bufLen)
		{
			SQLLEN indicator = 0;
			asd_CheckError(::SQLGetData(m_handle,
										a_colIndex,
										a_returnType,
										a_buf,
										a_bufLen,
										&indicator));
			return indicator;
		}


		void GetParam(SQLUSMALLINT a_paramNumber,
					  void*& a_bufPtr /*Out*/,
					  SQLLEN& a_indicator /*Out*/)
		{
			auto it = m_paramMap_byParamNum.find(a_paramNumber);
			if (it == m_paramMap_byParamNum.end()) {
				asd_RaiseException("parameter[{}] does not exist", a_paramNumber);
			}
			it->second->GetBoundObj(a_bufPtr, a_indicator);
		}


		void GetParam(void* a_key,
					  void*& a_bufPtr /*Out*/,
					  SQLLEN& a_indicator /*Out*/)
		{
			auto it = m_paramMap_byBoundPtr.find(a_key);
			if (it == m_paramMap_byBoundPtr.end()) {
				asd_RaiseException("parameter[{}] does not exist", a_key);
			}
			it->second->GetBoundObj(a_bufPtr, a_indicator);
		}


		void ClearParam()
		{
			asd_CheckError(::SQLFreeStmt(m_handle, SQL_RESET_PARAMS));
			m_paramMap_byParamNum.clear();
			m_paramMap_byBoundPtr.clear();
		}


		void CloseStatement()
		{
			auto fin = FinallyTask([this]()
			{
				Close();
			});

			if (m_conHandle != nullptr) {
				asd_CheckError(::SQLCloseCursor(m_handle),
							   [](SQLRETURN a_ret,
								  const DBDiagInfo& a_err)
				{
					if (OdbcHandle::IgnoreWarning(a_ret, a_err))
						return true;

					// 커서가 열리지 않았는데 닫으려는 경우
					if (asd::strcmp("24000", a_err.m_state, false) == 0)
						return true;
					return false;
				});
				ClearParam();
				m_conHandle = DBConnectionHandle_ptr();
			}
		}


		virtual ~DBStatementHandle()
		{
			asd_BeginDestructor();
			CloseStatement();
			asd_EndDestructor();
		}
	};



	DBStatement::DBStatement()
	{
	}


	DBStatement::DBStatement(DBConnection& a_conHandle)
	{
		Init(a_conHandle);
	}


	void DBStatement::Init(DBConnection& a_conHandle)
	{
		m_handle.reset(new DBStatementHandle);
		m_handle->Init(a_conHandle.m_handle);
	}


	void DBStatement::Prepare(const char* a_query)
	{
		asd_DAssert(m_handle != nullptr);
		m_handle->Prepare(a_query);
	}


	int64_t DBStatement::Execute(FetchCallback a_callback)
	{
		asd_DAssert(m_handle != nullptr);
		return m_handle->Execute(nullptr, a_callback);
	}


	int64_t DBStatement::Execute(const char* a_query,
								 FetchCallback a_callback)
	{
		asd_DAssert(m_handle != nullptr);
		return m_handle->Execute(a_query, a_callback);
	}


	void DBStatement::ClearParam()
	{
		asd_DAssert(m_handle != nullptr);
		m_handle->ClearParam();
	}


	MString DBStatement::GetColumnName(uint16_t a_columnIndex)
	{
		asd_DAssert(m_handle != nullptr);
		return m_handle->GetColumnName(a_columnIndex);
	}


	uint16_t DBStatement::GetColumnCount() const
	{
		asd_DAssert(m_handle != nullptr);
		asd_DAssert(m_handle->m_columnNameList.size() <= std::numeric_limits<uint16_t>::max());
		return (uint16_t)m_handle->m_columnNameList.size();
	}


	void DBStatement::Close()
	{
		if (m_handle != nullptr)
			m_handle->CloseStatement();
	}


	DBStatement::~DBStatement()
	{
		asd_BeginDestructor();
		Close();
		asd_EndDestructor();
	}



	thread_local Caster* t_lastCaster;

	struct Caster_GetData : public Caster
	{
		DBStatementHandle* m_handle;
		uint16_t m_index;
		asd_Caster_Declare_CastOperatorList;
	};

	Caster& DBStatement::GetData(uint16_t a_columnIndex)
	{
		thread_local Caster_GetData t_caster;
		t_caster.m_handle = m_handle.get();
		t_caster.m_index = a_columnIndex;
		t_lastCaster = &t_caster;
		return t_caster;
	}

	Caster& DBStatement::GetData(const char* a_columnName)
	{
		return GetData(m_handle->GetColumnIndex(a_columnName));
	}



	struct Caster_GetParam : public Caster
	{
		DBStatementHandle* m_handle;
		uint16_t m_index;
		asd_Caster_Declare_CastOperatorList;
	};

	Caster& DBStatement::GetParam(uint16_t a_paramNumber)
	{
		thread_local Caster_GetParam t_caster;
		t_caster.m_handle = m_handle.get();
		t_caster.m_index = a_paramNumber;
		t_lastCaster = &t_caster;
		return t_caster;
	}



	/***** Templates **************************************************************************/

	// Convert Direction
	const bool Left_To_Right = true;
	const bool Right_To_Left = false;
	inline constexpr bool Is_Left_To_Right(bool a_direction)
	{
		return a_direction == Left_To_Right;
	}
	inline constexpr bool Is_Right_To_Left(bool a_direction)
	{
		return a_direction == Right_To_Left;
	}


	// Type별로 Specialize를 한번만 하기 위해
	// a_direction를 런타임 인자로 둔다.
	template <typename Type>
	inline void ConvertStream(bool a_direction,
							  Type& a_data,
							  void*& a_buf,
							  SQLLEN& a_len);

	template <typename Left, typename Right>
	inline void ConvertData(bool a_direction,
							Left& a_left,
							Right& a_right);


	// 아래 두가지 용도로 랩핑해준다.
	// - const body에서 사용
	// - GCC에서 발생하는 'specialization of ... after instantiation' 에러를 회피
	template <typename Src, typename Dst>
	inline void Call_ConvertData(const Src& a_src,
								 Dst& a_dst /*Out*/)
	{
		ConvertData<Src, Dst>(Left_To_Right, (Src&)a_src, a_dst);
	}



	// GetData() tempalte
	template <typename ReturnType>
	inline constexpr SQLSMALLINT GetTypecode()
	{
		return SQL_C_DEFAULT;
	}

	template <typename ReturnType>
	inline ReturnType* GetData_Internal(DBStatementHandle* a_handle,
										SQLUSMALLINT a_index,
										ReturnType& a_return)
	{
		const SQLLEN BufSize = 1024;
		uint8_t buf[BufSize];
		SQLLEN indicator;

		bool loop = true;
		while (loop) {
			indicator = a_handle->GetData(a_index,
										  GetTypecode<ReturnType>(),
										  buf,
										  BufSize);
			if (indicator == SQL_NULL_DATA)
				return nullptr;

			SQLLEN addLen;
			if (indicator == SQL_NO_TOTAL)
				addLen = BufSize;
			else {
				asd_DAssert(indicator > 0);
				addLen = std::min(indicator, BufSize);
				loop = indicator > BufSize;
			}
			void* p = buf;
			ConvertStream<ReturnType>(Right_To_Left, a_return, p, addLen);
		}
		return &a_return;
	}

#define asd_Define_Caster_GetData_Ptr(Type, PtrClass)										\
	Caster::operator PtrClass<Type>() const													\
	{																						\
		/* unused function, avoid build error */											\
		asd_DAssert(false);																	\
		return t_lastCaster->operator PtrClass<Type>();										\
	}																						\
																							\
	Caster_GetData::operator PtrClass<Type>() const											\
	{																						\
		PtrClass<Type> ret;																	\
		Type temp;																			\
		if (GetData_Internal<Type>(m_handle, m_index, temp) != nullptr)						\
			ret = PtrClass<Type>(new Type(std::move(temp)));								\
		return ret;																			\
	}																						\

#define asd_Define_Caster_GetData(Type)														\
	Caster::operator Type() const															\
	{																						\
		/* unused function, avoid build error */											\
		asd_DAssert(false);																	\
		return t_lastCaster->operator Type();												\
	}																						\
																							\
	Caster::operator Type*() const															\
	{																						\
		/* unused function, avoid build error */											\
		asd_DAssert(false);																	\
		return t_lastCaster->operator Type*();												\
	}																						\
																							\
	Caster_GetData::operator Type() const													\
	{																						\
		Type ret;																			\
		if (GetData_Internal<Type>(m_handle, m_index, ret) == nullptr)						\
			throw NullDataException("is null data.");										\
		return ret;																			\
	}																						\
																							\
	Caster_GetData::operator Type*() const													\
	{																						\
		thread_local Type t_temp;															\
		t_temp = Type();																	\
		return GetData_Internal<Type>(m_handle, m_index, t_temp);							\
	}																						\
																							\
	asd_Define_Caster_GetData_Ptr(Type, std::shared_ptr);									\
	asd_Define_Caster_GetData_Ptr(Type, std::unique_ptr);									\

#define asd_Define_GetData(ReturnType)														\
	asd_Define_Caster_GetData(ReturnType);													\
																							\
	template <>																				\
	ReturnType* DBStatement::GetData<ReturnType>(const char* a_columnName,					\
												 ReturnType& a_return /*Out*/)				\
	{																						\
		auto index = m_handle->GetColumnIndex(a_columnName);								\
		return GetData_Internal<ReturnType>(m_handle.get(),									\
											index,											\
											a_return);										\
	}																						\
																							\
																							\
	template <>																				\
	ReturnType* DBStatement::GetData<ReturnType>(uint16_t a_enum,							\
												 ReturnType& a_return /*Out*/)				\
	{																						\
		return GetData_Internal<ReturnType>(m_handle.get(),									\
											a_enum,											\
											a_return);										\
	}																						\

#define asd_Define_GetData_SetTypecode(ReturnType, Target_C_Type)							\
	template<>																				\
	inline constexpr SQLSMALLINT GetTypecode<ReturnType>()									\
	{																						\
		return Target_C_Type;																\
	}																						\
																							\
	asd_Define_GetData(ReturnType)															\



	// GetParam() template
	template <typename ReturnType>
	inline ReturnType* GetParam_Internal(DBStatementHandle* a_handle,
										 SQLUSMALLINT a_paramNumber,
										 ReturnType& a_return)
	{
		void* buf;
		SQLLEN ind;
		a_handle->GetParam(a_paramNumber, buf, ind);
		if (ind == SQL_NULL_DATA)
			return nullptr;

		a_return = *(ReturnType*)buf;
		return &a_return;
	}

#define asd_Define_Caster_GetParam_Ptr(Type, PtrClass)										\
	Caster_GetParam::operator PtrClass<Type>() const										\
	{																						\
		PtrClass<Type> ret;																	\
		Type temp;																			\
		if (GetParam_Internal<Type>(m_handle, m_index, temp) != nullptr)					\
			ret = PtrClass<Type>(new Type(std::move(temp)));								\
		return ret;																			\
	}																						\

#define asd_Define_GetParam(Type)															\
	Caster_GetParam::operator Type() const													\
	{																						\
		Type ret;																			\
		if (GetParam_Internal<Type>(m_handle, m_index, ret) == nullptr)						\
			throw NullDataException("is null data.");										\
		return ret;																			\
	}																						\
																							\
	Caster_GetParam::operator Type*() const													\
	{																						\
		thread_local Type t_temp;															\
		return GetParam_Internal<Type>(m_handle, m_index, t_temp);							\
	}																						\
																							\
	asd_Define_Caster_GetParam_Ptr(Type, std::shared_ptr);									\
	asd_Define_Caster_GetParam_Ptr(Type, std::unique_ptr);									\
																							\
	template<>																				\
	Type* DBStatement::GetParam<Type>(uint16_t a_paramNumber,								\
									  Type& a_return /*Out*/)								\
	{																						\
		return GetParam_Internal<Type>(m_handle.get(), a_paramNumber, a_return);			\
	}																						\
																							\
	template <>																				\
	bool DBStatement::IsNullParam<Type>(uint16_t a_columnIndex)								\
	{																						\
		void* buf;																			\
		SQLLEN ind;																			\
		m_handle->GetParam(a_columnIndex, buf, ind);										\
		if (ind == SQL_NULL_DATA)															\
			return true;																	\
		return false;																		\
	}																						\
																							\
	template <>																				\
	bool DBStatement::IsNullParam<Type>(Type* a_boundPtr)									\
	{																						\
		void* buf;																			\
		SQLLEN ind;																			\
		m_handle->GetParam(a_boundPtr, buf, ind);											\
		if (ind == SQL_NULL_DATA)															\
			return true;																	\
		return false;																		\
	}																						\



	// Specialization Getter, Define Convert
#define asd_Define_ConvertStream(Type, SQL_C_Type, DirParamName, DataParamName, BufParamName, LenParamName)		\
	asd_Define_GetData_SetTypecode(Type, SQL_C_Type);															\
	asd_Define_GetParam(Type);																					\
																												\
	template <>																									\
	inline void ConvertStream<Type>(bool DirParamName,															\
									Type& DataParamName,														\
									void*& BufParamName,														\
									SQLLEN& LenParamName)														\

#define asd_Define_ConvertStream_TypicalCase(Type, SQL_C_Type)													\
	asd_Define_ConvertStream(Type, SQL_C_Type, a_direction, a_data, a_bufPtr, a_dataLen)						\
	{																											\
		if (Is_Left_To_Right(a_direction)) {																	\
			a_bufPtr = &a_data;																					\
			a_dataLen = sizeof(Type);																			\
		}																										\
		else {																									\
			asd_DAssert(sizeof(Type) == a_dataLen);																\
			a_data = *((Type*)a_bufPtr);																		\
		}																										\
	}																											\

#define asd_Define_ConvertStream_BinaryCase(Type, SQL_C_Type, IsString)											\
	asd_Define_ConvertStream(Type, SQL_C_Type, a_direction, a_data, a_bufPtr, a_dataLen)						\
	{																											\
		typedef Type::value_type ValType;																		\
		static_assert(IsString ? true : sizeof(ValType)==1, "is not binary type");								\
		if (Is_Left_To_Right(a_direction)) {																	\
			a_bufPtr = (void*)a_data.data();																	\
			a_dataLen = IsString ? (sizeof(ValType) * a_data.size()) : a_data.size();							\
		}																										\
		else {																									\
			size_t addSize;																						\
			if (IsString) {																						\
				addSize = a_dataLen / sizeof(ValType);															\
				ValType* p = (ValType*)a_bufPtr;																\
				asd_DAssert(a_dataLen % sizeof(ValType) == 0);													\
				asd_DAssert(p[addSize] == '\0');																\
			}																									\
			else {																								\
				addSize = a_dataLen;																			\
			}																									\
			const auto oldSize = a_data.size();																	\
			a_data.resize(oldSize + addSize);																	\
			std::memcpy((void*)(a_data.data() + oldSize),														\
						a_bufPtr,																				\
						a_dataLen);																				\
		}																										\
	}																											\

#define asd_Define_ConvertData_ProxyCase(SrcType, SrcParamName, ProxyType, ProxyParamName, DirParamName)		\
	inline void ConvertData_Internal(bool DirParamName,														\
									 SrcType& SrcParamName,													\
									 ProxyType& ProxyParamName);											\
																												\
	template <>																									\
	inline SrcType* GetData_Internal<SrcType>(DBStatementHandle* a_handle,									\
											  SQLUSMALLINT a_enum,											\
											  SrcType& a_return)											\
	{																											\
		ProxyType t;																							\
		if (GetData_Internal<ProxyType>(a_handle, a_enum, t) == nullptr)										\
			return nullptr;																						\
		Call_ConvertData<ProxyType, SrcType>(t, a_return);														\
		return &a_return;																						\
	}																											\
																												\
	asd_Define_GetData(SrcType);																				\
																												\
	asd_Define_GetParam(SrcType);																				\
																												\
	template<>																									\
	inline void ConvertData<ProxyType, SrcType>(bool a_direction,											\
												ProxyType& a_proxy,											\
												SrcType& a_result)											\
	{																											\
		ConvertData_Internal(!a_direction, a_result, a_proxy);													\
	}																											\
																												\
	template<>																									\
	inline void ConvertData<SrcType, ProxyType>(bool DirParamName,											\
												SrcType& a_result,											\
												ProxyType& a_proxy)											\
	{																											\
		ConvertData_Internal(a_direction, a_result, a_proxy);													\
	}																											\
																												\
	inline void ConvertData_Internal(bool DirParamName,														\
									 SrcType& SrcParamName,													\
									 ProxyType& ProxyParamName)												\



	//  SetParameter & BindParameter
	template <typename Type, SQLSMALLINT Param_Direction, bool Bind>
	inline DBStatementHandle::Parameter_ptr CreateParameter(Type* a_bindingTarget,
															bool a_nullInput,
															SQLSMALLINT a_dbType,
															SQLULEN a_bufferSize,
															SQLSMALLINT a_columnScale);

	//  a_bindingTarget이 null인 경우와 null이 아닌 경우에 대한 처리
	//  +--------+---------------------------------------+-----------------------------------------+
	//  |        |                 Set                   |                   Bind                  |
	//  +--------+---------------------------------------+-----------------------------------------+
	//  | In     |   null : input null value             |   null : input null value               |
	//  |        |  !null : use temp buffer,             |  !null : reference to bound variable    |
	//  |        |          copy value to temp buffer    |                                         |
	//  +--------+---------------------------------------+-----------------------------------------+
	//  | Out    |   null : ignore output result         |   null : ignore output result           |
	//  |        |  !null : use temp buffer              |  !null : output to bound variable       |
	//  |        |                                       |                                         |
	//  +--------+---------------------------------------+-----------------------------------------+
	//  | InOut  |  use temp buffer                      |  not null only,                         |
	//  |        |   null : input null value             |  use a_nullInput(bool) parameter,       |
	//  |        |  !null : copy value to temp buffer    |  reference to bound variable            |
	//  +--------+---------------------------------------+-----------------------------------------+

	template <typename T,
			  SQLSMALLINT SQL_C_Type,
			  SQLSMALLINT Param_Direction,
			  bool Bind,
			  int ReservedFlag>
	struct Parameter_Template : public DBStatementHandle::Parameter
	{
		typedef T Type;
		static_assert(Param_Direction == SQL_PARAM_INPUT ||
					  Param_Direction == SQL_PARAM_OUTPUT ||
					  Param_Direction == SQL_PARAM_INPUT_OUTPUT,
					  "invalid template parameter : Param_Direction");

		static const SQLLEN NotInitIndicaotr = SQL_NULL_DATA - 123;
		static_assert(NotInitIndicaotr < 0 && SQL_NULL_DATA < 0,
					  "SQL_NULL_DATA값 확인 요망");

		SQLSMALLINT m_dbType = TypeTable[SQLType::UNKNOWN_TYPE];
		SQLULEN m_bufferSize = 0;
		SQLSMALLINT m_columnScale = 0;
		Type* m_bindingTarget = nullptr;
		SQLLEN m_indicator = SQL_NULL_DATA;
		Type m_temp;


		Parameter_Template(Type* a_bindingTarget,
						   bool a_nullInput,
						   SQLSMALLINT a_dbType,
						   SQLULEN a_bufferSize,
						   SQLSMALLINT a_columnScale)
		{
			m_dbType = a_dbType;
			m_bufferSize = a_bufferSize;
			m_columnScale = a_columnScale;

			// Null 여부
			m_indicator = a_nullInput ? SQL_NULL_DATA : NotInitIndicaotr;

			// 입력값 초기화, 바인딩 목표 설정
			m_bindingTarget = a_bindingTarget;
			const bool SetNotNull = (Bind==false && a_nullInput==false);
			switch (Param_Direction) {
				case SQL_PARAM_INPUT: {
					asd_DAssert((a_bindingTarget == nullptr) == a_nullInput);
					if (SetNotNull) {
						asd_DAssert(a_bindingTarget != nullptr);
						m_temp = *a_bindingTarget;
						m_bindingTarget = &m_temp;
					}
					break;
				}
				case SQL_PARAM_OUTPUT: {
					if (Bind)
						asd_DAssert((a_bindingTarget == nullptr) == a_nullInput);
					if (SetNotNull)
						m_bindingTarget = &m_temp;
					break;
				}
				case SQL_PARAM_INPUT_OUTPUT: {
					if (Bind == false) {
						m_bindingTarget = &m_temp;
						if (a_nullInput == false) {
							asd_DAssert(a_bindingTarget != nullptr);
							m_temp = *a_bindingTarget;
						}
					}
					asd_DAssert(m_bindingTarget != nullptr);
					break;
				}
				default:
					asd_DAssert(false);
					break;
			}
		}


		void RequestParamInfo(DBStatementHandle& a_stmtHandle,
							  SQLUSMALLINT a_paramNumber)
		{
			const auto& MaramMap = a_stmtHandle.m_paramMap_byParamNum;
			asd_DAssert(MaramMap.find(a_paramNumber) != MaramMap.end());
			if (m_dbType != SQL_UNKNOWN_TYPE)
				return;

			a_stmtHandle.GetParamInfo(a_paramNumber,
									  &m_dbType,
									  &m_bufferSize,
									  &m_columnScale,
									  nullptr);
		}


		virtual void GetBindInfo(DBStatementHandle& a_requester,
								 SQLUSMALLINT a_paramNumber,
								 SQLSMALLINT& a_direction /*Out*/,
								 SQLSMALLINT& a_appType /*Out*/,
								 SQLSMALLINT& a_dbType /*Out*/,
								 SQLULEN& a_bufferSize /*Out*/,
								 SQLSMALLINT& a_columnScale /*Out*/,
								 void*& a_buffer /*Out*/,
								 SQLLEN*& a_indicator /*Out*/) override
		{
			RequestParamInfo(a_requester, a_paramNumber);

			a_direction = Param_Direction;
			a_appType = SQL_C_Type;
			a_dbType = m_dbType;
			a_columnScale = m_columnScale;
			a_indicator = &m_indicator;

			switch (Param_Direction) {
				case SQL_PARAM_INPUT: {
					if (m_indicator == SQL_NULL_DATA) {
						a_buffer = nullptr;
						a_bufferSize = 0;
					}
					else {
						asd_DAssert(m_bindingTarget != nullptr);
						ConvertStream<Type>(Left_To_Right, *m_bindingTarget, a_buffer, m_indicator);
						a_bufferSize = m_indicator;
						asd_DAssert(m_indicator >= 0);
					}
					break;
				}
				case SQL_PARAM_OUTPUT: {
					if (m_indicator == SQL_NULL_DATA) {
						a_buffer = nullptr;
						a_bufferSize = 0;
					}
					else {
						asd_DAssert(m_bindingTarget != nullptr);
						ConvertStream<Type>(Left_To_Right, *m_bindingTarget, a_buffer, m_indicator);
						asd_DAssert(m_indicator >= 0);
						if (m_bufferSize < (SQLULEN)m_indicator)
							m_bufferSize = (SQLULEN)m_indicator;
						asd_DAssert(m_bufferSize >= (SQLULEN)m_indicator);
						a_bufferSize = m_bufferSize;
					}
					break;
				}
				case SQL_PARAM_INPUT_OUTPUT: {
					asd_DAssert(m_bindingTarget != nullptr);
					SQLLEN inputLen;
					ConvertStream<Type>(Left_To_Right, *m_bindingTarget, a_buffer, inputLen);
					if (m_indicator != SQL_NULL_DATA) {
						m_indicator = inputLen;
						asd_DAssert(m_indicator >= 0);
						if (m_bufferSize < (SQLULEN)m_indicator)
							m_bufferSize = (SQLULEN)m_indicator;
					}
					a_bufferSize = m_bufferSize;
					break;
				}
				default:
					asd_DAssert(false);
					break;
			}
		}


		virtual void GetBoundObj(void*& a_objPtr /*Out*/,
								 SQLLEN& a_indicator /*Out*/) const override
		{
			a_objPtr = m_bindingTarget;
			a_indicator = m_indicator;
		}
	};



	template <typename Binary,
			  SQLSMALLINT SQL_C_Type,
			  SQLSMALLINT Param_Direction,
			  bool Bind,
			  bool IsStringType,
			  bool IsSharedPtr>
	struct Parameter_Binary : public Parameter_Template<Binary,
														SQL_C_Type,
														Param_Direction,
														Bind,
														(int)IsStringType>
	{
		typedef
			Parameter_Template<Binary,
							   SQL_C_Type,
							   Param_Direction,
							   Bind,
							   (int)IsStringType>
			BaseType;

		typedef
			typename Binary::value_type
			ValType;

		typedef
			typename BaseType::Type
			Type;

		using BaseType::BaseType;
		using BaseType::m_bufferSize;
		using BaseType::m_indicator;
		using BaseType::m_bindingTarget;

		inline static size_t ToCharCount(const size_t a_byte)
		{
			asd_DAssert(a_byte % sizeof(ValType) == 0);
			if (IsStringType)
				return a_byte / sizeof(ValType);
			else
				return a_byte;
		}


		virtual void GetBindInfo(DBStatementHandle& a_requester,
								 SQLUSMALLINT a_paramNumber,
								 SQLSMALLINT& a_direction /*Out*/,
								 SQLSMALLINT& a_appType /*Out*/,
								 SQLSMALLINT& a_dbType /*Out*/,
								 SQLULEN& a_bufferSize /*Out*/,
								 SQLSMALLINT& a_columnScale /*Out*/,
								 void*& a_buffer /*Out*/,
								 SQLLEN*& a_indicator /*Out*/) override
		{
			// Output이 유효한 경우 버퍼를 충분히 준비한다.
			SQLLEN inputSize = SQL_NULL_DATA;
			if (Param_Direction != SQL_PARAM_INPUT) {
				BaseType::RequestParamInfo(a_requester, a_paramNumber);
				asd_DAssert(m_bufferSize > 0);
				if (m_indicator == SQL_NULL_DATA)
					asd_DAssert(Param_Direction == SQL_PARAM_OUTPUT);
				else {
					asd_DAssert(m_bindingTarget != nullptr);
					inputSize = m_bindingTarget->size();
					asd_DAssert(inputSize >= 0);
					if (m_bufferSize > (SQLULEN)inputSize) {
						const size_t NullChar = IsStringType ? 1 : 0;
						m_bindingTarget->resize(ToCharCount(m_bufferSize) - NullChar);
						if (IsStringType)
							((ValType*)m_bindingTarget->data())[inputSize] = '\0';
					}
					else if (IsSharedPtr) {
						// 공유버퍼인 경우 다른 공유자에게 영향을 주면 안되므로
						// 버퍼를 새로 할당한다.
						m_bindingTarget->resize(m_bindingTarget->size());
					}
				}
			}

			BaseType::GetBindInfo(a_requester,
								  a_paramNumber,
								  a_direction,
								  a_appType,
								  a_dbType,
								  a_bufferSize,
								  a_columnScale,
								  a_buffer,
								  a_indicator);

			// 위의 BaseType::GetBindInfo가 리턴된 후
			// Output 버퍼를 준비 할 때 호출했던 resize()로 인하여
			// m_indicator에 실제 입력값보다 큰 값이 들어갈 수 있으므로 
			// 이를 보정해준다.
			if (Param_Direction != SQL_PARAM_INPUT) {
				if (m_indicator != SQL_NULL_DATA) {
					m_indicator = inputSize;
					asd_DAssert(m_indicator != SQL_NULL_DATA);
					asd_DAssert(m_indicator >= 0);
					asd_DAssert(m_bindingTarget != nullptr);
					asd_DAssert(m_bindingTarget->size() >= (size_t)m_indicator);
				}
			}
		}


		virtual void AfterFetch() override
		{
			if (Param_Direction == SQL_PARAM_INPUT)
				return;

			if (m_indicator == SQL_NULL_DATA)
				return;

			if (m_bindingTarget == nullptr) {
				asd_DAssert(Param_Direction == SQL_PARAM_OUTPUT);
				return;
			}

			asd_DAssert(m_indicator >= 0);
			asd_DAssert((size_t)m_indicator <= m_bindingTarget->size());
			m_bindingTarget->resize(ToCharCount(m_indicator));
		}
	};



	template <typename T,
			  SQLSMALLINT SQL_C_Type,
			  SQLSMALLINT Param_Direction,
			  bool Bind,
			  typename ProxyType>
	struct Parameter_Proxy : public DBStatementHandle::Parameter
	{
		T* m_bindingTarget = nullptr;
		bool m_nullInput = true;
		ProxyType m_proxyObj;
		DBStatementHandle::Parameter_ptr m_proxyParam;


		Parameter_Proxy(T* a_bindingTarget,
						bool a_nullInput,
						SQLSMALLINT a_dbType,
						SQLULEN a_bufferSize,
						SQLSMALLINT a_columnScale)
		{
			m_nullInput = a_nullInput;
			if (Bind)
				m_bindingTarget = a_bindingTarget;

			ProxyType* p = nullptr;
			if (a_bindingTarget != nullptr) {
				p = &m_proxyObj;
				if (Bind==false && Param_Direction!=SQL_PARAM_OUTPUT && a_nullInput==false) {
					// 셋팅 시점 복사
					Call_ConvertData<T, ProxyType>(*a_bindingTarget, m_proxyObj);
				}
			}
			else {
				if (Bind==false && Param_Direction!=SQL_PARAM_INPUT)
					p = &m_proxyObj;
			}
			m_proxyParam = CreateParameter<ProxyType, Param_Direction, true>(p,
																			 a_nullInput,
																			 a_dbType,
																			 a_bufferSize,
																			 a_columnScale);
		}


		virtual void GetBindInfo(DBStatementHandle& a_requester,
								 SQLUSMALLINT a_paramNumber,
								 SQLSMALLINT& a_direction /*Out*/,
								 SQLSMALLINT& a_appType /*Out*/,
								 SQLSMALLINT& a_dbType /*Out*/,
								 SQLULEN& a_bufferSize /*Out*/,
								 SQLSMALLINT& a_columnScale /*Out*/,
								 void*& a_buffer /*Out*/,
								 SQLLEN*& a_indicator /*Out*/) override
		{
			asd_DAssert(m_proxyParam != nullptr);

			const bool NeedCopy = Bind
							   && Param_Direction != SQL_PARAM_OUTPUT
							   && m_nullInput == false
							   && m_bindingTarget != nullptr;

			if (NeedCopy) {
				// Execute 시점 복사
				Call_ConvertData<T, ProxyType>(*m_bindingTarget, m_proxyObj);
			}

			m_proxyParam->GetBindInfo(a_requester,
									  a_paramNumber,
									  a_direction,
									  a_appType,
									  a_dbType,
									  a_bufferSize,
									  a_columnScale,
									  a_buffer,
									  a_indicator);
		}


		virtual void AfterFetch() override
		{
			asd_DAssert(m_proxyParam != nullptr);
			m_proxyParam->AfterFetch();

			if (Bind && Param_Direction!=SQL_PARAM_INPUT) {
				void* p;
				SQLLEN ind;
				m_proxyParam->GetBoundObj(p, ind);

				if (p!=nullptr && m_bindingTarget!=nullptr && ind!=SQL_NULL_DATA) {
					asd_DAssert(p == &m_proxyObj);
					Call_ConvertData<ProxyType, T>(m_proxyObj, *m_bindingTarget);
				}
			}
		}


		virtual void GetBoundObj(void*& a_objPtr /*Out*/,
								 SQLLEN& a_indicator /*Out*/) const override
		{
			asd_DAssert(m_proxyParam != nullptr);

			void* p;
			m_proxyParam->GetBoundObj(p, a_indicator);

			if (Bind) {
				a_objPtr = m_bindingTarget;
			}
			else {
				if (p == nullptr)
					a_objPtr = nullptr;
				else {
					thread_local T t_tempObj;
					asd_DAssert(p == &m_proxyObj);
					Call_ConvertData<ProxyType, T>(m_proxyObj, t_tempObj);
					a_objPtr = &t_tempObj;
				}
			}
		}
	};



#define asd_Specialize_CreateParameter(Type, ParamClass, Param_Direction, Bind, ...)			\
	template<>																					\
	inline DBStatementHandle::Parameter_ptr														\
		CreateParameter<Type, Param_Direction, Bind>(Type* a_bindingTarget,					\
													 bool a_nullInput,						\
													 SQLSMALLINT a_dbType,					\
													 SQLULEN a_bufferSize,					\
													 SQLSMALLINT a_columnScale)				\
	{																							\
		typedef																					\
			ParamClass<Type, GetTypecode<Type>(), Param_Direction, Bind, __VA_ARGS__>			\
			PT;																					\
		auto p = new PT(a_bindingTarget, a_nullInput, a_dbType, a_bufferSize, a_columnScale);	\
		return DBStatementHandle::Parameter_ptr(p);												\
	}																							\


#define asd_Define_BindParam(Type, ParamClass, ...)														\
																										\
	/* SetInParam */																					\
	asd_Specialize_CreateParameter(Type, ParamClass, SQL_PARAM_INPUT, false, __VA_ARGS__);				\
																										\
	template <>																							\
	void DBStatement::SetInParam<Type>(uint16_t a_paramNumber,										\
									   const Type& a_value,											\
									   SQLType a_columnType,											\
									   uint32_t a_columnSize,										\
									   uint16_t a_columnScale)										\
	{																									\
		auto p = CreateParameter<Type, SQL_PARAM_INPUT, false>((Type*)&a_value,							\
															   false,									\
															   TypeTable[a_columnType],					\
															   a_columnSize,							\
															   a_columnScale);							\
		m_handle->PrepareParameter(a_paramNumber, nullptr, p);											\
	}																									\
																										\
	template <>																							\
	void DBStatement::SetInParam_NullInput<Type>(uint16_t a_paramNumber,								\
												 SQLType a_columnType,								\
												 uint32_t a_columnSize,								\
												 uint16_t a_columnScale)								\
	{																									\
		auto p = CreateParameter<Type, SQL_PARAM_INPUT, false>(nullptr,									\
															   true,									\
															   TypeTable[a_columnType],					\
															   a_columnSize,							\
															   a_columnScale);							\
		m_handle->PrepareParameter(a_paramNumber, nullptr, p);											\
	}																									\
																										\
																										\
	/* BindInParam */																					\
	asd_Specialize_CreateParameter(Type, ParamClass, SQL_PARAM_INPUT, true, __VA_ARGS__);				\
																										\
	template <>																							\
	void DBStatement::BindInParam<Type>(uint16_t a_paramNumber,										\
										Type* a_value,												\
										SQLType a_columnType,										\
										uint32_t a_columnSize,										\
										uint16_t a_columnScale)										\
	{																									\
		auto p = CreateParameter<Type, SQL_PARAM_INPUT, true>(a_value,									\
															  a_value == nullptr,						\
															  TypeTable[a_columnType],					\
															  a_columnSize,								\
															  a_columnScale);							\
		m_handle->PrepareParameter(a_paramNumber, a_value, p);											\
	}																									\
																										\
																										\
																										\
	/* SetOutParam */																					\
	asd_Specialize_CreateParameter(Type, ParamClass, SQL_PARAM_OUTPUT, false, __VA_ARGS__);				\
																										\
	template <>																							\
	void DBStatement::SetOutParam<Type>(uint16_t a_paramNumber,										\
										SQLType a_columnType,										\
										uint32_t a_columnSize,										\
										uint16_t a_columnScale)										\
	{																									\
		auto p = CreateParameter<Type, SQL_PARAM_OUTPUT, false>(nullptr,								\
																false,									\
																TypeTable[a_columnType],				\
																a_columnSize,							\
																a_columnScale);							\
		m_handle->PrepareParameter(a_paramNumber, nullptr, p);											\
	}																									\
																										\
	template <>																							\
	void DBStatement::SetOutParam_NullInput<Type>(uint16_t a_paramNumber,							\
												  SQLType a_columnType,								\
												  uint32_t a_columnSize,								\
												  uint16_t a_columnScale)							\
	{																									\
		auto p = CreateParameter<Type, SQL_PARAM_OUTPUT, false>(nullptr,								\
																true,									\
																TypeTable[a_columnType],				\
																a_columnSize,							\
																a_columnScale);							\
		m_handle->PrepareParameter(a_paramNumber, nullptr, p);											\
	}																									\
																										\
																										\
	/* BindOutParam */																					\
	asd_Specialize_CreateParameter(Type, ParamClass, SQL_PARAM_OUTPUT, true, __VA_ARGS__);				\
																										\
	template <>																							\
	void DBStatement::BindOutParam<Type>(uint16_t a_paramNumber,										\
										 Type* a_varptr,											\
										 SQLType a_columnType,										\
										 uint32_t a_columnSize,										\
										 uint16_t a_columnScale)										\
	{																									\
		auto p = CreateParameter<Type, SQL_PARAM_OUTPUT, true>(a_varptr,								\
															   a_varptr == nullptr,						\
															   TypeTable[a_columnType],					\
															   a_columnSize,							\
															   a_columnScale);							\
		m_handle->PrepareParameter(a_paramNumber, a_varptr, p);											\
	}																									\
																										\
																										\
																										\
	/* SetInOutParam */																					\
	asd_Specialize_CreateParameter(Type, ParamClass, SQL_PARAM_INPUT_OUTPUT, false, __VA_ARGS__);		\
																										\
	template <>																							\
	void DBStatement::SetInOutParam<Type>(uint16_t a_paramNumber,									\
										  const Type& a_value,										\
										  SQLType a_columnType,										\
										  uint32_t a_columnSize,										\
										  uint16_t a_columnScale)									\
	{																									\
		auto p = CreateParameter<Type, SQL_PARAM_INPUT_OUTPUT, false>((Type*)&a_value,					\
																	  false,							\
																	  TypeTable[a_columnType],			\
																	  a_columnSize,						\
																	  a_columnScale);					\
		m_handle->PrepareParameter(a_paramNumber, nullptr, p);											\
	}																									\
																										\
	template <>																							\
	void DBStatement::SetInOutParam_NullInput<Type>(uint16_t a_paramNumber,							\
													SQLType a_columnType,							\
													uint32_t a_columnSize,							\
													uint16_t a_columnScale)							\
	{																									\
		auto p = CreateParameter<Type, SQL_PARAM_INPUT_OUTPUT, false>(nullptr,							\
																	  true,								\
																	  TypeTable[a_columnType],			\
																	  a_columnSize,						\
																	  a_columnScale);					\
		m_handle->PrepareParameter(a_paramNumber, nullptr, p);											\
	}																									\
																										\
																										\
	/* BindInOutParam */																				\
	asd_Specialize_CreateParameter(Type, ParamClass, SQL_PARAM_INPUT_OUTPUT, true, __VA_ARGS__);		\
																										\
	template <>																							\
	void DBStatement::BindInOutParam<Type>(uint16_t a_paramNumber,									\
										   Type& a_var,												\
										   bool a_nullInput,											\
										   SQLType a_columnType,										\
										   uint32_t a_columnSize,									\
										   uint16_t a_columnScale)									\
	{																									\
		auto p = CreateParameter<Type, SQL_PARAM_INPUT_OUTPUT, true>(&a_var,							\
																	 a_nullInput,						\
																	 TypeTable[a_columnType],			\
																	 a_columnSize,						\
																	 a_columnScale);					\
		m_handle->PrepareParameter(a_paramNumber, &a_var, p);											\
	}																									\



#define asd_Define_BindParam_TypicalCase(Type)										\
	asd_Define_BindParam(Type, Parameter_Template, 0);								\

#define asd_Define_BindParam_BinaryCase(Type, IsString, IsSharedPtr)				\
	asd_Define_BindParam(Type, Parameter_Binary, IsString, IsSharedPtr);			\

#define asd_Define_BindParam_ProxyCase(Type, ProxyType)								\
	asd_Define_BindParam(Type, Parameter_Proxy, ProxyType);							\




	/***** Tempalte Specialization **************************************************************************/

	asd_Define_ConvertStream_TypicalCase(int8_t, SQL_C_TINYINT);
	asd_Define_BindParam_TypicalCase(int8_t);

	asd_Define_ConvertStream_TypicalCase(short, SQL_C_SHORT);
	asd_Define_BindParam_TypicalCase(short);

	asd_Define_ConvertStream_TypicalCase(int, SQL_C_LONG);
	asd_Define_BindParam_TypicalCase(int);

	asd_Define_ConvertStream_TypicalCase(int64_t, SQL_C_SBIGINT);
	asd_Define_BindParam_TypicalCase(int64_t);

	asd_Define_ConvertStream_TypicalCase(float, SQL_C_FLOAT);
	asd_Define_BindParam_TypicalCase(float);

	asd_Define_ConvertStream_TypicalCase(double, SQL_C_DOUBLE);
	asd_Define_BindParam_TypicalCase(double);

	asd_Define_ConvertStream_TypicalCase(bool, SQL_C_BIT);
	asd_Define_BindParam_TypicalCase(bool);

	asd_Define_ConvertStream_TypicalCase(asd::SQL_TIMESTAMP_STRUCT, SQL_C_TIMESTAMP);
	asd_Define_BindParam_TypicalCase(asd::SQL_TIMESTAMP_STRUCT);

	asd_Define_ConvertStream_TypicalCase(asd::SQL_DATE_STRUCT, SQL_C_DATE);
	asd_Define_BindParam_TypicalCase(asd::SQL_DATE_STRUCT);

	asd_Define_ConvertStream_TypicalCase(asd::SQL_TIME_STRUCT, SQL_C_TIME);
	asd_Define_BindParam_TypicalCase(asd::SQL_TIME_STRUCT);

	asd_Define_ConvertStream_BinaryCase(MString, SQL_C_CHAR, true);
	asd_Define_BindParam_BinaryCase(MString, true, true);

	asd_Define_ConvertStream_BinaryCase(WString, SQL_C_WCHAR, true);
	asd_Define_BindParam_BinaryCase(WString, true, true);

	asd_Define_ConvertStream_BinaryCase(std::string, SQL_C_CHAR, true);
	asd_Define_BindParam_BinaryCase(std::string, true, false);

	asd_Define_ConvertStream_BinaryCase(std::wstring, SQL_C_WCHAR, true);
	asd_Define_BindParam_BinaryCase(std::wstring, true, false);

	asd_Define_ConvertStream_BinaryCase(std::vector<uint8_t>, SQL_C_BINARY, false);
	asd_Define_BindParam_BinaryCase(std::vector<uint8_t>, false, false);

	asd_Define_ConvertStream_BinaryCase(SharedVector<uint8_t>, SQL_C_BINARY, false);
	asd_Define_BindParam_BinaryCase(SharedVector<uint8_t>, false, true);

	asd_Define_ConvertData_ProxyCase(tm, a_src, asd::SQL_TIMESTAMP_STRUCT, a_proxy, a_direction)
	{
		if (Is_Left_To_Right(a_direction)) {
			memset(&a_proxy, 0, sizeof(a_proxy));
			a_proxy.year		= a_src.tm_year + 1900;
			a_proxy.month		= a_src.tm_mon + 1;
			a_proxy.day			= a_src.tm_mday;
			a_proxy.hour		= a_src.tm_hour;
			a_proxy.minute		= a_src.tm_min;
			a_proxy.second		= a_src.tm_sec;
			a_proxy.fraction	= 0;
		}
		else {
			// 요일정보를 채우기 위해 asd::DateTime를 사용
			DateTime(a_proxy).To(a_src);
		}
	}
	asd_Define_BindParam_ProxyCase(tm, asd::SQL_TIMESTAMP_STRUCT);

	asd_Define_ConvertData_ProxyCase(Date, a_src, asd::SQL_DATE_STRUCT, a_proxy, a_direction)
	{
		if (Is_Left_To_Right(a_direction)) {
			a_src.To(a_proxy);
		}
		else {
			a_src.From(a_proxy);
		}
	}
	asd_Define_BindParam_ProxyCase(Date, asd::SQL_DATE_STRUCT);

	asd_Define_ConvertData_ProxyCase(Time, a_src, asd::SQL_TIME_STRUCT, a_proxy, a_direction)
	{
		if (Is_Left_To_Right(a_direction)) {
			a_src.To(a_proxy);
		}
		else {
			a_src.From(a_proxy);
		}
	}
	asd_Define_BindParam_ProxyCase(Time, asd::SQL_TIME_STRUCT);

	asd_Define_ConvertData_ProxyCase(DateTime, a_src, asd::SQL_TIMESTAMP_STRUCT, a_proxy, a_direction)
	{
		if (Is_Left_To_Right(a_direction)) {
			a_src.To(a_proxy);
		}
		else {
			a_src.From(a_proxy);
		}
	}
	asd_Define_BindParam_ProxyCase(DateTime, asd::SQL_TIMESTAMP_STRUCT);
}

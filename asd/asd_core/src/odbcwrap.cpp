#include "stdafx.h"
#include "asd/odbcwrap.h"
#include <sql.h>
#include <sqlext.h>
#include <unordered_map>
#include <functional>

namespace asd
{
	MString DBDiagInfo::ToString() const asd_NoThrow
	{
		return MString("State=%s,NativeError=%d,Message=%s",
					   m_state,
					   m_nativeError,
					   m_message.GetData());
	}



	DBException::DBException(IN const DBDiagInfoList& a_diagInfoList) asd_NoThrow
	{
		m_what = "DBException";
		int index = 1;
		for (auto& diagInfo : a_diagInfoList) {
			m_diagInfoList.push_back(diagInfo);
			m_what << "\n [" << (index++) << "] " << diagInfo.ToString();
		}
	}



	struct TypeTableInitializer
	{
		SQLSMALLINT m_table[SQLType::UNKNOWN_TYPE + 1];

		TypeTableInitializer()
		{
#define asd_InitTypeTable(ENUM) m_table[SQLType::ENUM] = SQL_ ## ENUM
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
			asd_InitTypeTable(DATETIME);
			asd_InitTypeTable(DATE);
			asd_InitTypeTable(TIME);
			asd_InitTypeTable(UNKNOWN_TYPE);
		}

		inline SQLSMALLINT operator[] (IN SQLType a_index) const
		{
			return m_table[a_index];
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


		OdbcHandle(MOVE OdbcHandle&& a_rval)
		{
			*this = std::move(a_rval);
		}


		virtual OdbcHandle& operator = (MOVE OdbcHandle&& a_rval)
		{
			m_handle = a_rval.m_handle;
			m_handleType = a_rval.m_handleType;
			m_diagInfoList = std::move(a_rval.m_diagInfoList);
			a_rval.m_handle = SQL_NULL_HANDLE;
			return *this;
		}


		void Init(IN SQLSMALLINT a_handleType,
				  IN OdbcHandle* a_inputHandle)
		{
			Close();

			// 유효한 파라메터인지 검사
			SQLHANDLE inputHandle;
			if (a_inputHandle == nullptr) {
				assert(a_handleType == SQL_HANDLE_ENV);
				inputHandle = SQL_NULL_HANDLE;
			}
			else {
				assert(a_handleType != SQL_HANDLE_ENV);
				if (a_handleType == SQL_HANDLE_DBC)
					assert(a_inputHandle->m_handleType == SQL_HANDLE_ENV);
				else {
					assert(a_handleType==SQL_HANDLE_DESC || a_handleType==SQL_HANDLE_STMT);
					assert(a_inputHandle->m_handleType == SQL_HANDLE_DBC);
				}
				inputHandle = a_inputHandle->m_handle;
			}

			// 할당
			SQLRETURN r = SQLAllocHandle(a_handleType,
										 inputHandle,
										 &m_handle);
			CheckError(r, nullptr, 0);

			// 환경핸들인 경우 환경설정
			if (a_handleType == SQL_HANDLE_ENV) {
				r = SQLSetEnvAttr(m_handle,
								  SQL_ATTR_ODBC_VERSION,
								  (SQLPOINTER)SQL_OV_ODBC3,
								  SQL_IS_INTEGER);
				CheckError(r);
			}

			// 마무리
			m_handleType = a_handleType;
			assert(m_handle != SQL_NULL_HANDLE);
		}


		void CheckError(IN SQLRETURN a_retval)
		{
			CheckError(a_retval, nullptr, 0);
		}


		void CheckError(IN SQLRETURN a_retval,
						IN const SQLCHAR a_ignoreStates[][6],
						IN int a_ignoreCount)
		{
			if (a_retval != SQL_SUCCESS) {
				SetLastErrorList();
				if (a_retval != SQL_SUCCESS_WITH_INFO) {
					if (a_ignoreStates!=nullptr && a_ignoreCount>0) {
						for (auto it=m_diagInfoList.begin(); it!=m_diagInfoList.end(); ) {
							int i;
							for (i=0; i<a_ignoreCount; ++i) {
								int cmp = strcmp(it->m_state,
												 (const char*)a_ignoreStates[i],
												 false);
								if (cmp == 0)
									break;
							}
							if (i < a_ignoreCount)
								it = m_diagInfoList.erase(it);
							else
								++it;
						}
					}
					if (m_diagInfoList.size() > 0)
						throw DBException(m_diagInfoList);
				}
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
					asd_RaiseException("fail SQLGetDiagRec(), return:", r);
				}

				DBDiagInfo diagInfo;
				errMsg[errMsgLen] = '\0';
				memcpy(diagInfo.m_state,
					   state,
					   sizeof(diagInfo.m_state));
				diagInfo.m_nativeError = nativeError;
				diagInfo.m_message = (const char*)errMsg;
				m_diagInfoList.push_back(diagInfo);
			}
		}


		void Close()
		{
			if (m_handle != SQL_NULL_HANDLE) {
				CheckError(SQLFreeHandle(m_handleType, m_handle));
				m_handle = SQL_NULL_HANDLE;
				m_handleType = 0;
				m_diagInfoList.clear();
			}
		}


		virtual ~OdbcHandle()
		{
			asd_Destructor_Start
				Close();
			asd_Destructor_End
		}


	private:
		OdbcHandle(IN const OdbcHandle&) = delete;
		OdbcHandle& operator = (IN const OdbcHandle&) = delete;

	};




	/***** DBConnection **************************************************************************/

	struct DBConnectionHandle : public OdbcHandle
	{
		OdbcHandle m_envHandle;
		bool m_connected = false;


		void Init()
		{
			CloseConnectoin();
			m_envHandle.Init(SQL_HANDLE_ENV, nullptr);
			OdbcHandle::Init(SQL_HANDLE_DBC, &m_envHandle);
		}


		void Open(IN const char* a_constr)
		{
			Init();
			SQLSMALLINT t;
			SQLRETURN r = SQLDriverConnect(m_handle,
										   nullptr,
										   (SQLCHAR*)a_constr,
										   SQL_NTS,
										   nullptr,
										   0,
										   &t,
										   SQL_DRIVER_NOPROMPT);
			CheckError(r);
			m_connected = true;
		}


		void CloseConnectoin()
		{
			if (m_connected) {
				CheckError(SQLDisconnect(m_handle));
				m_connected = false;
			}
			Close();
			m_envHandle.Close();
		}


		virtual ~DBConnectionHandle()
		{
			asd_Destructor_Start
				CloseConnectoin();
			asd_Destructor_End
		}
	};



	void DBConnection::Open(IN const char* a_constr)
		asd_Throws(DBException)
	{
		m_handle = DBConnectionHandle_ptr(new DBConnectionHandle);
		m_handle->Open(a_constr);
	}


	void DBConnection::Close()
		asd_Throws(DBException)
	{
		if (m_handle != nullptr) {
			m_handle->CloseConnectoin();
			m_handle = DBConnectionHandle_ptr(nullptr);
		}
	}


	DBConnection::~DBConnection() asd_NoThrow
	{
		asd_Destructor_Start
			Close();
		asd_Destructor_End
	}



	/***** DBStatement **************************************************************************/

	struct DBStatementHandle : public OdbcHandle
	{
		struct Parameter
		{
			virtual void GetBindInfo(IN DBStatementHandle& a_requester,
									 IN SQLUSMALLINT a_paramNumber,
									 OUT SQLSMALLINT& a_direction,
									 OUT SQLSMALLINT& a_appType,
									 OUT SQLSMALLINT& a_dbType,
									 OUT SQLULEN& a_bufferSize,
									 OUT SQLSMALLINT& a_columnScale,
									 OUT void*& a_buffer,
									 OUT SQLLEN*& a_indicator) = 0;

			virtual void AfterFetch() {}

			virtual void GetBoundObj(OUT void*& a_objPtr,
									 OUT SQLLEN& a_indicator) const = 0;

			virtual ~Parameter() {}

		};
		typedef std::shared_ptr<Parameter> Parameter_ptr;

		DBConnectionHandle_ptr m_conHandle;
		SQLSMALLINT m_colCount = -1; // 컬럼 개수

		// 컬럼명:인덱스 맵
		std::unordered_map<MString, 
						   SQLSMALLINT,
						   MString::Hash_IgnoreCase,
						   MString::EqualTo_IgnoreCase> m_indexMap;

		// Prepare문 또는 Stored Procedure의 Parameter
		std::unordered_map<SQLUSMALLINT, Parameter_ptr> m_paramMap_byParamNum;
		std::unordered_map<void*, Parameter_ptr> m_paramMap_byBoundPtr;

		void Init(REF DBConnectionHandle_ptr a_conHandle)
		{
			CloseStatement();

			assert(a_conHandle != nullptr);
			m_conHandle = a_conHandle;
			OdbcHandle::Init(SQL_HANDLE_STMT, m_conHandle.get());
			CheckError(SQLSetStmtAttr(m_handle,
									  SQL_ATTR_CURSOR_SCROLLABLE,
									  (SQLPOINTER)SQL_TRUE,
									  SQL_IS_INTEGER));
		}


		void Prepare(IN const char* a_query)
		{
			m_paramMap_byParamNum.clear();
			m_paramMap_byBoundPtr.clear();
			CheckError(SQLPrepare(m_handle, 
								  (SQLCHAR*)a_query,
								  SQL_NTS));
		}


		void GetParamInfo(IN SQLUSMALLINT a_paramNumber,
						  OUT SQLSMALLINT* a_columnType,
						  OUT SQLULEN* a_columnSize,
						  OUT SQLSMALLINT* a_scale,
						  OUT SQLSMALLINT* a_nullable)
		{
			CheckError(SQLDescribeParam(m_handle,
										a_paramNumber,
										a_columnType,
										a_columnSize,
										a_scale,
										a_nullable));

			if (a_columnSize != nullptr) {
				assert(a_columnType != nullptr);
				if (*a_columnType==SQL_WCHAR || *a_columnType==SQL_WVARCHAR)
					*a_columnSize *= 4;
			}
		}


		void PrepareParameter(IN SQLUSMALLINT a_paramNumber,
							  IN void* a_bindKey,
							  IN Parameter_ptr& a_param)
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

				CheckError(SQLBindParameter(m_handle,
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


		SQLLEN Excute()
		{
			BindParameter();
			CheckError(SQLExecute(m_handle));

			SQLLEN rows;
			CheckError(SQLRowCount(m_handle, &rows));
			return rows;
		}


		SQLLEN Excute(IN const char* a_query)
		{
			BindParameter();
			CheckError(SQLExecDirect(m_handle,
									 (SQLCHAR*)a_query,
									 SQL_NTS));

			SQLLEN rows;
			CheckError(SQLRowCount(m_handle, &rows));
			return rows;
		}


		bool m_firstFetch = true;
		bool Fetch()
		{
			assert(m_conHandle != nullptr);

			// 1. Fetch
			SQLRETURN r = SQLFetch(m_handle);
			if (r == SQL_NO_DATA_FOUND) {
				return false;
			}

			CheckError(r);

			// 2. 최초 Fetch인 경우 컬럼명과 인덱스를 매핑한다.
			if (m_firstFetch) {
				m_firstFetch = false;
				assert(m_indexMap.empty());

				// 2-1. 컬럼의 개수를 구한다.
				m_colCount = -1;
				CheckError(SQLNumResultCols(m_handle, &m_colCount));
				assert(m_colCount >= 0);

				// 2-2. 컬럼을 순회하면서 컬럼명과 인덱스를 매핑한다.
				for (SQLUSMALLINT i=1; i<=m_colCount; ++i) {
					SQLCHAR buf[SQL_MAX_MESSAGE_LENGTH];
					MString colName;

					// 2-2-1. 컬럼명을 구한다.
					SQLSMALLINT retlen;
					CheckError(SQLColAttribute(m_handle,
											   i,
											   SQL_DESC_NAME,
											   (SQLPOINTER)buf,
											   sizeof(buf),
											   &retlen,
											   nullptr));
					// 2-2-2. 매핑
					if (buf[0] != '\0') {
						colName = (const char*)buf;
						m_indexMap[colName] = i;
					}
				}
			}
			return true;
		}


		bool MoreResult()
		{
			assert(m_conHandle != nullptr);

			m_firstFetch = true;
			m_indexMap.clear();
			SQLRETURN r = SQLMoreResults(m_handle);
			if (r == SQL_NO_DATA) {
				// 모든 데이터를 fetch 완료한 후
				// 바인드했던 출력인자들에 대한 처리를 수행.
				for (auto& param : m_paramMap_byParamNum)
					param.second->AfterFetch();

				return false;
			}

			CheckError(r);
			return true;
		}


		SQLUSMALLINT GetColumnIndex(IN const char* a_colName) const
		{
			const auto it = m_indexMap.find(a_colName);
			if (it == m_indexMap.end())
				return std::numeric_limits<SQLUSMALLINT>::max();
			return it->second;
		}


		SQLLEN GetData(IN SQLUSMALLINT a_colIndex,
					   IN SQLSMALLINT a_returnType,
					   OUT void* a_buf,
					   IN SQLLEN a_bufLen)
		{
			SQLLEN indicator = 0;
			CheckError(SQLGetData(m_handle,
								  a_colIndex,
								  a_returnType,
								  a_buf,
								  a_bufLen,
								  &indicator));
			return indicator;
		}


		void GetParameter(IN SQLUSMALLINT a_paramNumber,
						  OUT void*& a_bufPtr,
						  OUT SQLLEN& a_indicator)
		{
			auto it = m_paramMap_byParamNum.find(a_paramNumber);
			if (it == m_paramMap_byParamNum.end()) {
				asd_RaiseException("parameter[%u] does not exist", a_paramNumber);
			}
			it->second->GetBoundObj(a_bufPtr, a_indicator);
		}


		void GetParameter(IN void* a_key,
						  OUT void*& a_bufPtr,
						  OUT SQLLEN& a_indicator)
		{
			auto it = m_paramMap_byBoundPtr.find(a_key);
			if (it == m_paramMap_byBoundPtr.end()) {
				asd_RaiseException("parameter[%p] does not exist", a_key);
			}
			it->second->GetBoundObj(a_bufPtr, a_indicator);
		}


		void CloseStatement()
		{
			if (m_conHandle != nullptr) {
				const SQLCHAR ignore[] = "24000"; // 커서가 열리지 않았는데 닫으려는 경우
				CheckError(SQLCloseCursor(m_handle),
						   &ignore,
						   1);
				m_conHandle = DBConnectionHandle_ptr();
			}
			Close();
			m_paramMap_byParamNum.clear();
			m_paramMap_byBoundPtr.clear();
		}


		virtual ~DBStatementHandle()
		{
			asd_Destructor_Start
				CloseStatement();
			asd_Destructor_End
		}
	};



	DBStatement::DBStatement() asd_NoThrow
	{
	}


	DBStatement::DBStatement(REF DBConnection& a_conHandle)
		asd_Throws(DBException)
	{
		Init(a_conHandle);
	}


	void DBStatement::Init(REF DBConnection& a_conHandle)
		asd_Throws(DBException)
	{
		m_handle = DBStatementHandle_ptr(new DBStatementHandle);
		m_handle->Init(a_conHandle.m_handle);
	}


	void DBStatement::Prepare(IN const char* a_query)
		asd_Throws(DBException)
	{
		assert(m_handle != nullptr);
		m_handle->Prepare(a_query);
	}


	int64_t DBStatement::Excute()
		asd_Throws(DBException)
	{
		assert(m_handle != nullptr);
		return m_handle->Excute();
	}


	int64_t DBStatement::Excute(IN const char* a_query)
		asd_Throws(DBException)
	{
		assert(m_handle != nullptr);
		return m_handle->Excute(a_query);
	}


	void DBStatement::FetchLoop(IN FetchCallback a_callback)
		asd_Throws(DBException)
	{
		assert(m_handle != nullptr);
		int result = 1;
		do {
			int record = 1;
			while (m_handle->Fetch()) {
				if (a_callback != nullptr)
					a_callback(result, record);
				++record;
			}
			++result;
		} while (m_handle->MoreResult());
	}


	void DBStatement::Close()
		asd_Throws(DBException)
	{
		if (m_handle != nullptr)
			m_handle->CloseStatement();
	}


	DBStatement::~DBStatement()
		asd_NoThrow
	{
		asd_Destructor_Start
			Close();
		asd_Destructor_End
	}




	thread_local DBStatement::Caster* t_lastCaster;

	struct Caster_GetData : public DBStatement::Caster
	{
		DBStatementHandle* m_handle;
		uint16_t m_index;

		asd_DBStatement_Declare_CastOperatorList;
	};

	DBStatement::Caster& DBStatement::GetData(IN uint16_t a_columnIndex)
		asd_Throws(DBException)
	{
		thread_local Caster_GetData caster;
		caster.m_handle = m_handle.get();
		caster.m_index = a_columnIndex;
		t_lastCaster = &caster;
		return caster;
	}

	DBStatement::Caster& DBStatement::GetData(IN const char* a_columnName)
		asd_Throws(DBException)
	{
		return GetData(m_handle->GetColumnIndex(a_columnName));
	}



	struct Caster_GetParameter : public DBStatement::Caster
	{
		DBStatementHandle* m_handle;
		uint16_t m_index;

		asd_DBStatement_Declare_CastOperatorList;
	};

	DBStatement::Caster& DBStatement::GetParameter(IN uint16_t a_paramNumber)
		asd_Throws(DBException)
	{
		thread_local Caster_GetParameter caster;
		caster.m_handle = m_handle.get();
		caster.m_index = a_paramNumber;
		t_lastCaster = &caster;
		return caster;
	}



	/***** Templates **************************************************************************/

	// Convert
	const bool Left_To_Right = true;
	const bool Right_To_Left = false;

	inline constexpr bool Is_Left_To_Right(IN const bool a_direction)
	{
		return a_direction == Left_To_Right;
	}
	inline constexpr bool Is_Right_To_Left(IN const bool a_direction)
	{
		return a_direction == Right_To_Left;
	}


	template <typename Type>
	inline void ConvertStream(IN bool a_direction,
							  REF Type& a_data,
							  REF void*& a_buf,
							  REF SQLLEN& a_len);

	template <typename Type>
	inline void Call_ConvertStream(IN bool a_direction,
								   REF Type& a_data,
								   REF void*& a_buf,
								   REF SQLLEN& a_len)
	{
		ConvertStream<Type>(a_direction, a_data, a_buf, a_len);
	}

	template <typename Left, typename Right>
	inline void ConvertData(IN bool a_direction,
							REF Left& a_left,
							REF Right& a_right);

	template <typename Left, typename Right>
	inline void Call_ConvertData(IN bool a_direction,
								 REF Left& a_left,
								 REF Right& a_right)
	{
		ConvertData<Left, Right>(a_direction, a_left, a_right);
	}



	// GetData() tempalte
	template <typename ReturnType>
	inline constexpr SQLSMALLINT GetTypecode()
	{
		return SQL_C_DEFAULT;
	}

	template <typename ReturnType>
	inline ReturnType* GetData_Internal(IN DBStatementHandle* a_handle,
										IN SQLUSMALLINT a_index,
										REF ReturnType& a_return)
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
				assert(indicator > 0);
				addLen = indicator;
				loop = false;
			}
			void* p = buf;
			Call_ConvertStream<ReturnType>(Right_To_Left, a_return, p, addLen);
		}
		return &a_return;
	}

#define asd_Define_Caster_GetData_Ptr(Type, PtrClass)										\
	DBStatement::Caster::operator PtrClass<Type>()											\
		asd_Throws(DBException)																\
	{																						\
		/* avoid build error */																\
		assert(false);																		\
		return t_lastCaster->operator PtrClass<Type>();										\
	}																						\
																							\
	Caster_GetData::operator PtrClass<Type>()												\
		asd_Throws(DBException)																\
	{																						\
		PtrClass<Type> ret;																	\
		Type temp;																			\
		if (GetData_Internal<Type>(m_handle, m_index, temp) != nullptr)						\
			ret = PtrClass<Type>(new Type(std::move(temp)));								\
		return ret;																			\
	}																						\

#define asd_Define_Caster_GetData(Type)														\
	DBStatement::Caster::operator Type()													\
		asd_Throws(DBException, NullDataException)											\
	{																						\
		/* avoid build error */																\
		assert(false);																		\
		return t_lastCaster->operator Type();												\
	}																						\
																							\
	Caster_GetData::operator Type()															\
		asd_Throws(DBException, NullDataException)											\
	{																						\
		Type ret;																			\
		if (GetData_Internal<Type>(m_handle, m_index, ret) == nullptr)						\
			throw NullDataException("is null data.");										\
		return ret;																			\
	}																						\
																							\
	asd_Define_Caster_GetData_Ptr(Type, std::shared_ptr);									\
	asd_Define_Caster_GetData_Ptr(Type, std::unique_ptr);									\

#define asd_Define_GetData(ReturnType)														\
	asd_Define_Caster_GetData(ReturnType);													\
																							\
	template <>																				\
	ReturnType* DBStatement::GetData<ReturnType>(IN const char* a_columnName,				\
												 OUT ReturnType& a_return)					\
		asd_Throws(DBException)																\
	{																						\
		auto index = m_handle->GetColumnIndex(a_columnName);								\
		return GetData_Internal<ReturnType>(m_handle.get(),									\
											index,											\
											a_return);										\
	}																						\
																							\
																							\
	template <>																				\
	ReturnType* DBStatement::GetData<ReturnType>(IN uint16_t a_index,						\
												 OUT ReturnType& a_return)					\
		asd_Throws(DBException)																\
	{																						\
		return GetData_Internal<ReturnType>(m_handle.get(),									\
											a_index,										\
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



	// GetParameter() template
	template <typename ReturnType>
	inline ReturnType* GetParameter_Internal(IN DBStatementHandle* a_handle,
											 IN SQLUSMALLINT a_paramNumber,
											 REF ReturnType& a_return)
	{
		void* buf;
		SQLLEN ind;
		a_handle->GetParameter(a_paramNumber, buf, ind);
		if (ind == SQL_NULL_DATA)
			return nullptr;

		a_return = *(ReturnType*)buf;
		return &a_return;
	}

#define asd_Define_Caster_GetParameter_Ptr(Type, PtrClass)									\
	Caster_GetParameter::operator PtrClass<Type>()											\
		asd_Throws(DBException)																\
	{																						\
		PtrClass<Type> ret;																	\
		Type temp;																			\
		if (GetParameter_Internal<Type>(m_handle, m_index, temp) != nullptr)				\
			ret = PtrClass<Type>(new Type(std::move(temp)));								\
		return ret;																			\
	}																						\

#define asd_Define_GetParameter(Type)														\
	Caster_GetParameter::operator Type()													\
		asd_Throws(DBException, NullDataException)											\
	{																						\
		Type ret;																			\
		if (GetParameter_Internal<Type>(m_handle, m_index, ret) == nullptr)					\
			throw NullDataException("is null data.");										\
		return ret;																			\
	}																						\
																							\
	asd_Define_Caster_GetParameter_Ptr(Type, std::shared_ptr);								\
	asd_Define_Caster_GetParameter_Ptr(Type, std::unique_ptr);								\
																							\
	template<>																				\
	Type* DBStatement::GetParameter<Type>(IN uint16_t a_paramNumber,						\
										  OUT Type& a_return)								\
		asd_Throws(DBException)																\
	{																						\
		return GetParameter_Internal<Type>(m_handle.get(),									\
										   a_paramNumber,									\
										   a_return);										\
	}																						\
																							\
	template <>																				\
	bool DBStatement::IsNull<Type>(IN Type* a_boundPtr)										\
		asd_Throws(Exception)																\
	{																						\
		void* buf;																			\
		SQLLEN ind;																			\
		m_handle->GetParameter(a_boundPtr, buf, ind);										\
		if (ind == SQL_NULL_DATA)															\
			return true;																	\
		return false;																		\
	}																						\



	// Specialization Getter, Define Convert
#define asd_Define_ConvertStream(Type, SQL_C_Type, DirParamName, DataParamName, BufParamName, LenParamName)		\
	asd_Define_GetData_SetTypecode(Type, SQL_C_Type);															\
	asd_Define_GetParameter(Type);																				\
																												\
	template <>																									\
	inline void ConvertStream<Type>(IN bool DirParamName,														\
									REF Type& DataParamName,													\
									REF void*& BufParamName,													\
									REF SQLLEN& LenParamName)													\


#define asd_Define_ConvertStream_TypicalCase(Type, SQL_C_Type)													\
	asd_Define_ConvertStream(Type, SQL_C_Type, a_direction, a_data, a_bufPtr, a_dataLen)						\
	{																											\
		if (Is_Left_To_Right(a_direction)) {																	\
			a_bufPtr = &a_data;																					\
			a_dataLen = sizeof(Type);																			\
		}																										\
		else {																									\
			assert(sizeof(Type) == a_dataLen);																	\
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
				assert(a_dataLen % sizeof(ValType) == 0);														\
				assert(p[addSize] == '\0');																		\
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


#define asd_Define_ConvertData_ProxyCase(ReturnType, ReturnParamName, ProxyType, ProxyParamName, DirParamName)	\
	template <>																									\
	inline ReturnType* GetData_Internal<ReturnType>(IN DBStatementHandle* a_handle,								\
													IN SQLUSMALLINT a_index,									\
													REF ReturnType& a_return)									\
	{																											\
		ProxyType t;																							\
		if (GetData_Internal<ProxyType>(a_handle, a_index, t) == nullptr)										\
			return nullptr;																						\
		Call_ConvertData<ProxyType, ReturnType>(Left_To_Right, t, a_return);									\
		return &a_return;																						\
	}																											\
																												\
	asd_Define_GetData(ReturnType);																				\
																												\
	asd_Define_GetParameter(ReturnType);																		\
																												\
	template<>																									\
	inline void ConvertData<ProxyType, ReturnType>(IN bool a_direction,											\
												   REF ProxyType& a_proxy,										\
												   REF ReturnType& a_result)									\
	{																											\
		Call_ConvertData<ReturnType, ProxyType>(!a_direction, a_result, a_proxy);								\
	}																											\
																												\
	template<>																									\
	inline void ConvertData<ReturnType, ProxyType>(IN bool DirParamName,										\
												   REF ReturnType& ReturnParamName,								\
												   REF ProxyType& ProxyParamName)								\



	//  SetParameter & BindParameter
	template <typename Type, SQLSMALLINT Param_Direction, bool Bind>
	inline DBStatementHandle::Parameter_ptr CreateParameter(REF Type* a_bindingTarget,
															IN bool a_nullInput,
															IN SQLSMALLINT a_dbType,
															IN SQLULEN a_bufferSize,
															IN SQLSMALLINT a_columnScale);

	//  a_bindingTarget이 null인 경우와 null이 아닌 경우에 대한 처리
	//  +--------+---------------------------------------+-----------------------------------------+
	//  |        |                 Set                   |                   Bind                  |
	//  +--------+---------------------------------------+-----------------------------------------+
	//  | In     |   null : input null value             |   null : input null value               |
	//  |        |  !null : use temp buffer,             |  !null : reference to bound variable    |
	//  |        |          copy value to temp buffer    |                                         |
	//  +--------+---------------------------------------+-----------------------------------------+
	//  | Out    |  null only,                           |   null : ignore output result           |
	//  |        |  use temp buffer                      |  !null : output to bound variable       |
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

		SQLSMALLINT m_dbType = TypeTable[SQLType::UNKNOWN_TYPE];
		SQLULEN m_bufferSize = 0;
		SQLSMALLINT m_columnScale = 0;
		Type* m_bindingTarget = nullptr;
		SQLLEN m_indicator = SQL_NULL_DATA;
		Type* m_temp = nullptr;

		virtual void Init(REF Type* a_bindingTarget,
						  IN bool a_nullInput,
						  IN SQLSMALLINT a_dbType,
						  IN SQLULEN a_bufferSize,
						  IN SQLSMALLINT a_columnScale)
		{
			m_dbType = a_dbType;
			m_bufferSize = a_bufferSize;
			m_columnScale = a_columnScale;
			m_bindingTarget = a_bindingTarget;
			m_indicator = a_nullInput ? SQL_NULL_DATA : ~SQL_NULL_DATA;
			if (Bind == false) {
				if (Param_Direction!=SQL_PARAM_INPUT || a_bindingTarget!=nullptr) {
					if (a_bindingTarget == nullptr) {
						assert(Param_Direction != SQL_PARAM_INPUT);
						m_temp = new Type;
					}
					else {
						m_temp = new Type(*a_bindingTarget);
						m_bindingTarget = nullptr;
					}
				}
			}

			// 설계상의 Validation Check
			{
				switch (Param_Direction) {
					case SQL_PARAM_INPUT: {
						if (Bind) {
							assert(m_temp == nullptr);
						}
						else {
							assert((a_bindingTarget == nullptr) == (m_temp == nullptr));
							assert((a_bindingTarget == nullptr) == a_nullInput);
						}
						break;
					}
					case SQL_PARAM_OUTPUT: {
						if (Bind) {
							assert(m_temp == nullptr);
							assert((a_bindingTarget == nullptr) == a_nullInput);
						}
						else {
							assert(m_temp != nullptr);
							assert(m_bindingTarget == nullptr);
							assert(a_nullInput == false);
						}
						break;
					}
					case SQL_PARAM_INPUT_OUTPUT: {
						if (Bind) {
							assert(m_temp == nullptr);
							assert(m_bindingTarget != nullptr);
						}
						else {
							assert(m_temp != nullptr);
							assert(m_bindingTarget == nullptr);
							assert((a_bindingTarget == nullptr) == a_nullInput);
						}
						break;
					}
					default:
						assert(false);
						break;
				}
			}
		}


		virtual void RequestParamInfo(IN DBStatementHandle& a_stmtHandle,
									  IN SQLUSMALLINT a_paramNumber)
		{
			const auto& MaramMap = a_stmtHandle.m_paramMap_byParamNum;
			assert(MaramMap.find(a_paramNumber) != MaramMap.end());
			if (m_dbType != SQL_UNKNOWN_TYPE)
				return;

			a_stmtHandle.GetParamInfo(a_paramNumber,
									  &m_dbType,
									  &m_bufferSize,
									  &m_columnScale,
									  nullptr);
		}


		virtual void GetBindInfo(IN DBStatementHandle& a_requester,
								 IN SQLUSMALLINT a_paramNumber,
								 OUT SQLSMALLINT& a_direction,
								 OUT SQLSMALLINT& a_appType,
								 OUT SQLSMALLINT& a_dbType,
								 OUT SQLULEN& a_bufferSize,
								 OUT SQLSMALLINT& a_columnScale,
								 OUT void*& a_buffer,
								 OUT SQLLEN*& a_indicator) override
		{
			RequestParamInfo(a_requester, a_paramNumber);

			a_direction = Param_Direction;
			a_appType = SQL_C_Type;
			a_dbType = m_dbType;
			a_columnScale = m_columnScale;
			a_indicator = &m_indicator;

			switch (Param_Direction) {
				case SQL_PARAM_INPUT: {
					if (Bind) {
						if (m_bindingTarget == nullptr) {
							assert(m_indicator == SQL_NULL_DATA);
							a_buffer = nullptr;
							a_bufferSize = 0;
						}
						else {
							assert(m_indicator != SQL_NULL_DATA);
							ConvertStream<Type>(Left_To_Right, *m_bindingTarget, a_buffer, m_indicator);
							a_bufferSize = m_indicator;
						}
					}
					else {
						if (m_temp == nullptr) {
							assert(m_indicator == SQL_NULL_DATA);
							a_buffer = nullptr;
							a_bufferSize = 0;
						}
						else {
							assert(m_indicator != SQL_NULL_DATA);
							ConvertStream<Type>(Left_To_Right, *m_temp, a_buffer, m_indicator);
							a_bufferSize = m_indicator;
						}
					}
					break;
				}
				case SQL_PARAM_OUTPUT: {
					if (Bind) {
						if (m_bindingTarget == nullptr) {
							assert(m_indicator == SQL_NULL_DATA);
							a_buffer = nullptr;
							a_bufferSize = 0;
						}
						else {
							assert(m_indicator != SQL_NULL_DATA);
							ConvertStream<Type>(Left_To_Right, *m_bindingTarget, a_buffer, m_indicator);
							assert(m_bufferSize >= (SQLULEN)m_indicator && m_indicator >= 0);
							a_bufferSize = m_bufferSize;
						}
					}
					else {
						assert(m_temp != nullptr);
						assert(m_indicator != SQL_NULL_DATA);
						ConvertStream<Type>(Left_To_Right, *m_temp, a_buffer, m_indicator);
						assert(m_bufferSize >= (SQLULEN)m_indicator && m_indicator >= 0);
						a_bufferSize = m_bufferSize;
					}
					break;
				}
				case SQL_PARAM_INPUT_OUTPUT: {
					Type* p = Bind ? m_bindingTarget : m_temp;
					assert(p != nullptr);

					SQLLEN inputLen;
					ConvertStream<Type>(Left_To_Right, *p, a_buffer, inputLen);
					if (m_indicator != SQL_NULL_DATA)
						m_indicator = inputLen;
					assert(m_bufferSize >= (SQLULEN)m_indicator && m_indicator >= 0);
					a_bufferSize = m_bufferSize;
					break;
				}
				default:
					assert(false);
					break;
			}
		}


		virtual void GetBoundObj(OUT void*& a_objPtr,
								 OUT SQLLEN& a_indicator) const override
		{
			a_indicator = m_indicator;
			if (m_bindingTarget != nullptr) {
				assert(m_temp == nullptr);
				a_objPtr = m_bindingTarget;
			}
			else if (m_temp != nullptr) {
				assert(m_bindingTarget == nullptr);
				a_objPtr = m_temp;
			}
			else {
				assert(Param_Direction == SQL_PARAM_INPUT);
				a_objPtr = nullptr;
			}
		}


		virtual void Fin()
		{
			if (m_temp != nullptr)
				delete m_temp;
		}


		virtual ~Parameter_Template()
		{
			Fin();
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
			Parameter_Template<Binary, SQL_C_Type, Param_Direction, Bind, (int)IsStringType>
			BaseType;

		typedef
			typename Binary::value_type
			ValType;

		typedef
			typename BaseType::Type
			Type;

		using BaseType::m_bufferSize;
		using BaseType::m_indicator;
		using BaseType::m_bindingTarget;
		using BaseType::m_temp;

		inline static size_t ToBytes(IN const size_t a_charCount)
		{
			if (IsStringType)
				return sizeof(ValType) * (a_charCount + 1);
			else
				return a_charCount;
		}

		inline static size_t ToCharCount(IN const size_t a_byte)
		{
			assert(a_byte % sizeof(ValType) == 0);
			if (IsStringType)
				return (a_byte / sizeof(ValType)) - 1;
			else
				return a_byte;
		}


		virtual void GetBindInfo(IN DBStatementHandle& a_requester,
								 IN SQLUSMALLINT a_paramNumber,
								 OUT SQLSMALLINT& a_direction,
								 OUT SQLSMALLINT& a_appType,
								 OUT SQLSMALLINT& a_dbType,
								 OUT SQLULEN& a_bufferSize,
								 OUT SQLSMALLINT& a_columnScale,
								 OUT void*& a_buffer,
								 OUT SQLLEN*& a_indicator) override
		{
			// Output이 유효한 경우 버퍼를 충분히 준비한다.
			Binary* p = Bind ? m_bindingTarget : m_temp;
			SQLLEN inputSize = SQL_NULL_DATA;
			if (Param_Direction != SQL_PARAM_INPUT) {
				RequestParamInfo(a_requester, a_paramNumber);
				assert(m_bufferSize > 0);
				if (p == nullptr)
					assert(Bind && Param_Direction == SQL_PARAM_OUTPUT);
				else {
					inputSize = ToBytes(p->size());
					assert(inputSize >= 0);
					if (m_bufferSize > (SQLULEN)inputSize) {
						p->resize(ToCharCount(m_bufferSize), '\0');
					}
					else if (IsSharedPtr) {
						// 공유버퍼인 경우 다른 공유자에게 영향을 주면 안되므로
						// 새로 버퍼를 할당한다.
						p->resize(p->size());
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

			// Output 버퍼를 준비 할 때 호출한 resize로 인하여
			// m_indicator에 실제 입력값보다 큰 값이 들어갈 수 있으므로 
			// 이를 보정해준다.
			if (Param_Direction != SQL_PARAM_INPUT) {
				if (m_indicator != SQL_NULL_DATA) {
					m_indicator = inputSize;
					assert(m_indicator != SQL_NULL_DATA);
					assert(m_indicator >= 0);
					assert(p != nullptr);
					assert(p->size() >= (size_t)m_indicator);
				}
			}
		}


		virtual void AfterFetch() override
		{
			if (Param_Direction == SQL_PARAM_INPUT)
				return;

			if (m_indicator == SQL_NULL_DATA)
				return;

			Binary* p = Bind ? m_bindingTarget : m_temp;
			if (p == nullptr) {
				assert(Bind && Param_Direction == SQL_PARAM_OUTPUT);
				return;
			}
			
			assert(m_indicator >= 0);
			assert((size_t)m_indicator <= p->size());
			const size_t NullChar = IsStringType ? 1 : 0;
			p->resize(ToCharCount(m_indicator) + NullChar);
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
		ProxyType m_proxyObj;
		DBStatementHandle::Parameter_ptr m_proxyParam;

		void Init(REF T* a_bindingTarget,
				  IN bool a_nullInput,
				  IN SQLSMALLINT a_dbType,
				  IN SQLULEN a_bufferSize,
				  IN SQLSMALLINT a_columnScale)
		{
			ProxyType* p;
			m_bindingTarget = a_bindingTarget;
			if (m_bindingTarget == nullptr) {
				if (Bind && Param_Direction==SQL_PARAM_OUTPUT)
					p = nullptr;
				else
					p = &m_proxyObj;
			}
			else {
				p = &m_proxyObj;
				if (Bind == false)
					Call_ConvertData<T, ProxyType>(Left_To_Right, *m_bindingTarget, m_proxyObj);
			}
			m_proxyParam = CreateParameter<ProxyType, Param_Direction, true>(p,
																			 a_nullInput,
																			 a_dbType,
																			 a_bufferSize,
																			 a_columnScale);
		}

		virtual void GetBindInfo(IN DBStatementHandle& a_requester,
								 IN SQLUSMALLINT a_paramNumber,
								 OUT SQLSMALLINT& a_direction,
								 OUT SQLSMALLINT& a_appType,
								 OUT SQLSMALLINT& a_dbType,
								 OUT SQLULEN& a_bufferSize,
								 OUT SQLSMALLINT& a_columnScale,
								 OUT void*& a_buffer,
								 OUT SQLLEN*& a_indicator) override
		{
			assert(m_proxyParam != nullptr);
			
			if (Bind && m_bindingTarget!=nullptr)
				Call_ConvertData<T, ProxyType>(Left_To_Right, *m_bindingTarget, m_proxyObj);

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
			assert(m_proxyParam != nullptr);
			m_proxyParam->AfterFetch();

			if (Bind) {
				void* p;
				SQLLEN ind;
				m_proxyParam->GetBoundObj(p, ind);

				if (p!=nullptr && m_bindingTarget!=nullptr && ind!=SQL_NULL_DATA)
					Call_ConvertData<T, ProxyType>(Right_To_Left, *m_bindingTarget, *(ProxyType*)p);
			}
		}

		virtual void GetBoundObj(OUT void*& a_objPtr,
								 OUT SQLLEN& a_indicator) const override
		{
			assert(m_proxyParam != nullptr);
			void* p;
			SQLLEN ind;
			m_proxyParam->GetBoundObj(p, ind);

			if (Bind) {
				a_objPtr = m_bindingTarget;
			}
			else {
				if (p == nullptr)
					a_objPtr = nullptr;
				else {
					thread_local T tempObj;
					Call_ConvertData<T, ProxyType>(Right_To_Left, tempObj, *(ProxyType*)p);
					a_objPtr = &tempObj;
				}
			}
			a_indicator = ind;
		}
	};



#define asd_Specialize_CreateParameter(Type, ParamClass, Param_Direction, Bind, ...)			\
	template<>																					\
	inline DBStatementHandle::Parameter_ptr														\
		CreateParameter<Type, Param_Direction, Bind>(REF Type* a_bindingTarget,					\
													 IN bool a_nullInput,						\
													 IN SQLSMALLINT a_dbType,					\
													 IN SQLULEN a_bufferSize,					\
													 IN SQLSMALLINT a_columnScale)				\
	{																							\
		typedef																					\
			ParamClass<Type, GetTypecode<Type>(), Param_Direction, Bind, __VA_ARGS__>			\
			PT;																					\
		auto p = new PT;																		\
		p->Init(a_bindingTarget, a_nullInput, a_dbType, a_bufferSize, a_columnScale);			\
		return DBStatementHandle::Parameter_ptr(p);												\
	}																							\


	// SetInParam
#define asd_Define_SetInParam(Type, ParamClass, ...)													\
	asd_Specialize_CreateParameter(Type, ParamClass, SQL_PARAM_INPUT, false, __VA_ARGS__);				\
																										\
	template <>																							\
	void DBStatement::SetInParam<Type>(IN uint16_t a_paramNumber,										\
									   IN Type* a_value,												\
									   IN SQLType a_columnType,											\
									   IN uint32_t a_columnSize,										\
									   IN uint16_t a_columnScale)										\
		asd_Throws(DBException)																			\
	{																									\
		auto p = CreateParameter<Type, SQL_PARAM_INPUT, false>(a_value,									\
															   a_value == nullptr,						\
															   TypeTable[a_columnType],					\
															   a_columnSize,							\
															   a_columnScale);							\
		m_handle->PrepareParameter(a_paramNumber, nullptr, p);											\
	}																									\


	// BindInParam
#define asd_Define_BindInParam(Type, ParamClass, ...)													\
	asd_Specialize_CreateParameter(Type, ParamClass, SQL_PARAM_INPUT, true, __VA_ARGS__);				\
																										\
	template <>																							\
	void DBStatement::BindInParam<Type>(IN uint16_t a_paramNumber,										\
										REF Type* a_value,												\
										IN SQLType a_columnType,										\
										IN uint32_t a_columnSize,										\
										IN uint16_t a_columnScale)										\
		asd_Throws(DBException)																			\
	{																									\
		auto p = CreateParameter<Type, SQL_PARAM_INPUT, true>(a_value,									\
															  a_value == nullptr,						\
															  TypeTable[a_columnType],					\
															  a_columnSize,								\
															  a_columnScale);							\
		m_handle->PrepareParameter(a_paramNumber, a_value, p);											\
	}																									\


	// SetOutParam
#define asd_Define_SetOutParam(Type, ParamClass, ...)													\
	asd_Specialize_CreateParameter(Type, ParamClass, SQL_PARAM_OUTPUT, false, __VA_ARGS__);				\
																										\
	template <>																							\
	void DBStatement::SetOutParam<Type>(IN uint16_t a_paramNumber,										\
										IN SQLType a_columnType,										\
										IN uint32_t a_columnSize,										\
										IN uint16_t a_columnScale)										\
		asd_Throws(DBException)																			\
	{																									\
		auto p = CreateParameter<Type, SQL_PARAM_OUTPUT, false>(nullptr,								\
																false,									\
																TypeTable[a_columnType],				\
																a_columnSize,							\
																a_columnScale);							\
		m_handle->PrepareParameter(a_paramNumber, nullptr, p);											\
	}																									\


	// BindOutParam
#define asd_Define_BindOutParam(Type, ParamClass, ...)													\
	asd_Specialize_CreateParameter(Type, ParamClass, SQL_PARAM_OUTPUT, true, __VA_ARGS__);				\
																										\
	template <>																							\
	void DBStatement::BindOutParam<Type>(IN uint16_t a_paramNumber,										\
										 REF Type* a_varptr,											\
										 IN SQLType a_columnType,										\
										 IN uint32_t a_columnSize,										\
										 IN uint16_t a_columnScale)										\
		asd_Throws(DBException)																			\
	{																									\
		auto p = CreateParameter<Type, SQL_PARAM_OUTPUT, true>(a_varptr,								\
															   a_varptr == nullptr,						\
															   TypeTable[a_columnType],					\
															   a_columnSize,							\
															   a_columnScale);							\
		m_handle->PrepareParameter(a_paramNumber, a_varptr, p);											\
	}																									\


	// SetInOutParam
#define asd_Define_SetInOutParam(Type, ParamClass, ...)													\
	asd_Specialize_CreateParameter(Type, ParamClass, SQL_PARAM_INPUT_OUTPUT, false, __VA_ARGS__);		\
																										\
	template <>																							\
	void DBStatement::SetInOutParam<Type>(IN uint16_t a_paramNumber,									\
										  IN Type* a_value,												\
										  IN SQLType a_columnType,										\
										  IN uint32_t a_columnSize,										\
										  IN uint16_t a_columnScale)									\
		asd_Throws(DBException)																			\
	{																									\
		auto p = CreateParameter<Type, SQL_PARAM_INPUT_OUTPUT, false>(a_value,							\
																	  a_value == nullptr,				\
																	  TypeTable[a_columnType],			\
																	  a_columnSize,						\
																	  a_columnScale);					\
		m_handle->PrepareParameter(a_paramNumber, nullptr, p);											\
	}																									\


	// BindInOutParam
#define asd_Define_BindInOutParam(Type, ParamClass, ...)												\
	asd_Specialize_CreateParameter(Type, ParamClass, SQL_PARAM_INPUT_OUTPUT, true, __VA_ARGS__);		\
																										\
	template <>																							\
	void DBStatement::BindInOutParam<Type>(IN uint16_t a_paramNumber,									\
										   REF Type& a_var,												\
										   IN bool a_nullInput,											\
										   IN SQLType a_columnType,										\
										   IN uint32_t a_columnSize,									\
										   IN uint16_t a_columnScale)									\
		asd_Throws(DBException)																			\
	{																									\
		auto p = CreateParameter<Type, SQL_PARAM_INPUT_OUTPUT, true>(&a_var,							\
																	 a_nullInput,						\
																	 TypeTable[a_columnType],			\
																	 a_columnSize,						\
																	 a_columnScale);					\
		m_handle->PrepareParameter(a_paramNumber, &a_var, p);											\
	}																									\



#define asd_Define_BindParam_TypicalCase(Type)										\
	asd_Define_SetInParam(Type, Parameter_Template, 0);								\
	asd_Define_BindInParam(Type, Parameter_Template, 0);							\
																					\
	asd_Define_SetOutParam(Type, Parameter_Template, 0);							\
	asd_Define_BindOutParam(Type, Parameter_Template, 0);							\
																					\
	asd_Define_SetInOutParam(Type, Parameter_Template, 0);							\
	asd_Define_BindInOutParam(Type, Parameter_Template, 0);							\


#define asd_Define_BindParam_BinaryCase(Type, IsString, IsSharedPtr)				\
	asd_Define_SetInParam(Type, Parameter_Binary, IsString, IsSharedPtr);			\
	asd_Define_BindInParam(Type, Parameter_Binary, IsString, IsSharedPtr);			\
																					\
	asd_Define_SetOutParam(Type, Parameter_Binary, IsString, IsSharedPtr);			\
	asd_Define_BindOutParam(Type, Parameter_Binary, IsString, IsSharedPtr);			\
																					\
	asd_Define_SetInOutParam(Type, Parameter_Binary, IsString, IsSharedPtr);		\
	asd_Define_BindInOutParam(Type, Parameter_Binary, IsString, IsSharedPtr);		\


#define asd_Define_BindParam_ProxyCase(Type, ProxyType)								\
	asd_Define_SetInParam(Type, Parameter_Proxy, ProxyType);						\
	asd_Define_BindInParam(Type, Parameter_Proxy, ProxyType);						\
																					\
	asd_Define_SetOutParam(Type, Parameter_Proxy, ProxyType);						\
	asd_Define_BindOutParam(Type, Parameter_Proxy, ProxyType);						\
																					\
	asd_Define_SetInOutParam(Type, Parameter_Proxy, ProxyType);						\
	asd_Define_BindInOutParam(Type, Parameter_Proxy, ProxyType);					\




	/***** Tempalte Specialization **************************************************************************/

	asd_Define_ConvertStream_TypicalCase(char, SQL_C_TINYINT);
	asd_Define_BindParam_TypicalCase(char);

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

	asd_Define_ConvertStream_TypicalCase(SQL_TIMESTAMP_STRUCT, SQL_C_TIMESTAMP);
	asd_Define_BindParam_TypicalCase(SQL_TIMESTAMP_STRUCT);

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

	asd_Define_ConvertStream_BinaryCase(SharedArray<uint8_t>, SQL_C_BINARY, false);
	asd_Define_BindParam_BinaryCase(SharedArray<uint8_t>, false, true);

	asd_Define_ConvertData_ProxyCase(tm, a_return, SQL_TIMESTAMP_STRUCT, a_proxy, a_direction)
	{
		if (Is_Left_To_Right(a_direction)) {
			a_proxy.year		= a_return.tm_year;
			a_proxy.month		= a_return.tm_mon + 1;
			a_proxy.day			= a_return.tm_mday;
			a_proxy.hour		= a_return.tm_hour;
			a_proxy.minute		= a_return.tm_min;
			a_proxy.second		= a_return.tm_sec;
			a_proxy.fraction	= 0;
		}
		else {
			a_return.tm_year	= a_proxy.year;
			a_return.tm_mon 	= a_proxy.month - 1;
			a_return.tm_mday	= a_proxy.day;
			a_return.tm_hour	= a_proxy.hour;
			a_return.tm_min 	= a_proxy.minute;
			a_return.tm_sec 	= a_proxy.second;
		}
	}
	asd_Define_BindParam_ProxyCase(tm, SQL_TIMESTAMP_STRUCT);

	asd_Define_ConvertData_ProxyCase(Time, a_return, SQL_TIMESTAMP_STRUCT, a_proxy, a_direction)
	{
		if (Is_Left_To_Right(a_direction))
			a_return.To(a_proxy);
		else
			a_return.From(a_proxy);
	}
	asd_Define_BindParam_ProxyCase(Time, SQL_TIMESTAMP_STRUCT);
}

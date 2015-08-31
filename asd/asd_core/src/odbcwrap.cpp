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
								int cmp = std::memcmp(it->m_state,
													  a_ignoreStates[i],
													  5 * sizeof(SQLCHAR));
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


		void Open(IN const char* a_constr,
				  IN size_t a_constrlen)
		{
			Init();
			SQLSMALLINT t;
			SQLRETURN r = SQLDriverConnect(m_handle,
										   nullptr,
										   (SQLCHAR*)a_constr,
										   (a_constrlen==0 ? asd::strlen(a_constr) : a_constrlen),
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



	void DBConnection::Open(IN const char* a_constr,
							IN size_t a_constrlen /*= 0*/)
		asd_Throws(DBException)
	{
		m_handle = DBConnectionHandle_ptr(new DBConnectionHandle);
		m_handle->Open(a_constr, a_constrlen);
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
		DBConnectionHandle_ptr m_conHandle;
		SQLSMALLINT m_colCount = -1; // 컬럼 개수

		// 컬럼명:인덱스 맵
		std::unordered_map<MString, 
						   SQLSMALLINT,
						   MString::Hash_IgnoreCase,
						   MString::EqualTo_IgnoreCase> m_indexMap;


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


		void Excute(REF DBConnectionHandle_ptr a_conHandle,
					IN const char* a_query,
					IN size_t a_querylen)
		{
			Init(a_conHandle);
			CheckError(SQLExecDirect(m_handle,
									 (SQLCHAR*)a_query,
									 a_querylen));
		}


		bool Fetch(IN SQLSMALLINT a_orient,
				   IN SQLLEN a_offset)
		{
			assert(m_conHandle != nullptr);

			// 1. Fetch
			SQLRETURN r = SQLFetchScroll(m_handle, a_orient, a_offset);
			if (r == SQL_NO_DATA_FOUND)
				return false;
			else
				CheckError(r);

			// 2. 컬럼의 개수를 구한다.
			m_colCount = -1;
			CheckError(SQLNumResultCols(m_handle, &m_colCount));
			assert(m_colCount >= 0);

			// 3. 컬럼을 순회하면서 컬럼명과 인덱스를 매핑한다.
			for (SQLUSMALLINT i=1; i<=m_colCount; ++i) {
				SQLCHAR buf[SQL_MAX_MESSAGE_LENGTH];
				MString colName;

				// 3.1 컬럼명을 구한다.
				SQLSMALLINT retlen;
				CheckError(SQLColAttribute(m_handle,
										   i,
										   SQL_DESC_NAME,
										   (SQLPOINTER)buf,
										   sizeof(buf),
										   &retlen,
										   nullptr));
				if (buf[0] != '\0') {
					colName = (const char*)buf;

					// 3.2 매핑
					m_indexMap[colName] = i;
				}
#ifdef asd_Debug
				else {
					// 이 경우는 UNNAMED Column이다.
					SQLLEN val;
					assert(SQL_SUCCESS == SQLColAttribute(m_handle,
														  i,
														  SQL_DESC_UNNAMED,
														  nullptr,
														  0,
														  nullptr,
														  &val));
					assert(val == SQL_UNNAMED);
				}
#endif
			}

			return true;
		}


		SQLUSMALLINT GetIndex(IN const char* a_colName) const
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
		}


		virtual ~DBStatementHandle()
		{
			asd_Destructor_Start
				CloseStatement();
			asd_Destructor_End
		}
	};



	void DBStatement::Excute(REF DBConnection& a_conHandle,
							 IN const char* a_query,
							 IN size_t a_querylen /*= 0*/)
		asd_Throws(DBException)
	{
		m_handle = DBStatementHandle_ptr(new DBStatementHandle);
		m_handle->Excute(a_conHandle.m_handle,
						 a_query,
						 a_querylen == 0 ? asd::strlen(a_query) : a_querylen);
	}


	bool DBStatement::Next()
		asd_Throws(DBException)
	{
		assert(m_handle != nullptr);
		return m_handle->Fetch(SQL_FETCH_NEXT, 1);
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


	DBStatement::Caster DBStatement::GetData(IN uint16_t a_columnIndex)
		asd_Throws(DBException)
	{
		return Caster(*this, a_columnIndex);
	}


	DBStatement::Caster DBStatement::GetData(IN const char* a_columnName)
		asd_Throws(DBException)
	{
		return Caster(*this, m_handle->GetIndex(a_columnName));
	}




	/***** DBStatement::GetData() Templates **************************************************************************/

	// Caster
#define asd_Define_CastOperator_Ptr(Type, PtrClass)											\
	DBStatement::Caster::operator PtrClass<Type>()											\
		asd_Throws(DBException)																\
	{																						\
		PtrClass<Type> ret;																	\
		Type temp;																			\
		if (GetData_Base(m_owner.m_handle, m_index, temp) != nullptr)						\
			ret = PtrClass<Type>(new Type(std::move(temp)));								\
		return ret;																			\
	}																						\


#define asd_Define_CastOperator(Type)														\
	DBStatement::Caster::operator Type()													\
		asd_Throws(DBException, NullDataException)											\
	{																						\
		Type ret;																			\
		if (GetData_Base(m_owner.m_handle, m_index, ret) == nullptr)						\
			throw NullDataException("is null data.");										\
		return ret;																			\
	}																						\
	asd_Define_CastOperator_Ptr(Type, std::shared_ptr);										\
	asd_Define_CastOperator_Ptr(Type, std::unique_ptr);										\



	// Base
#define asd_Define_GetData(ReturnType, HandleParamName, IndexParamName, ReturnParamName)	\
	inline ReturnType* GetData_Base(IN DBStatementHandle_ptr&,								\
									IN uint16_t,											\
									OUT ReturnType&);										\
																							\
																							\
	template <>																				\
	ReturnType* DBStatement::GetData<ReturnType>(IN const char* a_columnName,				\
												 OUT ReturnType& a_return)					\
		asd_Throws(DBException)																\
	{																						\
		return GetData_Base(m_handle, m_handle->GetIndex(a_columnName), a_return);			\
	}																						\
																							\
																							\
	template <>																				\
	ReturnType* DBStatement::GetData<ReturnType>(IN uint16_t a_index,						\
												 OUT ReturnType& a_return)					\
		asd_Throws(DBException)																\
	{																						\
		return GetData_Base(m_handle, a_index, a_return);									\
	}																						\
																							\
																							\
	asd_Define_CastOperator(ReturnType);													\
																							\
																							\
	inline ReturnType* GetData_Base(IN DBStatementHandle_ptr& HandleParamName,				\
									IN uint16_t IndexParamName,								\
									OUT ReturnType& ReturnParamName)						\



	// Fixed Size
	template <typename ReturnType>
	inline ReturnType* GetData_FixedSize(IN DBStatementHandle_ptr& a_handle,
										 IN SQLUSMALLINT a_index,
										 IN SQLSMALLINT a_ctype,
										 OUT ReturnType& a_return)
	{
		SQLLEN len = a_handle->GetData(a_index,
									   a_ctype,
									   &a_return,
									   sizeof(a_return));
		if (len == SQL_NULL_DATA)
			return nullptr;
		assert(len == sizeof(a_return));
		return &a_return;
	}

#define asd_Define_GetData_FixedSize(ReturnType, Target_C_Type)			\
	asd_Define_GetData(ReturnType, a_handle, a_index, a_return)			\
	{																	\
		return GetData_FixedSize<ReturnType>(a_handle,					\
											 a_index,					\
											 Target_C_Type,				\
											 a_return);					\
	}																	\



	// String
	template <typename StringType, typename CharType>
	inline StringType* GetData_String(IN DBStatementHandle_ptr& a_handle,
									  IN SQLUSMALLINT a_index,
									  IN SQLSMALLINT a_ctype,
									  OUT StringType& a_return)
	{
		const int BufSize = 1024;
		uint8_t buf[BufSize];
		SQLLEN indicator;
		do {
			indicator = a_handle->GetData(a_index,
										  a_ctype,
										  buf,
										  BufSize);
			if (indicator == SQL_NULL_DATA)
				return nullptr;
			a_return += (const CharType*)buf;
		} while (indicator == SQL_NO_TOTAL);
		assert(indicator > 0);
		return &a_return;
	}

#define asd_Define_GetData_String(ReturnType, CharType, Target_C_Type)		\
	asd_Define_GetData(ReturnType, a_handle, a_index, a_return)				\
	{																		\
		return GetData_String<ReturnType, CharType>(a_handle,				\
													a_index,				\
													Target_C_Type,			\
													a_return);				\
	}																		\



	// Binary, Blob
	template <typename ReturnType>
	struct GetData_Binary_Callback
	{
		inline void operator() (REF ReturnType& a_return,
								IN void* a_buf,
								IN SQLLEN a_len);
	};

	template <typename ReturnType>
	inline ReturnType* GetData_Binary(IN DBStatementHandle_ptr& a_handle,
									  IN SQLUSMALLINT a_index,
									  IN GetData_Binary_Callback<ReturnType>& a_readCallback,
									  REF ReturnType& a_return)
	{
		const int BufSize = 1024;
		uint8_t buf[BufSize];
		SQLLEN indicator;

		bool loop = true;
		while (loop) {
			indicator = a_handle->GetData(a_index,
										  SQL_C_BINARY,
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
			a_readCallback(a_return, buf, addLen);
		}
		return &a_return;
	}

#define asd_Define_GetData_Binary(ReturnType, ReturnTypeName, BufParamName, LenParamName)		\
	asd_Define_GetData(ReturnType, a_handle, a_index, a_return)									\
	{																							\
		GetData_Binary_Callback<ReturnType> callback;											\
		return GetData_Binary<ReturnType>(a_handle,												\
										  a_index,												\
										  callback,												\
										  a_return);											\
	}																							\
																								\
	template<>																					\
	inline void																					\
	GetData_Binary_Callback<ReturnType>::operator() (REF ReturnType& ReturnTypeName,			\
													 IN void* BufParamName,						\
													 IN SQLLEN LenParamName)					\



	// Proxy
	template <typename ProxyType, typename ReturnType>
	struct GetData_UseProxy_Callback
	{
		inline ReturnType* operator() (IN const ProxyType& a_proxy,
									   OUT ReturnType& a_return);
	};

	template <typename ProxyType, typename ReturnType>
	ReturnType* GetData_UseProxy(IN DBStatementHandle_ptr& a_handle,
								 IN SQLUSMALLINT a_index,
								 OUT ReturnType& a_return)
	{
		ProxyType t;
		if (GetData_Base(a_handle, a_index, t) == nullptr)
			return nullptr;
		GetData_UseProxy_Callback<ProxyType, ReturnType> functor;
		return functor(t, a_return);
	}

#define asd_Define_GetData_UseProxy(ProxyType, ProxyParamName, ReturnType, ReturnParamName)				\
	asd_Define_GetData(ReturnType, a_handle, a_index, a_return)											\
	{																									\
		return GetData_UseProxy<ProxyType, ReturnType>(a_handle, a_index, a_return);					\
	}																									\
																										\
	template<>																							\
	inline ReturnType*																					\
	GetData_UseProxy_Callback<ProxyType, ReturnType>::operator() (IN const ProxyType& ProxyParamName,	\
																  OUT ReturnType& ReturnParamName)		\




	/***** DBStatement::GetData() Specialization **************************************************************************/

	asd_Define_GetData_FixedSize(char, SQL_C_TINYINT);

	asd_Define_GetData_FixedSize(short, SQL_C_SHORT);

	asd_Define_GetData_FixedSize(int, SQL_C_LONG);

	asd_Define_GetData_FixedSize(int64_t, SQL_C_SBIGINT);

	asd_Define_GetData_FixedSize(float, SQL_C_FLOAT);

	asd_Define_GetData_FixedSize(double, SQL_C_DOUBLE);

	asd_Define_GetData_FixedSize(bool, SQL_C_BIT);

	asd_Define_GetData_FixedSize(SQL_TIMESTAMP_STRUCT, SQL_C_TIMESTAMP);

	asd_Define_GetData_String(MString, char, SQL_C_CHAR);

	asd_Define_GetData_String(WString, wchar_t, SQL_C_WCHAR);

	asd_Define_GetData_String(std::string, char, SQL_C_CHAR);

	asd_Define_GetData_String(std::wstring, wchar_t, SQL_C_WCHAR);

	asd_Define_GetData_Binary(REF std::vector<uint8_t>, a_return,
							  IN /*void* */ a_buf,
							  IN /*SQLLEN */ a_len)
	{
		const auto oldSize = a_return.size();
		a_return.resize(oldSize + a_len);
		std::memcpy(a_return.data() + oldSize,
					a_buf,
					a_len);
	}

	asd_Define_GetData_Binary(REF SharedArray<uint8_t>, a_return,
							  IN /*void* */ a_buf,
							  IN /*SQLLEN */ a_len)
	{
		const auto oldSize = a_return.GetCount();
		a_return.Resize(oldSize + a_len, true);
		std::memcpy(a_return.get() + oldSize,
					a_buf,
					a_len);
	}

	asd_Define_GetData_UseProxy(IN SQL_TIMESTAMP_STRUCT, a_proxy,
								OUT tm, a_return)
	{
		a_return.tm_year	= a_proxy.year;
		a_return.tm_mon 	= a_proxy.month - 1;
		a_return.tm_mday	= a_proxy.day;
		a_return.tm_hour	= a_proxy.hour;
		a_return.tm_min 	= a_proxy.minute;
		a_return.tm_sec 	= a_proxy.second;
		return &a_return;
	}

	asd_Define_GetData_UseProxy(IN SQL_TIMESTAMP_STRUCT, a_proxy,
								OUT Time, a_return)
	{
		a_return.From(a_proxy);
		return &a_return;
	}
}

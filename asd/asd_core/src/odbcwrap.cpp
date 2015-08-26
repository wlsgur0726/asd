#include "stdafx.h"
#include "asd/odbcwrap.h"
#include <sql.h>
#include <sqlext.h>
#include <unordered_map>

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
			CheckError(r);

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
			if (a_retval != SQL_SUCCESS) {
				SetOdbcErrorList();
				if (a_retval != SQL_SUCCESS_WITH_INFO)
					throw DBException(m_diagInfoList);
			}
		}


		void SetOdbcErrorList()
		{
			m_diagInfoList.clear();

			for (int i=1; ; ++i) {
				SQLCHAR state[6] = {0, 0, 0, 0, 0, 0};
				SQLINTEGER nativeError;
				SQLCHAR errMsg[SQL_MAX_MESSAGE_LENGTH + 1];
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
				diagInfo.m_message = errMsg;
				m_diagInfoList.push_back(diagInfo);
			}
		}


		virtual void Close()
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
			Close();
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


		virtual void Close() override
		{
			if (m_connected) {
				CheckError(SQLDisconnect(m_handle));
				m_connected = false;
			}
			OdbcHandle::Close();
			m_envHandle.Close();
		}


		virtual ~DBConnectionHandle()
		{
			asd_Destructor_Start
				Close();
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
		m_handle->Close();
		m_handle = DBConnectionHandle_ptr(nullptr);
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
		std::unordered_map<MString, SQLSMALLINT> m_indexMap; // 컬럼명 : 인덱스


		void Init(REF DBConnectionHandle_ptr a_conHandle)
		{
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
				MString colName;

				// 3.1 컬럼명의 길이를 구한다.
				SQLSMALLINT namelen = -1;
				SQLRETURN r = SQLColAttribute(m_handle,
											  i,
											  SQL_DESC_NAME,
											  nullptr,
											  0,
											  &namelen,
											  nullptr);
				if (r != SQL_SUCCESS_WITH_INFO)
					CheckError(r);
				assert(namelen >= 0);
				
				// 3.2 컬럼명을 구한다.
				if (namelen > 0) {
					colName.InitBuffer(namelen);
					CheckError(SQLColAttribute(m_handle,
											   i,
											   SQL_DESC_NAME,
											   (SQLPOINTER)colName.GetData(),
											   namelen,
											   &namelen,
											   nullptr));
					assert(colName.GetLength() == namelen);

					// 3.3 매핑
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
					   IN SQLSMALLINT a_targetType,
					   OUT void* a_buf,
					   IN SQLLEN a_bufLen)
		{
			SQLLEN remnant = 0;
			CheckError(SQLGetData(m_handle,
								  a_colIndex,
								  a_targetType,
								  a_buf,
								  a_bufLen,
								  &remnant));
			return remnant;
		}


		virtual void Close() override
		{
			if (m_conHandle != nullptr) {
				CheckError(SQLCloseCursor(m_handle));
				m_conHandle = DBConnectionHandle_ptr();
			}
			OdbcHandle::Close();
		}


		virtual ~DBStatementHandle()
		{
			asd_Destructor_Start
				Close();
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
		m_handle->Close();
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



#define asd_Define_CastOperator(Type)								\
	DBStatement::Caster::operator Type()							\
		asd_Throws(DBException)										\

#define asd_Define_CastOperator_FixSize(Type, Target_C_Type)		\
	asd_Define_CastOperator(Type)									\
	{																\
		Type r;														\
		SQLLEN len = m_owner.m_handle->GetData(m_index,				\
											   Target_C_Type,		\
											   &r,					\
											   sizeof(r));			\
		assert(len == 0);											\
		return r;													\
	}																\



	asd_Define_CastOperator(MString)
	{
		MString r;
		SQLLEN len = m_owner.m_handle->GetData(m_index,
											   SQL_C_CHAR,
											   nullptr,
											   0);
		assert(len >= 0);
		if (len > 0) {
			r.InitBuffer(len);
			len = m_owner.m_handle->GetData(m_index,
											SQL_C_CHAR,
											r.GetData(),
											len);
			assert(len == 0);
		}
		return r;
	}


	asd_Define_CastOperator(std::shared_ptr<uint8_t>)
	{
		typedef std::shared_ptr<uint8_t> BinArr;
		BinArr r;
		SQLLEN len = m_owner.m_handle->GetData(m_index,
											   SQL_C_BINARY,
											   nullptr,
											   0);
		assert(len >= 0);
		if (len > 0) {
			r = BinArr(new uint8_t[len],
					   std::default_delete<uint8_t[]>());
			len = m_owner.m_handle->GetData(m_index,
											SQL_C_CHAR,
											r.get(),
											len);
			assert(len == 0);
		}
		return r;
	}


	asd_Define_CastOperator_FixSize(char, SQL_C_TINYINT);

	asd_Define_CastOperator_FixSize(short, SQL_C_SHORT);

	asd_Define_CastOperator_FixSize(int, SQL_C_LONG);

	asd_Define_CastOperator_FixSize(int64_t, SQL_C_SBIGINT);

	asd_Define_CastOperator_FixSize(float, SQL_C_FLOAT);

	asd_Define_CastOperator_FixSize(double, SQL_C_DOUBLE);

	asd_Define_CastOperator_FixSize(bool, SQL_C_BIT);

	asd_Define_CastOperator_FixSize(SQL_TIMESTAMP_STRUCT, SQL_C_TIMESTAMP);
}

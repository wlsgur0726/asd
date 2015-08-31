#pragma once
#include "asd/asdbase.h"
#include "asd/time.h"
#include <vector>
#include <list>

namespace asd
{
	struct DBDiagInfo
	{
		char m_state[6] = {0, 0, 0, 0, 0, 0};
		int m_nativeError = 0;
		MString m_message;

		MString ToString() const asd_NoThrow;
	};

	typedef std::list<DBDiagInfo> DBDiagInfoList;



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



	struct DBConnectionHandle;
	typedef std::shared_ptr<DBConnectionHandle> DBConnectionHandle_ptr;
	class DBConnection
	{
		friend class DBStatement;
		DBConnectionHandle_ptr m_handle;

	public:
		void Open(IN const char* a_constr,
				  IN size_t a_constrlen = 0)
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
		void Excute(REF DBConnection& a_conHandle,
					IN const char* a_query,
					IN size_t a_querylen = 0)
			asd_Throws(DBException);


		bool Next()
			asd_Throws(DBException);


		void Close()
			asd_Throws(DBException);


		virtual ~DBStatement()
			asd_NoThrow;


		struct Caster
		{
			DBStatement& m_owner;
			const uint16_t m_index;

			Caster& operator = (IN const Caster&) = delete;

			Caster(REF DBStatement& a_owner,
				   IN uint16_t a_index)
				: m_owner(a_owner)
				, m_index(a_index)
			{
			}


#define asd_DBStatement_Declare_CastOperator(Type)					\
			operator Type()											\
				asd_Throws(DBException, NullDataException);			\
																	\
			operator std::shared_ptr<Type>()						\
				asd_Throws(DBException);							\
																	\
			operator std::unique_ptr<Type>()						\
				asd_Throws(DBException);							\

			asd_DBStatement_Declare_CastOperator(char);
			asd_DBStatement_Declare_CastOperator(short);
			asd_DBStatement_Declare_CastOperator(int);
			asd_DBStatement_Declare_CastOperator(int64_t);
			asd_DBStatement_Declare_CastOperator(float);
			asd_DBStatement_Declare_CastOperator(double);
			asd_DBStatement_Declare_CastOperator(bool);
			asd_DBStatement_Declare_CastOperator(SQL_TIMESTAMP_STRUCT);
			asd_DBStatement_Declare_CastOperator(MString);
			asd_DBStatement_Declare_CastOperator(WString);
			asd_DBStatement_Declare_CastOperator(std::string);
			asd_DBStatement_Declare_CastOperator(std::wstring);
			asd_DBStatement_Declare_CastOperator(SharedArray<uint8_t>);
			asd_DBStatement_Declare_CastOperator(std::vector<uint8_t>);
			asd_DBStatement_Declare_CastOperator(tm);
			asd_DBStatement_Declare_CastOperator(Time);

		};


		template <typename T>
		T* GetData(IN const char* a_columnName,
				   OUT T& a_return)
			asd_Throws(DBException);


		template <typename T>
		T* GetData(IN uint16_t a_columnIndex,
				   OUT T& a_return)
			asd_Throws(DBException);


		Caster GetData(IN uint16_t a_columnIndex)
			asd_Throws(DBException);


		Caster GetData(IN const char* a_columnName)
			asd_Throws(DBException);

	};

}

#pragma once
#include "asd/asdbase.h"
#include <sqltypes.h>
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
			Caster(REF DBStatement& a_owner,
				   IN uint16_t a_index)
				: m_owner(a_owner)
				, m_index(a_index)
			{
			}

#define asd_Declare_CastOperator(Type)						\
			operator Type() asd_Throws(DBException)			\

			asd_Declare_CastOperator(MString);
			asd_Declare_CastOperator(std::shared_ptr<uint8_t>);
			asd_Declare_CastOperator(char);
			asd_Declare_CastOperator(short);
			asd_Declare_CastOperator(int);
			asd_Declare_CastOperator(int64_t);
			asd_Declare_CastOperator(float);
			asd_Declare_CastOperator(double);
			asd_Declare_CastOperator(bool);
			asd_Declare_CastOperator(SQL_TIMESTAMP_STRUCT);

		};

		Caster GetData(IN uint16_t a_columnIndex)
			asd_Throws(DBException);

		Caster GetData(IN const char* a_columnName)
			asd_Throws(DBException);
	};
}
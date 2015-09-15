#include "stdafx.h"
#include "asd/odbcwrap.h"
#include <iostream>


#define Define_OperatorEqual(Type)							\
bool operator == (const Type& left, const Type& right)		\
{															\
	return 0 == std::memcmp(&left, &right, sizeof(Type));	\
}															\

Define_OperatorEqual(tm);
Define_OperatorEqual(SQL_DATE_STRUCT);
Define_OperatorEqual(SQL_TIME_STRUCT);
Define_OperatorEqual(SQL_TIMESTAMP_STRUCT);


namespace asdtest_odbc
{
	enum DBMS
	{
		MSSQL = 0,
		MySQL,
		PostgreSQL,
		_Last
	};


	struct DBString
	{
		asd::MString DBName = "asd_test_db";
		asd::MString TableName = "asd_test_table";
		asd::MString ConnectionString[DBMS::_Last];
		asd::MString CreateTable[DBMS::_Last];

		asd::MString SPName_Insert = "asd_test_sp_insert";
		asd::MString CreateProcedure_Insert[DBMS::_Last];
		asd::MString DropProcedure_Insert[DBMS::_Last];

		asd::MString SPName_Update = "asd_test_sp_update";
		asd::MString CreateProcedure_Update[DBMS::_Last];
		asd::MString DropProcedure_Update[DBMS::_Last];

		asd::MString SPName_Select = "asd_test_sp_select";
		asd::MString CreateProcedure_Select[DBMS::_Last];
		asd::MString DropProcedure_Select[DBMS::_Last];

		DBString()
		{
			// ConnectionString
			ConnectionString[MSSQL] =
				"Driver=SQL Server Native Client 11.0;"
				"Server=.;"
				"Trusted_Connection=Yes;"
				;

			ConnectionString[MySQL] =
				"Driver=MySQL ODBC 5.3 Unicode Driver;"
				"charset=utf8;"
				"Server=127.0.0.1;"
				"UID=root;"
				"PWD=password;"
				;

			ConnectionString[PostgreSQL] =
				"Driver=PostgreSQL Unicode(x64);"
				"Database=postgres;"
				"Server=127.0.0.1;"
				"UID=postgres;"
				"PWD=password;"
				;


			// CreateTable
			CreateTable[MSSQL] = asd::MString() <<
				"CREATE TABLE " << TableName << "(                   \n"
				"  cInt        INT           NOT NULL PRIMARY KEY,   \n"
				"  cBigInt     BIGINT        NULL,                   \n"
				"  cVarchar    NVARCHAR(50)  NULL,                   \n"
				"  cDate       DATE          NULL,                   \n"
				"  cTime       TIME          NULL,                   \n"
				"  cTimestamp  DATETIME      NULL,                   \n"
				"  cBlob       IMAGE         NULL,                   \n"
				"  cFloat      FLOAT(24)     NULL,                   \n"
				"  cDouble     FLOAT(53)     NULL,                   \n"
				"  cBit        BIT           NULL                    \n"
				");                                                  \n"
				;
			CreateTable[MySQL] = asd::MString() <<
				"CREATE TABLE " << TableName << "(                  \n"
				"  cInt        INT          NOT NULL PRIMARY KEY,   \n"
				"  cBigInt     BIGINT       NULL,                   \n"
				"  cVarchar    VARCHAR(50)  NULL,                   \n"
				"  cDate       DATE         NULL,                   \n"
				"  cTime       TIME         NULL,                   \n"
				"  cTimestamp  DATETIME     NULL,                   \n"
				"  cBlob       LONGBLOB     NULL,                   \n"
				"  cFloat      FLOAT        NULL,                   \n"
				"  cDouble     DOUBLE       NULL,                   \n"
				"  cBit        BOOLEAN      NULL                    \n"
				");                                                 \n"
				;
			CreateTable[PostgreSQL] = asd::MString() <<
				"CREATE TABLE " << TableName << "(                       \n"
				"  cInt        INTEGER           NOT NULL PRIMARY KEY,   \n"
				"  cBigInt     BIGINT            NULL,                   \n"
				"  cVarchar    VARCHAR(50)       NULL,                   \n"
				"  cDate       DATE              NULL,                   \n"
				"  cTime       TIME              NULL,                   \n"
				"  cTimestamp  TIMESTAMP         NULL,                   \n"
				"  cBlob       BYTEA             NULL,                   \n"
				"  cFloat      FLOAT             NULL,                   \n"
				"  cDouble     DOUBLE PRECISION  NULL,                   \n"
				"  cBit        BOOLEAN           NULL                    \n"
				");                                                      \n"
				;


			// CreateProcedure_Insert
			// Input Parameter와 Output Parameter를 테스트 하기 위한 SP
			DropProcedure_Insert[MSSQL] = asd::MString() << "DROP PROCEDURE " << SPName_Insert << ";";
			CreateProcedure_Insert[MSSQL] = asd::MString() <<
				"CREATE PROCEDURE " << SPName_Insert << "                                                          \n"
				"  @pInt          INT,                                                                             \n"
				"  @pVarchar      NVARCHAR(50),                                                                    \n"
				"  @pTimestamp    DATETIME,                                                                        \n"
				"  @pInt_out      INT           OUTPUT,                                                            \n"
				"  @pVarchar_out  NVARCHAR(50)  OUTPUT,                                                            \n"
				"  @pDate_out     DATE          OUTPUT,                                                            \n"
				"  @pTime_out     TIME          OUTPUT,                                                            \n"
				"  @pResult       BIT           OUTPUT                                                             \n"
				"AS                                                                                                \n"
				"BEGIN                                                                                             \n"
				"  SET NOCOUNT ON;                                                                                 \n"
				"  SET @pResult = 0;                                                                               \n"
				"                                                                                                  \n"
				"  DECLARE @cnt INT;                                                                               \n"
				"  SELECT @cnt = COUNT(*) FROM " << TableName << " WHERE cInt = @pInt;                             \n"
				"  IF @cnt > 0                                                                                     \n"
				"  BEGIN                                                                                           \n"
				"    RETURN;                                                                                       \n"
				"  END                                                                                             \n"
				"                                                                                                  \n"
				"  INSERT INTO " << TableName << "(cInt, cVarchar, cTimestamp, cDate, cTime)                       \n"
				"  VALUES (@pInt, @pVarchar, @pTimestamp, CAST(@pTimestamp AS DATE), CAST(@pTimestamp AS TIME));   \n"
				"                                                                                                  \n"
				"  SELECT @pInt_out=cInt, @pVarchar_out=cVarchar, @pDate_out=cDate, @pTime_out=cTime               \n"
				"  FROM " << TableName << " WHERE cInt = @pInt;                                                    \n"
				"                                                                                                  \n"
				"  SET @pResult = 1;                                                                               \n"
				"END                                                                                               \n"
				;
			DropProcedure_Insert[MySQL] = asd::MString() << "DROP PROCEDURE " << SPName_Insert << ";";
			CreateProcedure_Insert[MySQL] = asd::MString() <<
				"CREATE PROCEDURE `" << SPName_Insert <<"`(                                                   \n"
				"  IN   pInt          INT,                                                                    \n"
				"  IN   pVarchar      VARCHAR(50),                                                            \n"
				"  IN   pTimestamp    DATETIME,                                                               \n"
				"  OUT  pInt_out      INT,                                                                    \n"
				"  OUT  pVarchar_out  VARCHAR(50),                                                            \n"
				"  OUT  pDate_out     DATE,                                                                   \n"
				"  OUT  pTime_out     TIME,                                                                   \n"
				"  OUT  pResult       BOOLEAN                                                                 \n"
				")                                                                                            \n"
				"PROCBODY:BEGIN                                                                               \n"
				"  DECLARE cnt INT;                                                                           \n"
				"  SET pResult = FALSE;                                                                       \n"
				"                                                                                             \n"
				"  SELECT COUNT(*) INTO cnt FROM " << TableName << " WHERE cInt = pInt;                       \n"
				"  IF cnt > 0 THEN                                                                            \n"
				"    LEAVE PROCBODY;                                                                          \n"
				"  END IF;                                                                                    \n"
				"                                                                                             \n"
				"  INSERT INTO " << TableName << "(cInt, cVarchar, cTimestamp, cDate, cTime)                  \n"
				"  VALUES (pInt, pVarchar, pTimestamp, CAST(pTimestamp AS DATE), CAST(pTimestamp AS TIME));   \n"
				"                                                                                             \n"
				"  SELECT cInt, cVarchar, cDate, cTime                                                        \n"
				"  INTO pInt_out, pVarchar_out, pDate_out, pTime_out                                          \n"
				"  FROM " << TableName << " WHERE cInt = pInt;                                                \n"
				"                                                                                             \n"
				"  SET pResult = TRUE;                                                                        \n"
				"END                                                                                          \n"
				;


			// CreateProcedure_Update
			// InOut Parameter를 테스트 하기 위한 SP
			DropProcedure_Update[MSSQL] = asd::MString() << "DROP PROCEDURE " << SPName_Update << ";";
			CreateProcedure_Update[MSSQL] = asd::MString() <<
				"CREATE PROCEDURE " << SPName_Update << "                                                        \n"
				"  @pInt        INT,                                                                             \n"
				"  @pBigInt     BIGINT        OUTPUT,                                                            \n"
				"  @pVarchar    NVARCHAR(50)  OUTPUT,                                                            \n"
				"  @pTimestamp  DATETIME      OUTPUT,                                                            \n"
				"  @pResult     BIT           OUTPUT                                                             \n"
				"AS                                                                                              \n"
				"BEGIN                                                                                           \n"
				"  SET NOCOUNT ON;                                                                               \n"
				"  SET @pResult = 0;                                                                             \n"
				"                                                                                                \n"
				"  DECLARE @cnt INT;                                                                             \n"
				"  DECLARE @orgBigInt BIGINT;                                                                    \n"
				"  DECLARE @orgVarchar NVARCHAR(50);                                                             \n"
				"  DECLARE @orgTimestamp DATETIME;                                                               \n"
				"                                                                                                \n"
				"  SELECT @cnt=COUNT(*), @orgBigInt=cBigInt, @orgVarchar=cVarchar, @orgTimestamp=cTimestamp      \n"
				"  FROM " << TableName << " WHERE cInt = @pInt                                                   \n"
				"  GROUP BY cBigInt, cVarchar, cTimestamp;                                                       \n"
				"  IF @cnt = 0                                                                                   \n"
				"  BEGIN                                                                                         \n"
				"    RETURN;                                                                                     \n"
				"  END                                                                                           \n"
				"                                                                                                \n"
				"  UPDATE " << TableName << "                                                                    \n"
				"  SET cBigInt=@pBigInt, cVarchar=@pVarchar, cTimestamp=@pTimestamp                              \n"
				"  WHERE cInt = @pInt;                                                                           \n"
				"                                                                                                \n"
				"  SET @pBigInt = @orgBigInt;                                                                    \n"
				"  SET @pVarchar = @orgVarchar;                                                                  \n"
				"  SET @pTimestamp = @orgTimestamp;                                                              \n"
				"  SET @pResult = 1;                                                                             \n"
				"END                                                                                             \n"
				;
			DropProcedure_Update[MySQL] = asd::MString() << "DROP PROCEDURE " << SPName_Update << ";";
			CreateProcedure_Update[MySQL] = asd::MString() <<
				"CREATE PROCEDURE `" << SPName_Update << "`(                       \n"
				"  IN     pInt        INT,                                         \n"
				"  INOUT  pBigInt     bigint,                                      \n"
				"  INOUT  pVarchar    VARCHAR(50),                                 \n"
				"  INOUT  pTimestamp  DATETIME,                                    \n"
				"  OUT    pResult     BOOLEAN                                      \n"
				")                                                                 \n"
				"PROCBODY:BEGIN                                                    \n"
				"  DECLARE cnt INT;                                                \n"
				"  DECLARE orgBigInt BIGINT;                                       \n"
				"  DECLARE orgVarchar VARCHAR(50);                                 \n"
				"  DECLARE orgTimestamp DATETIME;                                  \n"
				"  SET pResult = FALSE;                                            \n"
				"                                                                  \n"
				"  SELECT COUNT(*), cBigInt, cVarchar, cTimestamp                  \n"
				"  INTO cnt, orgBigInt, orgVarchar, orgTimestamp                   \n"
				"  FROM " << TableName << " WHERE cInt = pInt;                     \n"
				"  IF cnt = 0 THEN                                                 \n"
				"    LEAVE PROCBODY;                                               \n"
				"  END IF;                                                         \n"
				"                                                                  \n"
				"  UPDATE " << TableName << "                                      \n"
				"  SET cBigInt=pBigInt, cVarchar=pVarchar, cTimestamp=pTimestamp   \n"
				"  WHERE cInt = pInt;                                              \n"
				"                                                                  \n"
				"  SET pBigInt = orgBigInt;                                        \n"
				"  SET pVarchar = orgVarchar;                                      \n"
				"  SET pTimestamp = orgTimestamp;                                  \n"
				"  SET pResult = TRUE;                                             \n"
				"END                                                               \n"
				;


			// CreateProcedure_Select
			// 복수의 조회 결과를 테스트 하기 위한 SP
			DropProcedure_Select[MSSQL] = asd::MString() << "DROP PROCEDURE " << SPName_Select << ";";
			CreateProcedure_Select[MSSQL] = asd::MString() <<
				"CREATE PROCEDURE " << SPName_Select << "                        \n"
				"  @criterion  INT                                               \n"
				"AS                                                              \n"
				"BEGIN                                                           \n"
				"  SET NOCOUNT ON;                                               \n"
				"  SELECT * FROM " << TableName << " WHERE cInt < @criterion;    \n"
				"  SELECT * FROM " << TableName << " WHERE cInt >= @criterion;   \n"
				"END                                                             \n"
				;
			DropProcedure_Select[MySQL] = asd::MString() << "DROP PROCEDURE " << SPName_Select << ";";
			CreateProcedure_Select[MySQL] = asd::MString() <<
				"CREATE PROCEDURE `" << SPName_Select << "` (                   \n"
				"  IN  criterion  INT                                           \n"
				")                                                              \n"
				"BEGIN                                                          \n"
				"  SELECT * FROM " << TableName << " WHERE cInt < criterion;    \n"
				"  SELECT * FROM " << TableName << " WHERE cInt >= criterion;   \n"
				"END                                                            \n"
				;
		}
	};
	const DBString DBStr;


#define AllowPrint 0
#if AllowPrint
	#define COUT std::cout

#else
	#define COUT NULLOUT()
	struct NULLOUT
	{
		NULLOUT& operator << (const asd::MString&) { return *this; }
	};

#endif


#define UseStdType 1
#if UseStdType
	typedef std::string StringType;
	typedef tm TimeType;
	typedef std::vector<uint8_t> BlobType;

#else
	typedef asd::MString StringType;
	typedef asd::Time TimeType;
	typedef asd::SharedArray<uint8_t> BlobType;

#endif


	template<typename T>
	asd::MString ToStr(T& p)
	{
		return asd::MString() << p;
	}


	template<>
	asd::MString ToStr<bool>(bool& p)
	{
		return p ? "TRUE" : "FALSE";
	}


	template<>
	asd::MString ToStr<TimeType>(TimeType& p)
	{
		return asd::Time(p).ToString();
	}


	template<>
	asd::MString ToStr<BlobType>(BlobType& p)
	{
		const size_t Limit = 512;
		size_t sz = p.size();
		asd::MString r("(%llu bytes)", sz);
		for (int i=0; i<sz; ++i) {
			r += asd::MString(" %02x", p.data()[i]);
			if (i > Limit) {
				r += " ...";
				break;
			}
		}
		return r;
	}


	template<typename T>
	asd::MString ToStr(T* ptr)
	{
		if (ptr == nullptr)
			return "(null)";
		return ToStr(*ptr);
	}



	int64_t TestData_BigInt(int pk)
	{
		return (int64_t)pk * std::numeric_limits<int32_t>::max();
	}


	StringType TestData_String(int pk)
	{
		return asd::MString("test%d", pk);
	}


	TimeType TestData_Time(int pk)
	{
		return asd::Time(2000 + pk,
						 pk % 12 + 1,
						 pk % 30 + 1,
						 (pk+12) % 24,
						 pk % 60,
						 pk % 60);
	}


	BlobType TestData_Blob(int pk)
	{
		int n = 1024 * (std::abs(pk) + 1);
		BlobType blob;
		blob.resize(n);
		for (int i=0; i<n; ++i) {
			blob[i] = i + pk;
		}
		return blob;
	}


	double TestData_double(int pk)
	{
		return pk + 0.123456;
	}


	bool TestData_bool(int pk)
	{
		return pk % 2 == 0;
	}


	/***** ODBCTest ***********************************************************************************/
	struct ODBCTest
	{
#define TestBegin(TestName, ConVarName, StmtVarName)									\
		void TestName ()																\
		{																				\
			{																			\
				const char* FuncName = # TestName ;										\
				COUT << asd::MString("\n[%s] %s:%d\n", FuncName, __FILE__, __LINE__);	\
			}																			\
			try {																		\
				asd::DBConnection ConVarName;											\
				ConVarName.Open(ConnectionString);										\
				asd::DBStatement StmtVarName(ConVarName);								\

#define TestEnd																			\
			}																			\
			catch(std::exception& e) {													\
				printf("Exception! : %s\n", e.what());									\
				throw e;																\
			}																			\
		}																				\

#define GetStr(Str) (DBStr.Str[TestTarget])

		const DBMS TestTarget;
		asd::MString DBName = DBStr.DBName;
		asd::MString TableName = DBStr.TableName;
		asd::MString SPName_Insert = DBStr.SPName_Insert;
		asd::MString SPName_Update = DBStr.SPName_Update;
		asd::MString SPName_Select = DBStr.SPName_Select;
		asd::MString ConnectionString = GetStr(ConnectionString);

		int64_t Execute(asd::DBStatement& stmt,
						const asd::MString& query,
						asd::DBStatement::FetchCallback fetchCallback = nullptr)
		{
			COUT << "[Execute] " << query << '\n';
			if (query.GetLength() > 0) {
				return stmt.Execute(query, fetchCallback);
			}
			else {
				return stmt.Execute(fetchCallback);
			}
		}


		int64_t Execute(asd::DBStatement& stmt,
						asd::DBStatement::FetchCallback fetchCallback = nullptr)
		{
			return Execute(stmt, "", fetchCallback);
		}



		ODBCTest(DBMS target) : TestTarget(target)
		{
			Init();
			CreateDBandTable();
			Test_Insert();
			Test_Update();
			Test_Delete();
			Test_Transaction();

			if (TestTarget == PostgreSQL)
				return;

			Test_StoredProcedure_1();
			Test_StoredProcedure_2();
			Test_StoredProcedure_3();
		}



		~ODBCTest()
		{
			ConnectionString = GetStr(ConnectionString);
			Init();
		}



		TestBegin(Init, con, stmt)
		{
			if (TestTarget == PostgreSQL)
				return;

			try {
				Execute(stmt, asd::MString() << "DROP DATABASE " << DBName);
			}
			catch (std::exception& e) {
				printf("%s\n", e.what());
			}
		}
		TestEnd;



		TestBegin(CreateDBandTable, con, stmt)
		{
			if (TestTarget != PostgreSQL) {
				Execute(stmt, asd::MString() << "CREATE DATABASE " << DBName);

				// ConnectionString에 접속할 DB정보를 추가하고 재접속
				ConnectionString << "Database=" << DBName << ";";
				stmt.Close();
				con.Close();
				con.Open(ConnectionString);
				stmt.Init(con);
			}

			try {
				asd::MString dropTableQuery;
				if (TestTarget == PostgreSQL)
					dropTableQuery << "DROP TABLE " << TableName << " CASCADE";
				else
					dropTableQuery << "DROP TABLE " << TableName;
				Execute(stmt, dropTableQuery);
			}
			catch (std::exception& e) {
				printf("%s\n", e.what());
			}
			Execute(stmt, GetStr(CreateTable));
		}
		TestEnd;



		TestBegin(Test_Insert, con, stmt)
		{
			int pk = 0;
			int loopCount;

			// 생성 직후이므로 아무것도 없어야 한다.
			loopCount = 0;
			Execute(stmt,
					asd::MString() << "SELECT count(*) FROM " << TableName,
					[&](int, int)
					{
						++loopCount;
						int rowCount = stmt.GetData(1);
						COUT << "rowCount : " << rowCount << '\n';
						EXPECT_EQ(rowCount, 0);
					}
			);
			EXPECT_EQ(loopCount, 1);


			// 1. Insert - 생쿼리
			{
				int			cInt		= ++pk;
				StringType	cVarchar	= TestData_String(cInt);
				asd::Time	cTimestamp	= TestData_Time(cInt);
				double		cDouble		= TestData_double(cInt);
				bool		cBit		= TestData_bool(cInt);

				asd::MString cBit_str;
				if (TestTarget == MSSQL)
					cBit_str = cBit ? "1" : "0";
				else
					cBit_str = cBit ? "TRUE" : "FALSE";

				int64_t ret = Execute(stmt,
									  asd::MString() <<
									  "INSERT INTO " << TableName << "("
									  "  cInt,"
									  "  cVarchar,"
									  "  cTimestamp,"
									  "  cDouble,"
									  "  cBit"
									  ") "
									  "VALUES ("
									  << cInt << ", "
									  << "'" << cVarchar << "', "
									  << "'" << cTimestamp.ToString("%Y-%m-%d %H:%M:%S") << "', "
									  << cDouble << ", "
									  << cBit_str <<
									  ");");
				EXPECT_EQ(ret, 1);
			}


			// 2. Insert - SetParam 방식
			{
				// 2-1. 입력할 변수 초기화
				int			cInt		= ++pk;
				StringType	cVarchar	= TestData_String(cInt);
				TimeType	cTimestamp	= TestData_Time(cInt);
				double		cDouble		= TestData_double(cInt);
				bool		cBit		= TestData_bool(cInt);

				// 2-2. SetParam.
				//      Prepare가 아니라서 파라메터 정보 조회가 불가능하므로
				//      Type, ColumnSize, Scale 등의 정보를 직접 입력해줘야 한다.
				uint16_t i = 0;
				stmt.SetInParam(++i, cInt, asd::SQLType::INTEGER);
				stmt.SetInParam(++i, cVarchar, asd::SQLType::WVARCHAR);
				stmt.SetInParam(++i, cTimestamp, asd::SQLType::TIMESTAMP);
				stmt.SetInParam(++i, cDouble, asd::SQLType::DOUBLE);
				stmt.SetInParam(++i, cBit, asd::SQLType::BIT);
				stmt.SetInParam_NullInput<int64_t>(++i, asd::SQLType::BIGINT); // null 입력

				// 2-3. Execute
				int64_t ret = Execute(stmt,
									  asd::MString() <<
									  "INSERT INTO " << TableName << "("
									  "  cInt,"
									  "  cVarchar,"
									  "  cTimestamp,"
									  "  cDouble,"
									  "  cBit,"
									  "  cBigInt"
									  ") "
									  "VALUES ("
									  " ?, ?, ?, ?, ?, ?"
									  ");");
				EXPECT_EQ(ret, 1);
			}


			// 3. Insert - BindParam 방식
			{
				int cInt;
				asd::MString cVarchar;
				TimeType cTimestamp;
				double cDouble;
				bool cBit;

				// 3-1. 변수 바인딩.
				//      Prepare가 아니라서 파라메터 정보 조회가 불가능하므로
				//      Type, ColumnSize, Scale 등의 정보를 직접 입력해줘야 한다.
				uint16_t i = 0;
				stmt.BindInParam(++i, &cInt, asd::SQLType::INTEGER);
				stmt.BindInParam(++i, &cVarchar, asd::SQLType::WVARCHAR);
				stmt.BindInParam(++i, &cTimestamp, asd::SQLType::TIMESTAMP);
				stmt.BindInParam(++i, &cDouble, asd::SQLType::DOUBLE);
				stmt.BindInParam(++i, &cBit, asd::SQLType::BIT);
				stmt.BindInParam<int64_t>(++i, nullptr, asd::SQLType::BIGINT); // null 입력

				// 3-2. 바인딩한 변수에 입력값 셋팅
				cInt		= ++pk;
				cVarchar	= TestData_String(cInt);
				cTimestamp	= TestData_Time(cInt);
				cDouble		= TestData_double(cInt);
				cBit		= TestData_bool(cInt);

				// 3-3. Execute
				int64_t ret = Execute(stmt,
									  asd::MString() <<
									  "INSERT INTO " << TableName << "("
									  "  cInt,"
									  "  cVarchar,"
									  "  cTimestamp,"
									  "  cDouble,"
									  "  cBit,"
									  "  cBigInt"
									  ") "
									  "VALUES ("
									  " ?, ?, ?, ?, ?, ?"
									  ");");
				EXPECT_EQ(ret, 1);
			}


			// 4. Insert - Prepare + SetParam 방식
			{
				// 4-1. 입력할 변수 초기화
				int			cInt		= ++pk;
				StringType	cVarchar	= TestData_String(cInt);
				TimeType	cTimestamp	= TestData_Time(cInt);
				double		cDouble		= TestData_double(cInt);
				bool		cBit		= TestData_bool(cInt);

				// 4-2. Prepare
				stmt.Prepare(asd::MString() <<
							 "INSERT INTO " << TableName << "("
							 "  cInt,"
							 "  cVarchar,"
							 "  cTimestamp,"
							 "  cDouble,"
							 "  cBit,"
							 "  cBigInt"
							 ") "
							 "VALUES ("
							 " ?, ?, ?, ?, ?, ?"
							 ");");

				// 4-3. SetParam 
				uint16_t i = 0;
				stmt.SetInParam(++i, cInt);
				stmt.SetInParam(++i, cVarchar);
				stmt.SetInParam(++i, cTimestamp);
				stmt.SetInParam(++i, cDouble);
				stmt.SetInParam(++i, cBit);
				stmt.SetInParam_NullInput<int64_t>(++i); // null 입력

				// 4-4. Execute
				int64_t ret = Execute(stmt);
				EXPECT_EQ(ret, 1);
			}


			// 5. Insert - Prepare + BindParam 방식
			{ 
				// 5-1. Prepare
				stmt.Prepare(asd::MString() <<
							 "INSERT INTO " << TableName << "("
							 "  cInt,"
							 "  cVarchar,"
							 "  cTimestamp,"
							 "  cDouble,"
							 "  cBit,"
							 "  cBigInt"
							 ") "
							 "VALUES ("
							 " ?, ?, ?, ?, ?, ?"
							 ");");

				int cInt;
				asd::MString cVarchar;
				asd::Time cTimestamp;
				double cDouble;
				bool cBit;

				// 5-2. 변수 바인딩
				uint16_t i = 0;
				stmt.BindInParam(++i, &cInt);
				stmt.BindInParam(++i, &cVarchar);
				stmt.BindInParam(++i, &cTimestamp);
				stmt.BindInParam(++i, &cDouble);
				stmt.BindInParam(++i, &cBit);
				stmt.BindInParam<int64_t>(++i, nullptr); // null 입력

				// 5-3. 바인딩한 변수에 입력값 셋팅
				cInt		= ++pk;
				cVarchar	= TestData_String(cInt);
				cTimestamp	= TestData_Time(cInt);
				cDouble		= TestData_double(cInt);
				cBit		= TestData_bool(cInt);

				// 5-4. Execute
				int64_t ret = Execute(stmt);
				EXPECT_EQ(ret, 1);
			}


			// 결과 확인
			{
				stmt.ClearParam();
				loopCount = 0;
				Execute(stmt,
						asd::MString() << "SELECT * FROM " << TableName,
						[&](const int Result, const int Record)
						{
							EXPECT_EQ(Result, 1);
							EXPECT_EQ(Record, ++loopCount);
							COUT << "Result:" << Result << ", Record:" << Record << '\n';
							int pk = stmt.GetData("cInt");

							uint16_t colCount = stmt.GetColumnCount();
							for (uint16_t i=1; i<=colCount; ++i) {
								asd::MString colName = stmt.GetColumnName(i);
								asd::MString colValue;
								asd::equal_to_String<char, false> Equal;
								if (Equal(colName, "cInt")) {
									colValue = ToStr(pk);
								}
								else if (Equal(colName, "cBigInt")) {
									int64_t* val = stmt.GetData(colName);
									colValue = ToStr(val);
									EXPECT_EQ(val, nullptr);
								}
								else if (Equal(colName, "cVarchar")) {
									StringType* val = stmt.GetData(colName);
									colValue = ToStr(val);
									EXPECT_NE(val, nullptr);
									EXPECT_EQ(TestData_String(pk), *val);
								}
								else if (Equal(colName, "cDate")) {
									TimeType* val = stmt.GetData(colName);
									colValue = ToStr(val);
									EXPECT_EQ(val, nullptr);
								}
								else if (Equal(colName, "cTime")) {
									TimeType* val = stmt.GetData(colName);
									colValue = ToStr(val);
									EXPECT_EQ(val, nullptr);
								}
								else if (Equal(colName, "cTimestamp")) {
									TimeType* val = stmt.GetData(colName);
									colValue = ToStr(val);
									EXPECT_NE(val, nullptr);
									EXPECT_EQ(TestData_Time(pk), *val);
								}
								else if (Equal(colName, "cBlob")) {
									BlobType* val = stmt.GetData(colName);
									colValue = ToStr(val);
									EXPECT_EQ(val, nullptr);
								}
								else if (Equal(colName, "cFloat")) {
									float* val = stmt.GetData(colName);
									colValue = ToStr(val);
									EXPECT_EQ(val, nullptr);
								}
								else if (Equal(colName, "cDouble")) {
									double* val = stmt.GetData(colName);
									colValue = ToStr(val);
									EXPECT_NE(val, nullptr);
									EXPECT_EQ(TestData_double(pk), *val);
								}
								else if (Equal(colName, "cBit")) {
									bool* val = stmt.GetData(colName);
									colValue = ToStr(val);
									EXPECT_NE(val, nullptr);
									EXPECT_EQ(TestData_bool(pk), *val);
								}
								else {
									asd_RaiseException("invalid colName : %s", colName.GetData());
								}

								COUT << asd::MString("  %10s  :  %s\n",
													 colName.GetData(),
													 colValue.GetData());
							}
							COUT << '\n';
						}
				);
				EXPECT_EQ(loopCount, pk);
			}
		}
		TestEnd;



		TestBegin(Test_Update, con, stmt)
		{
			int pk = 0;
			int loopCount;

			// 앞선 Insert Test에서 입력한 데이터들이 남아있어야 한다.
			loopCount = 0;
			Execute(stmt,
					asd::MString() << "SELECT count(*) FROM " << TableName,
					[&](int, int)
					{
						++loopCount;
						int rowCount = stmt.GetData(1);
						COUT << "rowCount : " << rowCount << '\n';
						EXPECT_EQ(rowCount, 5);
					}
			);
			EXPECT_EQ(loopCount, 1);


			// 1. Update - SetParam 방식
			{
				// 1-1. 입력할 변수 초기화
				int			cInt	= ++pk;
				BlobType	cBlob	= TestData_Blob(cInt);

				// 1-2. SetParam.
				//      Prepare가 아니라서 파라메터 정보 조회가 불가능하므로
				//      Type, ColumnSize, Scale 등의 정보를 직접 입력해줘야 한다.
				int i = 0;
				stmt.SetInParam_NullInput<bool>(++i, asd::SQLType::BIT);
				stmt.SetInParam(++i, cBlob, asd::SQLType::BINARY);
				stmt.SetInParam(++i, cInt, asd::SQLType::INTEGER);

				// 2-3. Execute
				int64_t ret = Execute(stmt,
									  asd::MString()
									  << "UPDATE " << TableName << " "
									  << "SET"
									  << "  cBit = ?,"
									  << "  cBlob = ? "
									  << "WHERE cInt = ?");
				EXPECT_EQ(ret, 1);
			}


			// 2. Update - BindParam 방식
			{
				int			cInt;
				bool*		cBit = nullptr;
				BlobType	cBlob;

				// 2-1. 변수 바인딩.
				//      Prepare가 아니라서 파라메터 정보 조회가 불가능하므로
				//      Type, ColumnSize, Scale 등의 정보를 직접 입력해줘야 한다.
				int i = 0;
				stmt.BindInParam(++i, cBit, asd::SQLType::BIT);
				stmt.BindInParam(++i, &cBlob, asd::SQLType::BINARY);
				stmt.BindInParam(++i, &cInt, asd::SQLType::INTEGER);

				// 2-2. 바인딩한 변수에 입력값 셋팅
				cInt	= ++pk;
				cBlob	= TestData_Blob(cInt);

				// 2-3. Execute
				int64_t ret = Execute(stmt,
									  asd::MString()
									  << "UPDATE " << TableName << " "
									  << "SET"
									  << "  cBit = ?,"
									  << "  cBlob = ? "
									  << "WHERE cInt = ?");
				EXPECT_EQ(ret, 1);
			}


			// 3. Update - Prepare + SetParam 방식
			{
				// 3-1. Prepare
				stmt.Prepare(asd::MString()
							 << "UPDATE " << TableName << " "
							 << "SET"
							 << "  cBit = ?,"
							 << "  cBlob = ? "
							 << "WHERE cInt = ?");

				// 3-2. 입력할 변수 초기화
				int			cInt	= ++pk;
				bool*		cBit	= nullptr;
				BlobType	cBlob	= TestData_Blob(cInt);

				// 3-3. SetParam
				int i = 0;
				stmt.SetInParam_NullInput<bool>(++i);
				stmt.SetInParam(++i, cBlob);
				stmt.SetInParam(++i, cInt);

				// 3-4. Execute
				int64_t ret = Execute(stmt);
				EXPECT_EQ(ret, 1);
			}


			// 4. Update - Prepare + BindParam 방식
			{
				// 4-1. Prepare
				stmt.Prepare(asd::MString()
							 << "UPDATE " << TableName << " "
							 << "SET"
							 << "  cBit = ?,"
							 << "  cBlob = ? "
							 << "WHERE cInt = ?");

				int			cInt;
				bool*		cBit = nullptr;
				BlobType	cBlob;

				// 4-2. 변수 바인딩
				int i = 0;
				stmt.BindInParam(++i, cBit);
				stmt.BindInParam(++i, &cBlob);
				stmt.BindInParam(++i, &cInt);

				// 4-3. 바인딩한 변수에 입력값 셋팅
				cInt	= ++pk;
				cBlob	= TestData_Blob(cInt);

				// 4-4. Execute
				int64_t ret = Execute(stmt);
				EXPECT_EQ(ret, 1);

				// 4-5. 5번도 업데이트해준다.
				cInt	= ++pk;
				cBlob	= TestData_Blob(cInt);
				ret = Execute(stmt);
				EXPECT_EQ(ret, 1);
			}


			// 결과 확인
			{
				stmt.ClearParam();
				loopCount = 0;
				Execute(stmt, 
						asd::MString() << "SELECT * FROM " << TableName,
						[&](const int Result, const int Record)
						{
							EXPECT_EQ(Result, 1);
							EXPECT_EQ(Record, ++loopCount);
							COUT << "Result:" << Result << ", Record:" << Record << '\n';
							int pk = stmt.GetData("cInt");

							uint16_t colCount = stmt.GetColumnCount();
							for (uint16_t i=1; i<=colCount; ++i) {
								asd::MString colName = stmt.GetColumnName(i);
								asd::MString colValue;
								asd::equal_to_String<char, false> Equal;
								if (Equal(colName, "cInt")) {
									colValue = ToStr(pk);
								}
								else if (Equal(colName, "cBigInt")) {
									int64_t* val = stmt.GetData(colName);
									colValue = ToStr(val);
									EXPECT_EQ(val, nullptr);
								}
								else if (Equal(colName, "cVarchar")) {
									StringType* val = stmt.GetData(colName);
									colValue = ToStr(val);
									EXPECT_NE(val, nullptr);
									EXPECT_EQ(TestData_String(pk), *val);
								}
								else if (Equal(colName, "cDate")) {
									TimeType* val = stmt.GetData(colName);
									colValue = ToStr(val);
									EXPECT_EQ(val, nullptr);
								}
								else if (Equal(colName, "cTime")) {
									TimeType* val = stmt.GetData(colName);
									colValue = ToStr(val);
									EXPECT_EQ(val, nullptr);
								}
								else if (Equal(colName, "cTimestamp")) {
									TimeType* val = stmt.GetData(colName);
									colValue = ToStr(val);
									EXPECT_NE(val, nullptr);
									EXPECT_EQ(TestData_Time(pk), *val);
								}
								else if (Equal(colName, "cBlob")) {
									BlobType* val = stmt.GetData(colName);
									colValue = ToStr(val);
									EXPECT_NE(val, nullptr);
									EXPECT_EQ(TestData_Blob(pk), *val);
								}
								else if (Equal(colName, "cFloat")) {
									float* val = stmt.GetData(colName);
									colValue = ToStr(val);
									EXPECT_EQ(val, nullptr);
								}
								else if (Equal(colName, "cDouble")) {
									double* val = stmt.GetData(colName);
									colValue = ToStr(val);
									EXPECT_NE(val, nullptr);
									EXPECT_EQ(TestData_double(pk), *val);
								}
								else if (Equal(colName, "cBit")) {
									bool* val = stmt.GetData(colName);
									colValue = ToStr(val);
									EXPECT_EQ(val, nullptr);
								}
								else {
									asd_RaiseException("invalid colName : %s", colName.GetData());
								}

								COUT << asd::MString("  %10s  :  %s\n",
													 colName.GetData(),
													 colValue.GetData());
							}
							COUT << '\n';
						}
				);
				EXPECT_EQ(loopCount, pk);
			}
		}
		TestEnd;



		TestBegin(Test_Delete, con, stmt)
		{
			// Delete
			{
				int64_t ret = Execute(stmt,
									  asd::MString()
									  << "DELETE FROM " << TableName << " "
									  << "WHERE cInt % 2 = 0");
				// 영향을 받은(삭제된) 행 수가 리턴된다.
				EXPECT_EQ(ret, 2);
			}

			// 결과 확인
			{
				stmt.ClearParam();
				int loopCount = 0;
				Execute(stmt, 
						asd::MString() << "SELECT * FROM " << TableName,
						[&](const int ResNum, const int RecNum)
						{
							EXPECT_EQ(ResNum, 1);
							EXPECT_EQ(RecNum, ++loopCount);
							COUT << "Result:" << ResNum << ", Record:" << RecNum << '\n';
							int pk = stmt.GetData("cInt");
							EXPECT_TRUE(pk % 2 == 1);

							uint16_t colCount = stmt.GetColumnCount();
							for (uint16_t i=1; i<=colCount; ++i) {
								asd::MString colName = stmt.GetColumnName(i);
								asd::MString colValue;
								asd::equal_to_String<char, false> Equal;
								if (Equal(colName, "cInt")) {
									colValue = ToStr(pk);
								}
								else if (Equal(colName, "cBigInt")) {
									int64_t* val = stmt.GetData(colName);
									colValue = ToStr(val);
									EXPECT_EQ(val, nullptr);
								}
								else if (Equal(colName, "cVarchar")) {
									StringType* val = stmt.GetData(colName);
									colValue = ToStr(val);
									EXPECT_NE(val, nullptr);
									EXPECT_EQ(TestData_String(pk), *val);
								}
								else if (Equal(colName, "cDate")) {
									TimeType* val = stmt.GetData(colName);
									colValue = ToStr(val);
									EXPECT_EQ(val, nullptr);
								}
								else if (Equal(colName, "cTime")) {
									TimeType* val = stmt.GetData(colName);
									colValue = ToStr(val);
									EXPECT_EQ(val, nullptr);
								}
								else if (Equal(colName, "cTimestamp")) {
									TimeType* val = stmt.GetData(colName);
									colValue = ToStr(val);
									EXPECT_NE(val, nullptr);
									EXPECT_EQ(TestData_Time(pk), *val);
								}
								else if (Equal(colName, "cBlob")) {
									BlobType* val = stmt.GetData(colName);
									colValue = ToStr(val);
									EXPECT_NE(val, nullptr);
									EXPECT_EQ(TestData_Blob(pk), *val);
								}
								else if (Equal(colName, "cFloat")) {
									float* val = stmt.GetData(colName);
									colValue = ToStr(val);
									EXPECT_EQ(val, nullptr);
								}
								else if (Equal(colName, "cDouble")) {
									double* val = stmt.GetData(colName);
									colValue = ToStr(val);
									EXPECT_NE(val, nullptr);
									EXPECT_EQ(TestData_double(pk), *val);
								}
								else if (Equal(colName, "cBit")) {
									bool* val = stmt.GetData(colName);
									colValue = ToStr(val);
									EXPECT_EQ(val, nullptr);
								}
								else {
									asd_RaiseException("invalid colName : %s", colName.GetData());
								}

								COUT << asd::MString("  %10s  :  %s\n",
													 colName.GetData(),
													 colValue.GetData());
							}
							COUT << '\n';
						}
				);
				EXPECT_EQ(loopCount, 3);
			}
		}
		TestEnd;



		TestBegin(Test_Transaction, con, stmt)
		{
			int loopCount;
			bool check;
			asd::MString selectQuery = asd::MString() << "SELECT cInt, cVarchar FROM " << TableName << " WHERE cInt = 100";

			auto Reconnect =[&]()
			{
				stmt.Close();
				con.Close();
				con.Open(ConnectionString);
				stmt.Init(con);
			};


			// 1. 커밋 테스트
			{
				// 1-1. BeginTran 후 Insert
				con.BeginTran();
				stmt.Prepare(asd::MString() <<
							 "INSERT INTO " << TableName << "(cInt, cVarchar) "
							 "VALUES (100, ?)");
				stmt.SetInParam(1, TestData_String(100));
				EXPECT_EQ(Execute(stmt), 1);

				// 1-2. 커밋
				con.CommitTran();

				// 1-3. 확인
				check = false;
				Reconnect();
				Execute(stmt, selectQuery, [&](const int Result, const int Record)
				{
					int cInt = stmt.GetData("cInt");
					asd::MString cVarchar = stmt.GetData("cVarchar");
					EXPECT_EQ(cInt, 100);
					EXPECT_EQ(cVarchar, TestData_String(100));
					check = true;
				});
				EXPECT_TRUE(check);
			}


			// 2. 롤백 테스트 - 명시적으로 롤백
			{
				// 2-1. BeginTran 후 수정
				con.BeginTran();
				stmt.Prepare(asd::MString() <<
							 "UPDATE " << TableName << " "
							 "SET cVarchar = ? "
							 "WHERE cInt = 100");
				stmt.SetInParam(1, TestData_String(101));
				EXPECT_EQ(Execute(stmt), 1);

				// 2-2. 확인 1
				check = false;
				Execute(stmt, selectQuery, [&](const int Result, const int Record)
				{
					int cInt = stmt.GetData("cInt");
					asd::MString cVarchar = stmt.GetData("cVarchar");
					EXPECT_EQ(cInt, 100);
					EXPECT_EQ(cVarchar, TestData_String(101));
					EXPECT_NE(TestData_String(100), TestData_String(101));
					check = true;
				});
				EXPECT_TRUE(check);

				// 2-3. 롤백
				con.RollbackTran();

				// 2-4. 확인 2
				check = false;
				Execute(stmt, selectQuery, [&](const int Result, const int Record)
				{
					int cInt = stmt.GetData("cInt");
					asd::MString cVarchar = stmt.GetData("cVarchar");
					EXPECT_EQ(cInt, 100);
					EXPECT_EQ(cVarchar, TestData_String(100));
					check = true;
				});
				EXPECT_TRUE(check);
			}


			// 3. 롤백 테스트 - Connection 종료로 인한 자동 롤백
			{
				// 3-1. BeginTran 후 수정
				con.BeginTran();
				stmt.Prepare(asd::MString() <<
							 "UPDATE " << TableName << " "
							 "SET cVarchar = ? "
							 "WHERE cInt = 100");
				stmt.SetInParam(1, TestData_String(101));
				EXPECT_EQ(Execute(stmt), 1);

				// 3-2. 확인 1
				check = false;
				Execute(stmt, selectQuery, [&](const int Result, const int Record)
				{
					int cInt = stmt.GetData("cInt");
					asd::MString cVarchar = stmt.GetData("cVarchar");
					EXPECT_EQ(cInt, 100);
					EXPECT_EQ(cVarchar, TestData_String(101));
					EXPECT_NE(TestData_String(100), TestData_String(101));
					check = true;
				});
				EXPECT_TRUE(check);

				// 3-3. 재접속
				Reconnect();

				// 3-4. 확인 2
				check = false;
				Execute(stmt, selectQuery, [&](const int Result, const int Record)
				{
					int cInt = stmt.GetData("cInt");
					asd::MString cVarchar = stmt.GetData("cVarchar");
					EXPECT_EQ(cInt, 100);
					EXPECT_EQ(cVarchar, TestData_String(100));
					check = true;
				});
				EXPECT_TRUE(check);
			}


			// 4. 제거
			{
				auto deleteQuery = asd::MString() << "DELETE FROM " << TableName << " WHERE cInt = 100";
				EXPECT_EQ(Execute(stmt, deleteQuery), 1);
				
				Reconnect();

				check = false;
				Execute(stmt, selectQuery, [&](const int Result, const int Record)
				{
					check = true;
				});
				EXPECT_FALSE(check);
			}
		}
		TestEnd;



		TestBegin(Test_StoredProcedure_1, con, stmt)
		{
			// 기존에 SP가 남아있다면 DROP
			try {
				Execute(stmt, GetStr(DropProcedure_Insert));
			}
			catch (std::exception& e) {
				printf("%s\n", e.what());
			}

			// SP 생성
			Execute(stmt, GetStr(CreateProcedure_Insert));
			asd::MString query = asd::MString("{ CALL %s(?,?,?,?,?,?,?,?) }",
											  SPName_Insert.data());

			// SetParam 방식
			{
				// 1. Prepare
				stmt.Prepare(query);

				// 2. SetParam
				const int pk = 200;
				const auto InsertString = TestData_String(pk);
				const auto InsertTime = TestData_Time(pk);
				const asd::Time InsertTime2 = InsertTime;

				int i = 0;
				stmt.SetInParam(++i, pk);
				stmt.SetInParam(++i, InsertString);
				stmt.SetInParam(++i, InsertTime);
				stmt.SetOutParam<int>(++i);
				stmt.SetOutParam<StringType>(++i);
				stmt.SetOutParam<TimeType>(++i);
				stmt.SetOutParam<TimeType>(++i);
				stmt.SetOutParam<bool>(++i);

				// 3. Execute
				Execute(stmt);


				// 4. 결과 확인
				std::unique_ptr<int> pInt_out = stmt.GetParam(4);
				std::unique_ptr<StringType> pVarchar_out = stmt.GetParam(5);
				std::unique_ptr<TimeType> pDate_out = stmt.GetParam(6);
				std::unique_ptr<TimeType> pTime_out = stmt.GetParam(7);
				std::unique_ptr<bool> pResult = stmt.GetParam(8);

				COUT << "pInt_out      :  " << ToStr(pInt_out.get()) << '\n';
				COUT << "pVarchar_out  :  " << ToStr(pVarchar_out.get()) << '\n';
				COUT << "pDate_out     :  " << ToStr(pDate_out.get()) << '\n';
				COUT << "pTime_out     :  " << ToStr(pTime_out.get()) << '\n';
				COUT << "pResult       :  " << ToStr(pResult.get()) << '\n';

				EXPECT_NE(nullptr, pInt_out);
				EXPECT_NE(nullptr, pVarchar_out);
				EXPECT_NE(nullptr, pDate_out);
				EXPECT_NE(nullptr, pTime_out);
				EXPECT_NE(nullptr, pResult);

				EXPECT_EQ(*pInt_out, pk);
				EXPECT_EQ(*pVarchar_out, InsertString);
				EXPECT_EQ(*pResult, true);

				asd::Time date = *pDate_out;
				EXPECT_EQ(date.Year(), InsertTime2.Year());
				EXPECT_EQ(date.Month(), InsertTime2.Month());
				EXPECT_EQ(date.Day(), InsertTime2.Day());

				asd::Time time = *pTime_out;
				EXPECT_EQ(time.Hour(), InsertTime2.Hour());
				EXPECT_EQ(time.Minute(), InsertTime2.Minute());
				EXPECT_EQ(time.Second(), InsertTime2.Second());
			}


			// BindParam 방식
			{
				// 1. Prepare
				stmt.Prepare(query);
				
				// 2. BindParam
				int			pInt;
				StringType	pVarchar;
				TimeType	pTimestamp;
				int			pInt_out;
				StringType	pVarchar_out;
				TimeType	pDate_out;
				TimeType	pTime_out;
				bool		pResult;

				int i = 0;
				stmt.BindInParam(++i, &pInt);
				stmt.BindInParam(++i, &pVarchar);
				stmt.BindInParam(++i, &pTimestamp);
				stmt.BindOutParam<int>(++i, &pInt_out);
				stmt.BindOutParam<StringType>(++i, &pVarchar_out);
				stmt.BindOutParam<TimeType>(++i, &pDate_out);
				stmt.BindOutParam<TimeType>(++i, &pTime_out);
				stmt.BindOutParam<bool>(++i, &pResult);

				// 3. Set InputValue
				const int pk = 201;
				const auto InsertString = TestData_String(pk);
				const auto InsertTime = TestData_Time(pk);
				const asd::Time InsertTime2 = InsertTime;
				pInt		= pk;
				pVarchar	= InsertString;
				pTimestamp	= InsertTime;

				// 4. Execute
				Execute(stmt);

				// 5. 결과 확인
				COUT << "pInt_out      :  " << ToStr(pInt_out) << '\n';
				COUT << "pVarchar_out  :  " << ToStr(pVarchar_out) << '\n';
				COUT << "pDate_out     :  " << ToStr(pDate_out) << '\n';
				COUT << "pTime_out     :  " << ToStr(pTime_out) << '\n';
				COUT << "pResult       :  " << ToStr(pResult) << '\n';

				EXPECT_FALSE(stmt.IsNullParam(&pInt_out));
				EXPECT_FALSE(stmt.IsNullParam(&pVarchar_out));
				EXPECT_FALSE(stmt.IsNullParam(&pDate_out));
				EXPECT_FALSE(stmt.IsNullParam(&pTime_out));
				EXPECT_FALSE(stmt.IsNullParam(&pResult));

				EXPECT_EQ(pInt, pk);
				EXPECT_EQ(pVarchar, InsertString);
				EXPECT_EQ(pTimestamp, InsertTime);
				EXPECT_EQ(pInt_out, pk);
				EXPECT_EQ(pVarchar_out, InsertString);
				EXPECT_EQ(pResult, true);

				asd::Time date = pDate_out;
				EXPECT_EQ(date.Year(), InsertTime2.Year());
				EXPECT_EQ(date.Month(), InsertTime2.Month());
				EXPECT_EQ(date.Day(), InsertTime2.Day());

				asd::Time time = pTime_out;
				EXPECT_EQ(time.Hour(), InsertTime2.Hour());
				EXPECT_EQ(time.Minute(), InsertTime2.Minute());
				EXPECT_EQ(time.Second(), InsertTime2.Second());
			}
		}
		TestEnd;



		TestBegin(Test_StoredProcedure_2, con, stmt)
		{
			// 기존에 SP가 남아있다면 DROP
			try {
				Execute(stmt, GetStr(DropProcedure_Update));
			}
			catch (std::exception& e) {
				printf("%s\n", e.what());
			}

			// SP 생성
			Execute(stmt, GetStr(CreateProcedure_Update));
			asd::MString query;
			query = asd::MString("{ CALL %s(?,?,?,?,?) }",
								 SPName_Update.data());

			// SetParam 방식
			{
				// 1. Prepare
				stmt.Prepare(query);

				// 2. SetParam
				const int pk = 200;
				const int64_t UpdateBigInt = TestData_BigInt(pk);
				const auto UpdateString = TestData_String(pk + 1);
				const auto OrgString = TestData_String(pk);
				const auto OrgTime = TestData_Time(pk);
				EXPECT_NE(UpdateString, OrgString);

				int i = 0;
				stmt.SetInParam(++i, pk);
				stmt.SetInOutParam(++i, UpdateBigInt);
				stmt.SetInOutParam(++i, UpdateString);
				stmt.SetInOutParam_NullInput<TimeType>(++i); // null 입력
				stmt.SetOutParam<bool>(++i);

				// 3. Execute
				Execute(stmt);

				// 4. 결과 확인
				std::unique_ptr<int64_t> pBigInt = stmt.GetParam(2);
				std::unique_ptr<StringType> pVarchar = stmt.GetParam(3);
				std::unique_ptr<TimeType> pTimestamp = stmt.GetParam(4);
				std::unique_ptr<bool> pResult = stmt.GetParam(5);

				COUT << "pBigInt     :  " << ToStr(pBigInt.get()) << '\n';
				COUT << "pVarchar    :  " << ToStr(pVarchar.get()) << '\n';
				COUT << "pTimestamp  :  " << ToStr(pTimestamp.get()) << '\n';
				COUT << "pResult     :  " << ToStr(pResult.get()) << '\n';

				EXPECT_EQ(pBigInt, nullptr);
				
				EXPECT_NE(pVarchar, nullptr);
				EXPECT_EQ(*pVarchar, OrgString);

				EXPECT_NE(pTimestamp, nullptr);
				EXPECT_EQ(*pTimestamp, OrgTime);

				EXPECT_NE(pResult, nullptr);
				EXPECT_EQ(*pResult, true);
			}


			// BindParam 방식
			{
				// 1. Prepare
				stmt.Prepare(query);

				// 2. BindParam
				int			pInt;
				int64_t		pBigInt;
				StringType	pVarchar;
				TimeType	pTimestamp;
				bool		pResult;

				int i = 0;
				stmt.BindInParam(++i, &pInt);
				stmt.BindInOutParam(++i, pBigInt, false);
				stmt.BindInOutParam(++i, pVarchar, false);
				stmt.BindInOutParam(++i, pTimestamp, true); // null 입력
				stmt.BindOutParam<bool>(++i, &pResult);

				// 3. Set InputValue
				const int pk = 201;
				const int64_t UpdateBigInt = TestData_BigInt(pk);
				const auto UpdateString = TestData_String(pk + 1);
				const auto OrgString = TestData_String(pk);
				const auto OrgTime = TestData_Time(pk);
				EXPECT_NE(UpdateString, OrgString);
				pInt		= pk;
				pBigInt		= UpdateBigInt;
				pVarchar	= UpdateString;
				pTimestamp;

				// 4. Execute
				Execute(stmt);

				// 5. 결과 확인
				COUT << "pBigInt     :  " << ToStr(pBigInt) << '\n';
				COUT << "pVarchar    :  " << ToStr(pVarchar) << '\n';
				COUT << "pTimestamp  :  " << ToStr(pTimestamp) << '\n';
				COUT << "pResult     :  " << ToStr(pResult) << '\n';

				EXPECT_TRUE(stmt.IsNullParam(&pBigInt));
				EXPECT_FALSE(stmt.IsNullParam(&pVarchar));
				EXPECT_FALSE(stmt.IsNullParam(&pTimestamp));
				EXPECT_FALSE(stmt.IsNullParam(&pResult));

				EXPECT_EQ(pBigInt, UpdateBigInt); // null이 리턴되었으므로 입력값과 동일
				EXPECT_EQ(pVarchar, OrgString);
				EXPECT_EQ(pTimestamp, OrgTime);
				EXPECT_EQ(pResult, true);
			}
		}
		TestEnd;



		TestBegin(Test_StoredProcedure_3, con, stmt)
		{
			// 기존에 SP가 남아있다면 DROP
			try {
				Execute(stmt, GetStr(DropProcedure_Select));
			}
			catch (std::exception& e) {
				printf("%s\n", e.what());
			}

			// SP 생성
			Execute(stmt, GetStr(CreateProcedure_Select));
			asd::MString query;
			query = asd::MString("{ CALL %s(?) }",
								 SPName_Select.data());

			int loop = 0;
			int res1 = 0;
			int res2 = 0;
			const int Criterion = 200;
			stmt.SetInParam(1, Criterion, asd::SQLType::INTEGER);
			Execute(stmt, query, [&](const int Result, const int Record)
			{
				++loop;
				COUT << "Result:" << Result << ", Record:" << Record << '\n';
				const int cInt = stmt.GetData("cInt");
				const asd::Time cmp = TestData_Time(cInt);

				uint16_t colCount = stmt.GetColumnCount();
				for (uint16_t i=1; i<=colCount; ++i) {
					asd::MString colName = stmt.GetColumnName(i);
					asd::MString colValue;
					asd::equal_to_String<char, false> Equal;
					if (Equal(colName, "cInt")) {
						colValue = ToStr(cInt);
					}
					else if (Equal(colName, "cBigInt")) {
						int64_t* val = stmt.GetData(colName);
						colValue = ToStr(val);
						if (Result == 1)
							EXPECT_EQ(val, nullptr);
						else if (Result == 2) {
							EXPECT_NE(val, nullptr);
							EXPECT_EQ(*val, TestData_BigInt(cInt));
						}
					}
					else if (Equal(colName, "cVarchar")) {
						StringType* val = stmt.GetData(colName);
						colValue = ToStr(val);
						EXPECT_NE(val, nullptr);
						if (Result == 1)
							EXPECT_EQ(*val, TestData_String(cInt));
						else if (Result == 2) {
							auto cmp = TestData_String(cInt + 1);
							EXPECT_EQ(*val, cmp);
						}
					}
					else if (Equal(colName, "cDate")) {
						TimeType* val = stmt.GetData(colName);
						colValue = ToStr(val);
						if (Result == 1)
							EXPECT_EQ(val, nullptr);
						else if (Result == 2) {
							EXPECT_NE(val, nullptr);
							const asd::Time t = *val;
							EXPECT_EQ(cmp.Year(), t.Year());
							EXPECT_EQ(cmp.Month(), t.Month());
							EXPECT_EQ(cmp.Day(), t.Day());
						}
					}
					else if (Equal(colName, "cTime")) {
						TimeType* val = stmt.GetData(colName);
						colValue = ToStr(val);
						if (Result == 1)
							EXPECT_EQ(val, nullptr);
						else if (Result == 2) {
							EXPECT_NE(val, nullptr);
							const asd::Time t = *val;
							EXPECT_EQ(cmp.Hour(), t.Hour());
							EXPECT_EQ(cmp.Minute(), t.Minute());
							EXPECT_EQ(cmp.Second(), t.Second());
						}
					}
					else if (Equal(colName, "cTimestamp")) {
						TimeType* val = stmt.GetData(colName);
						colValue = ToStr(val);
						if (Result == 1) {
							EXPECT_NE(val, nullptr);
							const asd::Time t = *val;
							EXPECT_EQ(cmp, t);
						}
						else if (Result == 2)
							EXPECT_EQ(val, nullptr);
					}
					else if (Equal(colName, "cBlob")) {
						BlobType* val = stmt.GetData(colName);
						colValue = ToStr(val);
						if (Result == 1) {
							EXPECT_NE(val, nullptr);
							EXPECT_EQ(*val, TestData_Blob(cInt));
						}
						else if (Result == 2)
							EXPECT_EQ(val, nullptr);
					}
					else if (Equal(colName, "cFloat")) {
						float* val = stmt.GetData(colName);
						colValue = ToStr(val);
						EXPECT_EQ(val, nullptr);
					}
					else if (Equal(colName, "cDouble")) {
						double* val = stmt.GetData(colName);
						colValue = ToStr(val);
						if (Result == 1) {
							EXPECT_NE(val, nullptr);
							EXPECT_EQ(*val, TestData_double(cInt));
						}
						else if (Result == 2)
							EXPECT_EQ(val, nullptr);
					}
					else if (Equal(colName, "cBit")) {
						bool* val = stmt.GetData(colName);
						colValue = ToStr(val);
						EXPECT_EQ(val, nullptr);
					}
					else {
						asd_RaiseException("invalid colName : %s", colName.GetData());
					}

					COUT << asd::MString("  %10s  :  %s\n",
										 colName.GetData(),
										 colValue.GetData());
				}
				COUT << '\n';


				if (Result == 1) {
					EXPECT_EQ(Record, ++res1);
					EXPECT_LT(cInt, Criterion);
				}
				else if (Result == 2) {
					EXPECT_EQ(Record, ++res2);
					EXPECT_GE(cInt, Criterion);
				}
				else {
					FAIL();
				}
			});
			EXPECT_EQ(res1, 3);
			EXPECT_EQ(res2, 2);
			EXPECT_EQ(loop, res1 + res2);
		}
		TestEnd;
	};



	TEST(ODBC, MSSQL)
	{
		ODBCTest test(MSSQL);
	}

	TEST(ODBC, MySQL)
	{
		ODBCTest test(MySQL);
	}

	TEST(ODBC, PostgreSQL)
	{
		ODBCTest test(PostgreSQL);
	}

}

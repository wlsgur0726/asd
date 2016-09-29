#include "stdafx.h"
#include "test.h"

/*
--help
	옵션들

--gtest_list_tests
	전체 테스트 목록을 출력

--gtest_filter=케이스명.테스트명
	테스트 필터
	정규표현식 사용 가능

--gtest_shuffle
	테스트 순서 섞기

--gtest_output=xml:파일경로/파일명
	테스트 결과를 xUnit Test Report 양식의 XML파일로 출력

*/
int main(int argc, char** argv)
{
	Test();
#if 0
	std::vector<char*> args;
	args.push_back(argv[0]);

	args.push_back("--gtest_filter=ODBC.MSSQL");

	argc = args.size();
	argv = args.data();
#endif
	::testing::InitGoogleTest(&argc, argv);
	int r = RUN_ALL_TESTS();
	return r;
}

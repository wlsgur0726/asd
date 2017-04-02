#include "stdafx.h"
#include "asd/exception.h"


TEST(Exception, MacroFunctionTest1)
{
	{
		asd_PrintStdErr("Test asd_PrintStdErr\n");
		asd_PrintStdErr("Test asd_PrintStdErr {}\n", 123);
	}

	{
		const int LineCheck = __LINE__ + 1;
		asd::DebugInfo di = asd_DebugInfo("Test asd_DebugInfo");
		EXPECT_EQ(di.Line, LineCheck);
		EXPECT_STREQ(__FUNCTION__, di.Function);
		EXPECT_STREQ(__FILE__, di.File);
		EXPECT_STREQ("Test asd_DebugInfo", di.Comment);
		asd::puts(di.ToString());
	}

	{
		const int LineCheck = __LINE__ + 1;
		asd::DebugInfo di = asd_DebugInfo("Test asd_DebugInfo {}", 123);
		EXPECT_EQ(di.Line, LineCheck);
		EXPECT_STREQ(__FUNCTION__, di.Function);
		EXPECT_STREQ(__FILE__, di.File);
		EXPECT_STREQ("Test asd_DebugInfo 123", di.Comment);
		asd::puts(di.ToString());
	}
}


TEST(Exception, MacroFunctionTest2)
{
	try {
		asd_RaiseException("Test asd_RaiseException");
	}
	catch (asd::Exception& e) {
		puts(e.what());
	}

	try {
		asd_RaiseException("Test asd_RaiseException {}", 123);
	}
	catch (asd::Exception& e) {
		puts(e.what());
	}
}
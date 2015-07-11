#include "stdafx.h"
#include "../asd/include/exception.h"


TEST(Exception, MacroFunctionTest1)
{
	{
		asd_PrintStdErr("Test asd_PrintStdErr\n");
		asd_PrintStdErr("Test asd_PrintStdErr %d\n", 123);
	}

	{
		const int LineCheck = __LINE__ + 1;
		asd::DebugInfo di = asd_MakeDebugInfo("Test asd_MakeDebugInfo");
		EXPECT_EQ(di.m_line, LineCheck);
		EXPECT_STREQ(__FUNCTION__, di.m_function);
		EXPECT_STREQ(__FILE__, di.m_file);
		EXPECT_STREQ("Test asd_MakeDebugInfo", di.m_comment);

		asd::MString cmpstr(asd::DebugInfo::ToStringFormat,
							__FILE__,
							LineCheck,
							__FUNCTION__,
							"Test asd_MakeDebugInfo");
		EXPECT_STREQ(di.ToString(), cmpstr);
		asd::puts(di.ToString());
	}

	{
		const int LineCheck = __LINE__ + 1;
		asd::DebugInfo di = asd_MakeDebugInfo("Test asd_MakeDebugInfo %d", 123);
		EXPECT_EQ(di.m_line, LineCheck);
		EXPECT_STREQ(__FUNCTION__, di.m_function);
		EXPECT_STREQ(__FILE__, di.m_file);
		EXPECT_STREQ("Test asd_MakeDebugInfo 123", di.m_comment);

		asd::MString cmpstr(asd::DebugInfo::ToStringFormat,
							__FILE__,
							LineCheck,
							__FUNCTION__,
							"Test asd_MakeDebugInfo 123");
		EXPECT_STREQ(di.ToString(), cmpstr);
		asd::puts(di.ToString());
	}
}


TEST(Exception, MacroFunctionTest2)
{
	int LineCheck;

	try {
		LineCheck = __LINE__ + 1;
		asd_RaiseException("Test asd_RaiseException");
	}
	catch (asd::Exception& e) {
		asd::MString cmpstr(asd::DebugInfo::ToStringFormat,
							__FILE__,
							LineCheck,
							__FUNCTION__,
							"Test asd_RaiseException");
		EXPECT_STREQ(e.what(), cmpstr);
		puts(e.what());
	}

	try {
		LineCheck = __LINE__ + 1;
		asd_RaiseException("Test asd_RaiseException %d", 123);
	}
	catch (asd::Exception& e) {
		asd::MString cmpstr(asd::DebugInfo::ToStringFormat,
							__FILE__,
							LineCheck,
							__FUNCTION__,
							"Test asd_RaiseException 123");
		EXPECT_STREQ(e.what(), cmpstr);
		puts(e.what());
	}
}
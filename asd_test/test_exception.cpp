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
		asd::DebugInfo di = asd_MakeDebugInfo("Test asd_MakeDebugInfo");
		EXPECT_EQ(di.Line, LineCheck);
		EXPECT_STREQ(__FUNCTION__, di.Function);
		EXPECT_STREQ(__FILE__, di.File);
		EXPECT_STREQ("Test asd_MakeDebugInfo", di.Comment);

		auto cmpstr = asd::MString::Format(asd::DebugInfo::ToStringFormat,
										   asd::GetCurrentThreadID(),
										   __FILE__,
										   LineCheck,
										   __FUNCTION__,
										   "Test asd_MakeDebugInfo");
		EXPECT_STREQ(di.ToString(), cmpstr);
		asd::puts(di.ToString());
	}

	{
		const int LineCheck = __LINE__ + 1;
		asd::DebugInfo di = asd_MakeDebugInfo("Test asd_MakeDebugInfo {}", 123);
		EXPECT_EQ(di.Line, LineCheck);
		EXPECT_STREQ(__FUNCTION__, di.Function);
		EXPECT_STREQ(__FILE__, di.File);
		EXPECT_STREQ("Test asd_MakeDebugInfo 123", di.Comment);

		auto cmpstr = asd::MString::Format(asd::DebugInfo::ToStringFormat,
										   asd::GetCurrentThreadID(),
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
		auto cmpstr = asd::MString::Format(asd::DebugInfo::ToStringFormat,
										   asd::GetCurrentThreadID(),
										   __FILE__,
										   LineCheck,
										   __FUNCTION__,
										   "Test asd_RaiseException");
		EXPECT_STREQ(e.what(), cmpstr);
		puts(e.what());
	}

	try {
		LineCheck = __LINE__ + 1;
		asd_RaiseException("Test asd_RaiseException {}", 123);
	}
	catch (asd::Exception& e) {
		auto cmpstr = asd::MString::Format(asd::DebugInfo::ToStringFormat,
										   asd::GetCurrentThreadID(),
										   __FILE__,
										   LineCheck,
										   __FUNCTION__,
										   "Test asd_RaiseException 123");
		EXPECT_STREQ(e.what(), cmpstr);
		puts(e.what());
	}
}
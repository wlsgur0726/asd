#include "stdafx.h"
#include "asd/string.h"

namespace asdtest_testtemplate
{
	TEST(TestTemplateSuite, TestTemplateCase)
	{
		asd::MString str = "Test Template";
		asd::puts(str);
		ASSERT_TRUE(true);
		ASSERT_FALSE(false);
		EXPECT_TRUE(true);
		EXPECT_FALSE(false);
	}
}

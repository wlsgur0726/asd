#include "stdafx.h"
#include "../asd/include/test.h"
int main(int argc, char ** argv)
{
	asd::Test();
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
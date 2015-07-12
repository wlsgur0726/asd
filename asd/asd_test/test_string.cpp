#include "stdafx.h"
#include "asd/string.h"
#include <unordered_map>
#include <iostream>

namespace asdtest_string
{
#define TEST_STRCMP_DEFAULT(CHECK, RVAL)					\
	CHECK(0, asd::strcmp(BufM, RVAL));						\
	CHECK(0, asd::strcmp(BufW, L ## RVAL));					\

#define TEST_STRCMP_CASE(CHECK, RVAL, IGNORE_CASE)			\
	CHECK(0, asd::strcmp(BufM, RVAL, IGNORE_CASE));			\
	CHECK(0, asd::strcmp(BufW, L ## RVAL, IGNORE_CASE));	\

	TEST(String, GlobalFunction)
	{
		// strlen
		{
			char*	teststr_m =  "0123456789";
			wchar_t* teststr_w = L"0123456789";

			auto len_m = asd::strlen(teststr_m);
			auto len_w = asd::strlen(teststr_w);
			EXPECT_EQ(len_m, 10);
			EXPECT_EQ(len_m, len_w);
		}

		// sprintf, strcmp
		{
			const int BufSize = 512;
			char BufM[BufSize] = {0xFF};
			wchar_t BufW[BufSize] = {0xFF};

			asd::sprintf(BufM, BufSize, 
						 "%s %s %d", "abc", "가나다", 123);
			asd::sprintf(BufW, BufSize,
						 L"%ls %ls %d", L"abc", L"가나다", 123);

			TEST_STRCMP_DEFAULT(EXPECT_EQ, "abc 가나다 123");

			TEST_STRCMP_DEFAULT(EXPECT_NE, "ABC 가나다 123");

			TEST_STRCMP_CASE(EXPECT_EQ, "ABC 가나다 123", true);

			TEST_STRCMP_DEFAULT(EXPECT_LT, "abc 가나다 000");

			TEST_STRCMP_DEFAULT(EXPECT_LT, "abc 가나다");

			TEST_STRCMP_DEFAULT(EXPECT_GT, "abc 가나다 999");

			TEST_STRCMP_DEFAULT(EXPECT_GT, "abc 가나다 123 ");
		}
	}



	template <typename StringType>
	void TestCode_hash_and_equal(const typename StringType::CharType* a_keyString)
	{
		const int BufSize = 512;
		typename StringType::CharType key[BufSize] ={0};
		asd::strcpy(key, a_keyString);

		std::unordered_map<StringType,
						   int,
						   typename StringType::Hash> map;

		map.emplace(a_keyString, BufSize);

		{
			auto it = map.find(a_keyString);
			ASSERT_NE(it, map.end());
			EXPECT_EQ(it->second, BufSize);
		}
		{
			auto it = map.find(key);
			ASSERT_NE(it, map.end());
			EXPECT_EQ(it->second, BufSize);
		}
		{
			StringType str;
			str = a_keyString;
			auto it = map.find(str);
			ASSERT_NE(it, map.end());
			EXPECT_EQ(it->second, BufSize);
		}
	}

	TEST(String, hash_and_equal)
	{
		// MultiByte
		TestCode_hash_and_equal<asd::MString>("abc가나다123");

		// Wide
		TestCode_hash_and_equal<asd::WString>(L"abc가나다123");
	}


	const char* FormatString_String(char) {
		return "%s";
	}
	const wchar_t* FormatString_String(wchar_t) {
		return L"%ls";
	}
#define FormatString(TYPE, FORMAT)						\
	const char* FormatString_##TYPE(char) {				\
		return FORMAT;									\
	}													\
	const wchar_t* FormatString_##TYPE(wchar_t) {		\
		return L ## FORMAT;								\
	}													\

	FormatString(Int32, "%d");

	FormatString(Int64, "%lld");

	FormatString(Double, "%lf");

	FormatString(Pointer, "%p");

	FormatString(True, "true");

	FormatString(False, "false");


	template <typename StringType>
	void TestCode_StringClass(const typename StringType::CharType* a_testString)
	{
		typename StringType::CharType c = 0;

		StringType s1(a_testString);
		StringType s2(s1);
		StringType s3(FormatString_String(c), a_testString);
		StringType s4 = a_testString;
		StringType s5 = s1;
		StringType s6;
		s6 = a_testString;
		StringType s7;
		s7 = s1;


		EXPECT_EQ(s1, s2);
		EXPECT_EQ(s2, s3);
		EXPECT_EQ(s3, s4);
		EXPECT_EQ(s4, s5);
		EXPECT_EQ(s5, s6);
		EXPECT_EQ(s6, s7);

		
		s2 += s3 + s4;
		EXPECT_NE(s1, s2);
		EXPECT_LT(s1, s2);


		const int BufSize = 512;
		typename StringType::CharType buf[BufSize] = {0xFF};
		asd::strcpy(buf,					s1);
		asd::strcpy(buf+s1.GetLength(),		s1);
		asd::strcpy(buf+(s1.GetLength()*2),	s1);
		EXPECT_EQ(s1, s3);
		EXPECT_EQ(s2, buf);


		s2 << s3 << s4 << s5;
		EXPECT_EQ(s1, s3);
		EXPECT_EQ(s1, s4);


		void* p = &c;
		s2 = s1;
		s2 << 123 << true << p;
		s3 += 123;
		s3 += true;
		s3 += p;
		s4 = s4 + FormatString_Int32(c) + FormatString_String(c) + FormatString_Pointer(c);
		s5.Format(s4, 123, FormatString_True(c), p);
		EXPECT_EQ(s2, s3);
		EXPECT_EQ(s3, s5);


		s7 = StringType();
		EXPECT_NE(s1, s7);
		EXPECT_GT(s1, s7);
	}

	TEST(String, StringClass)
	{
		// MultiByte
		TestCode_StringClass<asd::MString>("abc가나다123");

		// Wide
		TestCode_StringClass<asd::WString>(L"abc가나다123");
	}
}

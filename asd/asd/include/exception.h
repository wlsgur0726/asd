#pragma once
#include "../../asd/include/asdbase.h"
#include "../../asd/include/string.h"
#include <vector>

namespace asd
{
	struct DebugInfo 
	{
		static const char ToStringFormat[]; // = "[%s(%d) %s] %s ";

		const char*	m_file;
		const int	m_line;
		const char* m_function;
		MString m_comment;

		DebugInfo(IN const char* a_file,
				  IN const int a_line,
				  IN const char* a_function,
				  IN const char* a_comment = "",
				  IN ...) asd_NoThrow;

		MString ToString() const asd_NoThrow;
	};
	typedef std::vector<DebugInfo> DebugTrace;



	class Exception : public std::exception
	{
	public:
		MString m_what;

		Exception() asd_NoThrow;
		Exception(IN const char* a_what) asd_NoThrow;
		Exception(IN const MString& a_what) asd_NoThrow;
		Exception(IN const DebugInfo& a_dbginfo) asd_NoThrow;
		virtual ~Exception() asd_NoThrow;

		virtual const char* what() const asd_NoThrow override;
	};



#if defined(asd_Compiler_MSVC)
	#define asd_MakeDebugInfo(COMMENT, ...)										\
		asd::DebugInfo(__FILE__, __LINE__, __FUNCTION__, COMMENT, __VA_ARGS__)	\

#else
	#define asd_MakeDebugInfo(...)												\
		asd::DebugInfo(__FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)			\

#endif



#if defined(asd_Compiler_MSVC)
	#define asd_DebugTrace(TRACE, COMMENT, ...)									\
		TRACE.push_back( asd_MakeDebugInfo(COMMENT, __VA_ARGS__) )				\

#else
	#define asd_DebugTrace(TRACE, ...)											\
		TRACE.push_back( asd_MakeDebugInfo(__VA_ARGS__) )						\

#endif



#if defined(asd_Compiler_MSVC)
	#define asd_PrintStdErr(msg, ...)											\
		asd::fputs(asd_MakeDebugInfo(msg, __VA_ARGS__).ToString(), stderr);		\

#else
	#define asd_PrintStdErr(...)												\
		asd::fputs(asd_MakeDebugInfo(__VA_ARGS__).ToString(), stderr);			\

#endif



#ifdef asd_Can_not_use_Exception
	#error asd_RaiseException 미구현
	#define asd_RaiseException(msg, ...) \

#else

	#if defined(asd_Compiler_MSVC)
		#define asd_RaiseException(msg, ...)										\
			throw asd::Exception(asd_MakeDebugInfo(msg, __VA_ARGS__));				\

	#else
		#define asd_RaiseException(...)												\
			throw asd::Exception(asd_MakeDebugInfo(__VA_ARGS__));					\

	#endif

#endif
}

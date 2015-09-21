#pragma once
#include "asd/asdbase.h"
#include "asd/string.h"
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
				  IN ...) noexcept;

		MString ToString() const noexcept;
	};
	typedef std::vector<DebugInfo> DebugTrace;



	class Exception : public std::exception
	{
	public:
		MString m_what;

		Exception() noexcept;
		Exception(IN const char* a_what) noexcept;
		Exception(IN const MString& a_what) noexcept;
		Exception(IN const DebugInfo& a_dbginfo) noexcept;
		virtual ~Exception() noexcept;

		virtual const char* what() const noexcept override;
	};



	// asd_MakeDebugInfo
#if defined(asd_Compiler_MSVC)
	#define asd_MakeDebugInfo(COMMENT, ...)											\
		asd::DebugInfo(__FILE__, __LINE__, __FUNCTION__, COMMENT, __VA_ARGS__)		\

#else
	#define asd_MakeDebugInfo(...)													\
		asd::DebugInfo(__FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)				\

#endif



	// asd_DebugTrace
#if defined(asd_Compiler_MSVC)
	#define asd_DebugTrace(TRACE, COMMENT, ...)										\
		TRACE.push_back( asd_MakeDebugInfo(COMMENT, __VA_ARGS__) )					\

#else
	#define asd_DebugTrace(TRACE, ...)												\
		TRACE.push_back( asd_MakeDebugInfo(__VA_ARGS__) )							\

#endif



	// asd_PrintStdErr
#if defined(asd_Compiler_MSVC)
	#define asd_PrintStdErr(MsgFormat, ...)											\
		asd::fputs(asd_MakeDebugInfo(MsgFormat, __VA_ARGS__).ToString(), stderr);	\

#else
	#define asd_PrintStdErr(...)													\
		asd::fputs(asd_MakeDebugInfo(__VA_ARGS__).ToString(), stderr);				\

#endif



	// asd_Destructor
#define asd_Destructor_Start try {
#define asd_Destructor_End }														\
	catch (asd::Exception& e) {														\
		asd_PrintStdErr("asd::Exception! %s\n", e.what());							\
		assert(false);																\
	}																				\
	catch (std::exception& e) {														\
		asd_PrintStdErr("std::exception! %s\n", e.what());							\
		assert(false);																\
	}																				\
	catch (const char* e) {															\
		asd_PrintStdErr("const char* exception! %s\n", e);							\
		assert(false);																\
	}																				\
	catch (...) {																	\
		asd_PrintStdErr("unknown exception!\n");									\
		assert(false);																\
	}																				\



	// asd_RaiseException
#ifdef asd_Can_not_use_Exception
	#error asd_RaiseException 미구현
	#define asd_RaiseException(MsgFormat, ...) \

#else

	#if defined(asd_Compiler_MSVC)
		#define asd_RaiseException(MsgFormat, ...)									\
			throw asd::Exception(asd_MakeDebugInfo(MsgFormat, __VA_ARGS__));		\

	#else
		#define asd_RaiseException(...)												\
			throw asd::Exception(asd_MakeDebugInfo(__VA_ARGS__));					\

	#endif

#endif
}

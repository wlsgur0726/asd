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
				  IN ...) asd_noexcept;

		MString ToString() const asd_noexcept;
	};
	typedef std::vector<DebugInfo> DebugTrace;



	class Exception : public std::exception
	{
	public:
		MString m_what;

		Exception() asd_noexcept;
		Exception(IN const char* a_what) asd_noexcept;
		Exception(IN const MString& a_what) asd_noexcept;
		Exception(IN const DebugInfo& a_dbginfo) asd_noexcept;
		virtual ~Exception() asd_noexcept;

		virtual const char* what() const asd_noexcept override;
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
		OnException(e, asd_MakeDebugInfo("asd::Exception! %s\n", e.what()));		\
	}																				\
	catch (std::bad_alloc& e) {														\
		OnException(e, asd_MakeDebugInfo("std::bad_alloc! %s\n", e.what()));		\
	}																				\
	catch (std::exception& e) {														\
		OnException(e, asd_MakeDebugInfo("std::exception! %s\n", e.what()));		\
	}																				\
	catch (const char* e) {															\
		OnException(e, asd_MakeDebugInfo("const char* exception! %s\n", e));		\
	}																				\
	catch (...) {																	\
		OnUnknownException(asd_MakeDebugInfo("unknown exception!"));				\
	}																				\

	// VS 매크로 디버깅이 불편해서 함수로 뺌
	template <typename ExceptionType>
	inline void OnException(const ExceptionType& e,
							const DebugInfo& info) asd_noexcept
	{
		asd::fputs(info.ToString(), stderr);
		assert(false);
	}

	inline void OnUnknownException(const DebugInfo& info) asd_noexcept
	{
		asd::fputs(info.ToString(), stderr);
		assert(false);
	}



	// asd_RaiseException
#if asd_Can_not_use_Exception
	#define asd_RaiseException(MsgFormat, ...) {	\
		asd_PrintStdErr(MsgFormat, __VA_ARGS__);	\
		std::terminate();							\
	}												\

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

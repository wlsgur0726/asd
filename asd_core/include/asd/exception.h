#pragma once
#include "asdbase.h"
#include "string.h"
#include "trace.h"
#include <vector>
#include <functional>


namespace asd
{
	void DebugBreak();


	struct DebugInfo 
	{
		static const char ToStringFormat[]; // "[{}({}):{}] {}", m_file, m_line, m_function, m_comment)

		const char*	m_file;
		const int	m_line;
		const char* m_function;
		MString m_comment;

		template<typename... ARGS>
		inline DebugInfo(IN const char* a_file,
						 IN const int a_line,
						 IN const char* a_function,
						 IN const char* a_comment = "",
						 IN const ARGS&... a_args) asd_noexcept
			: m_file(a_file)
			, m_line(a_line)
			, m_function(a_function)
			, m_comment(MString::Format(a_comment, a_args...))
		{
			assert(a_file != nullptr);
			assert(m_line > 0);
			assert(a_function != nullptr);
		}

		MString ToString() const asd_noexcept;
	};
	typedef std::vector<DebugInfo> DebugTrace;



	class Exception : public std::exception
	{
	public:
		Exception() asd_noexcept;
		Exception(IN const char* a_what) asd_noexcept;
		Exception(IN const MString& a_what) asd_noexcept;
		Exception(IN const DebugInfo& a_dbginfo) asd_noexcept;
		virtual ~Exception() asd_noexcept;

		virtual const char* what() const asd_noexcept override;
		const StackTrace& GetStackTrace() const asd_noexcept;

	protected:
		MString m_what;
		const StackTrace m_stackTrace = StackTrace(1);
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
		TRACE.emplace_back(asd_MakeDebugInfo(COMMENT, __VA_ARGS__))					\

#else
	#define asd_DebugTrace(TRACE, ...)												\
		TRACE.emplace_back(asd_MakeDebugInfo(__VA_ARGS__))							\

#endif



	// asd_PrintStdErr
#if defined(asd_Compiler_MSVC)
	#define asd_PrintStdErr(MsgFormat, ...)											\
		asd::fputs(asd_MakeDebugInfo(MsgFormat, __VA_ARGS__).ToString(), stderr)	\

#else
	#define asd_PrintStdErr(...)													\
		asd::fputs(asd_MakeDebugInfo(__VA_ARGS__).ToString(), stderr)				\

#endif



	// asd_Destructor
#define asd_Destructor_Start try {
#define asd_Destructor_End }														\
	catch (asd::Exception& e) {														\
		OnException(e, asd_MakeDebugInfo("asd::Exception! {}\n", e.what()));		\
	}																				\
	catch (std::bad_alloc& e) {														\
		OnException(e, asd_MakeDebugInfo("std::bad_alloc! {}\n", e.what()));		\
	}																				\
	catch (std::exception& e) {														\
		OnException(e, asd_MakeDebugInfo("std::exception! {}\n", e.what()));		\
	}																				\
	catch (const char* e) {															\
		OnException(e, asd_MakeDebugInfo("const char* exception! {}\n", e));		\
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
	#define asd_RaiseException(MsgFormat, ...)		\
	do{												\
		asd_PrintStdErr(MsgFormat, __VA_ARGS__);	\
		std::terminate();							\
	}while(false)									\

#else
	#if defined(asd_Compiler_MSVC)
		#define asd_RaiseException(MsgFormat, ...)									\
			throw asd::Exception(asd_MakeDebugInfo(MsgFormat, __VA_ARGS__))			\

	#else
		#define asd_RaiseException(...)												\
			throw asd::Exception(asd_MakeDebugInfo(__VA_ARGS__))					\

	#endif

#endif


	// 표준 assert와는 다르게 release에서도 Check는 수행된다.
	// Check가 false인 경우 기본동작
	//  - 에러메시지를 asd::Logger::GlobalInstance().ErrorLog로 출력
	//  - debug일 경우 키입력 대기 후 std::terminate 호출
	//  - release일때는 계속 진행
#if defined(asd_Compiler_MSVC)
#	define asd_Assert(Check, MsgFormat, ...)\
		( (Check) ? (true) : (asd::Assert_Internal(asd_MakeDebugInfo(MsgFormat, __VA_ARGS__))) )
#else
#	define asd_Assert(Check, ...)\
		( (Check) ? (true) : (asd::Assert_Internal(asd_MakeDebugInfo(__VA_ARGS__))) )
#endif
	bool Assert_Internal(IN const DebugInfo& a_info);

	// asd_Assert에서 Check가 false일 때 기본동작 대신 핸들러를 호출한다.
	// nullptr을 입력하면 기본동작을 수행한다.
	void SetAssertHandler(IN const std::function<void(IN const DebugInfo&)>& a_handler) asd_noexcept;
}

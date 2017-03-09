#pragma once
#include "asdbase.h"
#include "string.h"
#include "trace.h"
#include <vector>
#include <functional>
#include <typeinfo>


namespace asd
{
	void DebugBreak();


	struct DebugInfo 
	{
		static const char ToStringFormat[]; // "[{}({}):{}] {}", File, Line, Function, Comment)

		const char*	File;
		const int	Line;
		const char* Function;
		MString		Comment;

		template<typename... ARGS>
		inline DebugInfo(IN const char* a_file,
						 IN const int a_line,
						 IN const char* a_function,
						 IN const char* a_comment = "",
						 IN const ARGS&... a_args) asd_noexcept
			: File(a_file)
			, Line(a_line)
			, Function(a_function)
			, Comment(MString::Format(a_comment, a_args...))
		{
			assert(File != nullptr);
			assert(Line > 0);
			assert(Function != nullptr);
		}

		MString ToString() const asd_noexcept;
	};
	typedef std::vector<DebugInfo> DebugTrace;



	// __VA_ARGS__ : format, ...
#define asd_MakeDebugInfo(...)\
	asd::DebugInfo(__FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

#define asd_DebugTrace(TRACE, ...)\
	TRACE.emplace_back(asd_MakeDebugInfo(__VA_ARGS__))

#define asd_PrintStdErr(...)\
	asd::fputs(asd_MakeDebugInfo(__VA_ARGS__).ToString(), stderr)



	// exception
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

	struct UnknownException
	{
		inline const char* what() const asd_noexcept
		{
			return "?";
		}
	};

	// try - catch
#define asd_Try try
#define asd_Catch(ExceptionHandler)													\
	catch (asd::Exception& e) {														\
		asd::OnException(e, ExceptionHandler, __FILE__, __LINE__, __FUNCTION__);	\
	}																				\
	catch (std::bad_alloc& e) {														\
		asd::OnException(e, ExceptionHandler, __FILE__, __LINE__, __FUNCTION__);	\
	}																				\
	catch (std::exception& e) {														\
		asd::OnException(e, ExceptionHandler, __FILE__, __LINE__, __FUNCTION__);	\
	}																				\
	catch (const char* e) {															\
		asd::OnException(e, ExceptionHandler, __FILE__, __LINE__, __FUNCTION__);	\
	}																				\

#define asd_CatchUnknown(ExceptionHandler)											\
	asd_Catch(ExceptionHandler)														\
	catch (...) {																	\
		asd::UnknownException e;													\
		asd::OnException(e, ExceptionHandler, __FILE__, __LINE__, __FUNCTION__);	\
	}																				\

	// use default handler
#define asd_Catch_Default()						asd_Catch(asd::DefaultExceptionHandler())
#define asd_CatchUnknown_Default()				asd_CatchUnknown(asd::DefaultExceptionHandler())

	// function style
#define asd_BeginTry()							asd_Try {
#define asd_EndTry(ExceptionHandler)			} asd_Catch(ExceptionHandler)
#define asd_EndTry_Default()					} asd_Catch_Default()
#define asd_EndTryUnknown(ExceptionHandler)		} asd_CatchUnknown(ExceptionHandler)
#define asd_EndTryUnknown_Default()				} asd_CatchUnknown_Default()

	// for destructor
#define asd_BeginDestructor()					asd_BeginTry()
#define asd_EndDestructor()						asd_EndTryUnknown_Default()

	// asd_RaiseException
	// __VA_ARGS__ : format, ...
#if asd_Can_not_use_Exception
	#define asd_RaiseException(...)				\
		do {									\
			asd_RAssert(false, __VA_ARGS__);	\
			std::terminate();					\
		} while(false)							\

#else
	#define asd_RaiseException(...)\
		throw asd::Exception(asd_MakeDebugInfo(__VA_ARGS__))

#endif

	// for asd_try - asd_catch
	struct ExceptionInterface
	{
		virtual ~ExceptionInterface() asd_noexcept {}
		virtual const char* what() const asd_noexcept = 0;
		virtual const std::type_info& GetType() const asd_noexcept = 0;
		virtual const StackTrace& GetStackTrace() const asd_noexcept
		{
			static StackTrace s_null(0, 0);
			return s_null;
		}
	};

	template<typename ExceptionType>
	struct ExceptionPtrTemplate : public ExceptionInterface
	{
		const ExceptionType* m_eip;

		ExceptionPtrTemplate(IN const ExceptionType* a_eip) asd_noexcept
			: m_eip(a_eip)
		{
		}

		virtual const char* what() const asd_noexcept override
		{
			return m_eip->what();
		}

		virtual const std::type_info& GetType() const asd_noexcept
		{
			return typeid(*m_eip);
		}
	};

	template<>
	struct ExceptionPtrTemplate<Exception> : public ExceptionInterface
	{
		const Exception* m_eip;

		ExceptionPtrTemplate(IN const Exception* a_eip) asd_noexcept
			: m_eip(a_eip)
		{
		}

		virtual const char* what() const asd_noexcept override
		{
			return m_eip->what();
		}

		virtual const std::type_info& GetType() const asd_noexcept
		{
			return typeid(*m_eip);
		}

		virtual const StackTrace& GetStackTrace() const asd_noexcept override
		{
			return m_eip->GetStackTrace();
		}
	};

	template<>
	struct ExceptionPtrTemplate<const char*> : public ExceptionInterface
	{
		const char* m_eip;

		ExceptionPtrTemplate(IN const char*const* a_eip) asd_noexcept
			: m_eip(*a_eip)
		{
			if (m_eip == nullptr)
				m_eip = "";
		}

		virtual const char* what() const asd_noexcept override
		{
			return m_eip;
		}

		virtual const std::type_info& GetType() const asd_noexcept
		{
			return typeid(const char*);
		}
	};

	template<typename ExceptionType, typename ExceptionHandler>
	void OnException(IN const ExceptionType& a_exception,
					 IN const ExceptionHandler& a_handler,
					 IN const char* a_file,
					 IN const int a_line,
					 IN const char* a_function)
	{
		ExceptionPtrTemplate<ExceptionType> ei(&a_exception);
		ExceptionInterface* eip = &ei;
		a_handler(eip,
				  DebugInfo(a_file, a_line, a_function));
	}

	// for asd_Catch_Default, asd_CatchUnknown_Default
	struct DefaultExceptionHandler
	{
		void operator () (IN ExceptionInterface* a_ei,
						  IN const DebugInfo& a_catchPosInfo) const asd_noexcept;
	};



	// assert
	struct AssertHandler
	{
		// 기본동작
		// - ErrorMessage를 asd::Logger::GlobalInstance().ErrorLog()로 출력
		// - release일 경우 StackTrace도 출력
		// - debug일 경우 DebugBreak
		virtual void OnError(IN const DebugInfo& a_info) asd_noexcept;
		virtual ~AssertHandler() {}
	};


	// 현재 셋팅되어있는 AssertHandler 리턴
	std::shared_ptr<AssertHandler> GetAssertHandler() asd_noexcept;


	// 커스텀 AssertHandler 셋팅
	// nullptr 입력할 경우 기본 AssertHandler로 셋팅
	void SetAssertHandler(IN std::shared_ptr<AssertHandler> a_handler) asd_noexcept;


	// asd_RAssert : 표준 assert와는 다르게 release에서도 Check는 수행된다.
	// __VA_ARGS__ : format, ...
#define asd_RAssert(Check, ...)														\
	do {																			\
		if ((Check) == false)														\
			asd::GetAssertHandler()->OnError(asd_MakeDebugInfo(__VA_ARGS__));		\
	} while(false)																	\


	// asd_DAssert : release에서는 Check를 수행하지 않는다.
#if defined(asd_Debug)
#	define asd_DAssert(Check) asd_RAssert(Check, #Check)

#else
#	define asd_DAssert(Check)

#endif


	// check error and return
	// __VA_ARGS__ : format, ...
#define asd_ChkErrAndRetVal(IsErr, RetVal, ...)										\
	do {																			\
		if (IsErr) {																\
			asd::GetAssertHandler()->OnError(asd_MakeDebugInfo(__VA_ARGS__));		\
			return RetVal;															\
		}																			\
	} while(false)																	\

#define asd_ChkErrAndRet(IsErr, ...)\
	asd_ChkErrAndRetVal(IsErr, ;, __VA_ARGS__)



}

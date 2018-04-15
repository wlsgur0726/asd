#pragma once
#include "asdbase.h"
#include "trace.h"
#include "threadutil.h"
#include <vector>
#include <functional>
#include <typeinfo>


namespace asd
{
	void DebugBreak();


	// exception
	class Exception : public std::exception
	{
	public:
		Exception();
		Exception(IN const char* a_what);
		Exception(IN const MString& a_what);
		Exception(IN const DebugInfo& a_dbginfo);
		virtual ~Exception();

		virtual const char* what() const asd_noexcept override;
		const StackTrace& GetStackTrace() const;

	protected:
		MString m_what;
		const StackTrace m_stackTrace = StackTrace(1);

#if asd_Exception_Enable_DumpCreator
	private:
		struct DumpCreator
		{
			DumpCreator();
		} m_dumpCreator;
#endif
	};

	struct UnknownException
	{
		inline const char* what() const
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
		throw asd::Exception(asd_DebugInfo(__VA_ARGS__))

#endif

	// for asd_try - asd_catch
	struct ExceptionPtrInterface
	{
		virtual ~ExceptionPtrInterface() {}
		virtual const char* what() const = 0;
		virtual const std::type_info& GetType() const = 0;
		virtual const StackTrace& GetStackTrace() const
		{
			static StackTrace s_null(0, 0);
			return s_null;
		}
	};

	template<typename ExceptionType>
	struct ExceptionPtrTemplate : public ExceptionPtrInterface
	{
		const ExceptionType* m_ptr;

		ExceptionPtrTemplate(IN const ExceptionType* a_ptr)
			: m_ptr(a_ptr)
		{
		}

		virtual const char* what() const override
		{
			return m_ptr->what();
		}

		virtual const std::type_info& GetType() const
		{
			return typeid(*m_ptr);
		}
	};

	template<>
	struct ExceptionPtrTemplate<Exception> : public ExceptionPtrInterface
	{
		const Exception* m_ptr;

		ExceptionPtrTemplate(IN const Exception* a_ptr)
			: m_ptr(a_ptr)
		{
		}

		virtual const char* what() const override
		{
			return m_ptr->what();
		}

		virtual const std::type_info& GetType() const
		{
			return typeid(*m_ptr);
		}

		virtual const StackTrace& GetStackTrace() const override
		{
			return m_ptr->GetStackTrace();
		}
	};

	template<>
	struct ExceptionPtrTemplate<const char*> : public ExceptionPtrInterface
	{
		const char* m_ptr;

		ExceptionPtrTemplate(IN const char*const* a_ptr)
			: m_ptr(*a_ptr)
		{
			if (m_ptr == nullptr)
				m_ptr = "";
		}

		virtual const char* what() const override
		{
			return m_ptr;
		}

		virtual const std::type_info& GetType() const
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
		ExceptionPtrTemplate<ExceptionType> exception(&a_exception);
		ExceptionPtrInterface* ptr = &exception;
		a_handler(ptr,
				  DebugInfo(a_file, a_line, a_function));
	}

	// for asd_Catch_Default, asd_CatchUnknown_Default
	struct DefaultExceptionHandler
	{
		void operator () (IN ExceptionPtrInterface* a_exception,
						  IN const DebugInfo& a_catchPosInfo) const;
	};



	// assert
	struct AssertHandler
	{
		// 기본동작
		// - ErrorMessage를 asd::Logger::GlobalInstance().ErrorLog()로 출력
		// - release일 경우 StackTrace도 출력
		// - debug일 경우 DebugBreak
		virtual void OnError(IN const DebugInfo& a_info);
		virtual ~AssertHandler() {}
	};


	// 현재 셋팅되어있는 AssertHandler 리턴
	std::shared_ptr<AssertHandler> GetAssertHandler();


	// 커스텀 AssertHandler 셋팅
	// nullptr 입력할 경우 기본 AssertHandler로 셋팅
	void SetAssertHandler(IN std::shared_ptr<AssertHandler> a_handler);


	// asd_RAssert : 표준 assert와는 다르게 release에서도 Check는 수행된다.
	// __VA_ARGS__ : format, ...
#define asd_RAssert(Check, ...)													\
	do {																		\
		if (!(Check))															\
			asd::GetAssertHandler()->OnError(asd_DebugInfo(__VA_ARGS__));		\
	} while(false)																\


	// asd_DAssert : release에서는 Check를 수행하지 않는다.
#if defined(asd_Debug)
#	define asd_DAssert(Check) asd_RAssert(Check, #Check)
#
#else
#	define asd_DAssert(Check)
#
#endif


	// check error and return
	// __VA_ARGS__ : format, ...
#define asd_ChkErrAndRetVal(IsErr, RetVal, ...)									\
	do {																		\
		if (IsErr) {															\
			asd::GetAssertHandler()->OnError(asd_DebugInfo(__VA_ARGS__));		\
			return RetVal;														\
		}																		\
	} while(false)																\

#define asd_ChkErrAndRet(IsErr, ...)\
	asd_ChkErrAndRetVal(IsErr, ;, __VA_ARGS__)


}

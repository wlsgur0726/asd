#include "asd_pch.h"
#include "asd/exception.h"
#include "asd/log.h"
#include <atomic>


namespace asd
{
#if asd_Debug
	#if asd_Compiler_MSVC
		void DebugBreak()
		{
			::DebugBreak();
		}

	#else
		void DebugBreak()
		{
			assert(false);
		}

	#endif

#else
	void DebugBreak() {}

#endif


	const char DebugInfo::ToStringFormat[] = "[{}({}):{}] {} ";


	MString DebugInfo::ToString() const asd_noexcept
	{
		return MString::Format(ToStringFormat,
							   File, 
							   Line, 
							   Function, 
							   Comment);
	}

	Exception::Exception()  asd_noexcept
	{
		m_what = "unknown asd::Exception ";
	}

	Exception::Exception(const char* a_what) asd_noexcept
	{
		m_what = a_what;
	}

	Exception::Exception(const MString& a_what) asd_noexcept
	{
		m_what = a_what;
	}

	Exception::Exception(const DebugInfo& a_dbginfo) asd_noexcept
	{
		m_what = a_dbginfo.ToString();
	}
	
	Exception::~Exception() asd_noexcept
	{
	}

	const char* Exception::what() const asd_noexcept
	{
		return m_what;
	}

	const StackTrace& Exception::GetStackTrace() const asd_noexcept
	{
		return m_stackTrace;
	}



	void DefaultExceptionHandler::operator()(IN ExceptionInterface* a_ei,
											 IN const DebugInfo& a_catchPosInfo) const asd_noexcept
	{
		static const char* MsgFormat =
			"on exception({}), {}\n"
			"    birthplace : {}\n"
			"    caught : {}";

		StackTrace::ToStrOpt opt;
		opt.IgnoreFirstIndent = true;
		opt.Indent = 8;
		auto msg = MString::Format(MsgFormat,
								   a_ei->GetType().name(),
								   a_ei->what(),
								   a_ei->GetStackTrace().ToString(opt),
								   StackTrace(2).ToString(opt));

		Logger::GlobalInstance()._ErrorLog(a_catchPosInfo.File,
										   a_catchPosInfo.Line,
										   msg);
		asd::DebugBreak();
	}



	std::shared_ptr<AssertHandler> g_defaultAssertHandler = std::make_shared<AssertHandler>();
	std::shared_ptr<AssertHandler> g_currentAssertHandler = nullptr;


	void AssertHandler::OnError(IN const DebugInfo& a_info) asd_noexcept
	{
		MString msg;
		msg << "assert fail, " << a_info.Comment;
#if !asd_Debug
		StackTrace::ToStrOpt opt;
		opt.Indent = 8;
		msg << "\n    stack trace\n" << StackTrace(1).ToString(opt);
#endif
		Logger::GlobalInstance()._ErrorLog(a_info.File,
										   a_info.Line,
										   msg);
		asd::DebugBreak();
	}


	std::shared_ptr<AssertHandler> GetAssertHandler() asd_noexcept
	{
		auto ret = std::atomic_load(&g_currentAssertHandler);
		if (ret != nullptr)
			return ret;
		return g_defaultAssertHandler;
	}

	void SetAssertHandler(IN std::shared_ptr<AssertHandler> a_handler) asd_noexcept
	{
		std::atomic_exchange(&g_currentAssertHandler, a_handler);
	}



	void OnDestructorException(IN const DebugInfo& a_di,
							   IN ExceptionInterface* a_ei) asd_noexcept
	{
		MString msg = a_di.Comment;
#if !asd_Debug
		MString st;
		if (a_ei->GetStackTrace().size() > 0)
			st = a_ei->GetStackTrace().ToString(8);
		if (st.empty())
			st = StackTrace(1).ToString(8);
		msg << "\n    StackTrace\n" << st;
#endif
		Logger::GlobalInstance()._ErrorLog(a_di.File,
										   a_di.Line,
										   msg);
		asd::DebugBreak();
	}

}
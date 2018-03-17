#include "asd_pch.h"
#include "asd/exception.h"
#include "asd/log.h"
#include <atomic>

#if asd_Exception_Enable_DumpCreator
#	include "asd/memdump.h"
#
#endif


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

	Exception::Exception() 
	{
		m_what = "unknown asd::Exception ";
	}

	Exception::Exception(const char* a_what)
	{
		m_what = a_what;
	}

	Exception::Exception(const MString& a_what)
	{
		m_what = a_what;
	}

	Exception::Exception(const DebugInfo& a_dbginfo)
	{
		m_what = a_dbginfo.ToString();
	}
	
	Exception::~Exception()
	{
	}

	const char* Exception::what() const asd_noexcept
	{
		return m_what;
	}

	const StackTrace& Exception::GetStackTrace() const
	{
		return m_stackTrace;
	}

#if asd_Exception_Enable_DumpCreator
	Exception::DumpCreator::DumpCreator()
	{
		MemDump::Create();
	}

#endif

	void DefaultExceptionHandler::operator()(IN ExceptionPtrInterface* a_exception,
											 IN const DebugInfo& a_catchPosInfo) const
	{
		static const char* MsgFormat =
			"on exception({}), {}\n"
			"    birthplace : {}\n"
			"    caught : {}";

		StackTrace::ToStrOpt opt;
		opt.IgnoreFirstIndent = true;
		opt.Indent = 8;
		auto msg = MString::Format(MsgFormat,
								   a_exception->GetType().name(),
								   a_exception->what(),
								   a_exception->GetStackTrace().ToString(opt),
								   StackTrace(2).ToString(opt));

		Logger::GlobalInstance()._ErrorLog(a_catchPosInfo.File,
										   a_catchPosInfo.Line,
										   msg);
		asd::DebugBreak();
	}



	std::shared_ptr<AssertHandler> g_defaultAssertHandler = std::make_shared<AssertHandler>();
	std::shared_ptr<AssertHandler> g_currentAssertHandler = nullptr;


	void AssertHandler::OnError(IN const DebugInfo& a_info)
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


	std::shared_ptr<AssertHandler> GetAssertHandler()
	{
		auto ret = std::atomic_load(&g_currentAssertHandler);
		if (ret != nullptr)
			return ret;
		return g_defaultAssertHandler;
	}

	void SetAssertHandler(IN std::shared_ptr<AssertHandler> a_handler)
	{
		std::atomic_exchange(&g_currentAssertHandler, a_handler);
	}

}
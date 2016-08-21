#include "asd_pch.h"
#include "asd/exception.h"
#include "asd/util.h"

#if __cplusplus >= 201700
#	include <shared_mutex>
#
#else
#	include <mutex>
#
#endif


namespace asd
{
	const char DebugInfo::ToStringFormat[] = "[{}({}) {}] {} ";


	MString DebugInfo::ToString() const asd_noexcept
	{
		MString s;
		s.Format(ToStringFormat,
				 m_file, 
				 m_line, 
				 m_function, 
				 m_comment);
		return s;
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



#if __cplusplus >= 201700
	typedef std::shared_mutex						AssertHandler_Lock;
	typedef std::shared_lock<AssertHandler_Lock>	AssertHandler_SLock;
	typedef std::unique_lock<AssertHandler_Lock>	AssertHandler_ULock;

#else
	typedef std::mutex	AssertHandler_Lock;
	typedef std::unique_lock<AssertHandler_Lock>	AssertHandler_SLock;
	typedef std::unique_lock<AssertHandler_Lock>	AssertHandler_ULock;

#endif

	struct AssertHandler : public Global<AssertHandler>
	{
		AssertHandler_Lock m_lock;
		std::function<void(IN const DebugInfo&)> m_handler = nullptr;
		const std::function<void(IN const DebugInfo&)> m_defaultHandler = [](IN const DebugInfo& a_dbginfo)
		{
			auto msg = a_dbginfo.ToString();
			asd::fputs(msg.c_str(), stderr);
#if asd_Debug
			Pause();
			assert(false);
#endif
		};
	};


	bool Assert_Internal(IN const DebugInfo& a_info)
	{
		auto& ah = AssertHandler::GlobalInstance();
		AssertHandler_SLock lock(ah.m_lock);
		if (ah.m_handler != nullptr)
			ah.m_handler(a_info);
		else
			ah.m_defaultHandler(a_info);
		return false;
	}


	void SetAssertHandler(IN const std::function<void(const DebugInfo&)>& a_handler) asd_noexcept
	{
		auto& ah = AssertHandler::GlobalInstance();
		AssertHandler_ULock lock(ah.m_lock);
		ah.m_handler = a_handler;
	}
}
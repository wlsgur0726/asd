#include "asd_pch.h"
#include "asd/log.h"
#include "asd/iconvwrap.h"
#include "asd/datetime.h"


namespace asd
{
	Logger::Logger(IN const MString& a_outDir,
				   IN const MString& a_logName)
		: m_outDir(ConvToW(a_outDir))
		, m_logName(ConvToW(a_logName))
	{
		Init();
	}


	Logger::Logger(IN const WString& a_outDir,
				   IN const WString& a_logName)
		: m_outDir(a_outDir)
		, m_logName(a_logName)
	{
		Init();
	}


	void Logger::Init()
	{
		DateTime::Now().ToString("%Y-%m-%d_%H%M%S");
#if asd_Platform_Windows
	/*	auto e = _wfopen_s(&m_logFile, m_logFilePath, L"a+,ccs=UTF-8");
		if (e != 0) {

		}*/
#else

#endif

		m_writer = std::thread([this]()
		{

		});
	}


	void Logger::SetGenHeadDelegate(MOVE std::function<MString()>&& a_delegate) asd_noexcept
	{
		MtxCtl_asdMutex lock(m_lock);
		m_genHead = std::move(a_delegate);
	}

	void Logger::SetGenTailDelegate(MOVE std::function<MString()>&& a_delegate) asd_noexcept
	{
		MtxCtl_asdMutex lock(m_lock);
		m_genTail = std::move(a_delegate);
	}

	void Logger::PushLog(IN const Log& a_log) asd_noexcept
	{
		MtxCtl_asdMutex lock(m_lock);
		Log* log = m_logObjPool.Alloc();
		*log = a_log;
		m_queue.push(log);
	}
}

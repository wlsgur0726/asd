#include "asd_pch.h"
#include "asd/log.h"
#include "asd/iconvwrap.h"
#include "asd/datetime.h"
#include <fcntl.h>

#if defined(asd_Platform_Windows)
#	include <io.h>
#
#endif

namespace asd
{
#if defined(asd_Platform_Windows)
	inline FILE* fopen(IN const FChar* a_path,
					   IN const FChar* a_mode)
	{
		FILE* fp;
		if (0 != _wfopen_s(&fp, a_path, a_mode))
			fp = nullptr;
		return fp;
	}

	inline FString ConvToF(IN const char* a_str)
	{
		return ConvToW(a_str);
	}

	inline FString ConvToF(IN const wchar_t* a_str)
	{
		return a_str;
	}

	inline bool Flush(IN FILE* a_fp)
	{
		if (fflush(a_fp) != 0)
			return false;
		int fd = _fileno(a_fp);
		if (fd == -1)
			return false;
		HANDLE handle = (HANDLE)_get_osfhandle(fd);
		if (handle == INVALID_HANDLE_VALUE)
			return false;
		return FlushFileBuffers(handle) != FALSE;
	}

#else
	inline FString ConvToF(IN const char* a_str)
	{
		return a_str;
	}

	inline FString ConvToF(IN const wchar_t* a_str)
	{
		return ConvToM(a_str);
	}

	inline bool Flush(IN FILE* a_fp)
	{
		if (fflush(a_fp) != 0)
			return false;
		int fd = fileno(a_fp);
		if (fd == -1)
			return false;
		return fsync(fd) == 0;
	}

#endif

	inline FString AddDelimiter(IN FString&& a_path)
	{
		FString path = std::move(a_path);
		if (path.empty())
			return path;

		FChar& c = path[path.size() - 1];
		if (c != asd_fs_delimiter) {
			if (c == asd_fs_delimiter2)
				c = asd_fs_delimiter;
			else
				path += asd_fs_delimiter;
		}
		return path;
	}


	Logger::Logger(IN const char* a_outDir,
				   IN const char* a_logName)
		: m_outDir(AddDelimiter(ConvToF(a_outDir)))
		, m_logName(ConvToF(a_logName))
		, m_logObjPool(100)
	{
		Init();
	}


	Logger::Logger(IN const wchar_t* a_outDir,
				   IN const wchar_t* a_logName)
		: m_outDir(AddDelimiter(ConvToF(a_outDir)))
		, m_logName(ConvToF(a_logName))
	{
		Init();
	}


	Logger::~Logger() asd_noexcept
	{
		m_writer.Stop(true);
	}


	void Logger::Init()
	{
		// 생성자에서만 호출됨
		MtxCtl_asdMutex lock(m_lock);
		if (RefreshLogFile(Date::Now()) == nullptr)
			asd_RaiseException("log file create fail, errno:{}", errno);

		m_writer.Start();
	}


	FILE* Logger::RefreshLogFile(IN const Date& a_today) asd_noexcept
	{
		if (a_today != m_today) {
			m_today = a_today;
			FString fn = FString::Format(_F("{}{}_{:04d}{:02d}{:02d}.txt"),
										 m_outDir, m_logName,
										 a_today.Year(), a_today.Month(), a_today.Day());
			FILE* fp = fopen(fn, _F("a+,ccs=UTF-8"));
			if (fp != nullptr) {
				if (m_logFile != nullptr)
					fclose(m_logFile);
				m_logFile = fp;
			}
		}
		return m_logFile;
	}


	void Logger::SetGenHeadDelegate(MOVE std::function<FString()>&& a_delegate) asd_noexcept
	{
		MtxCtl_asdMutex lock(m_lock);
		if (a_delegate == nullptr)
			m_genHead.reset();
		else
			m_genHead.reset(new std::function<FString()>(std::move(a_delegate)));
	}


	void Logger::SetGenTailDelegate(MOVE std::function<FString()>&& a_delegate) asd_noexcept
	{
		MtxCtl_asdMutex lock(m_lock);
		if (a_delegate == nullptr)
			m_genTail.reset();
		else
			m_genTail.reset(new std::function<FString()>(std::move(a_delegate)));
	}


	void Logger::PushLog(IN const Log& a_log) asd_noexcept
	{
		MtxCtl_asdMutex lock(m_lock);
		Log* log = m_logObjPool.Alloc();
		*log = a_log;

		m_writer.PushTask([this, log]()
		{
			Print(log);
		});
	}


	void Logger::Print(IN Log* a_log) asd_noexcept
	{
		MtxCtl_asdMutex lock(m_lock);

		Log log = std::move(*a_log);
		m_logObjPool.Free(a_log);

		auto genHead = m_genHead;
		auto genTail = m_genTail;

		DateTime now = DateTime::Now();
		FILE* fp = RefreshLogFile(now.m_date);
		errno_t e = fp!=nullptr ? 0 : errno;

		lock.unlock();

		FString msg;
		if (genHead != nullptr)
			msg = (*genHead)();
		msg << log.m_message;
		if (genTail != nullptr)
			msg << (*genTail)();
		msg << _F('\n');
		if (log.m_file != nullptr)
			msg << _F("  (") << ConvToF(log.m_file) << _F(":") << log.m_line << _F(")\n");

		if (fp != nullptr) {
			if (asd::fputs(msg, fp) < 0)
				e = errno;
			else if (log.m_flush) {
				if (Flush(fp) == false)
					e = errno;
			}
		}

		if (log.m_console != nullptr)
			asd::fputs(msg, log.m_console);

		if (log.m_sync != nullptr) {
			log.m_sync->m_errno = e;
			log.m_sync->m_event.Post();
		}
	}
}

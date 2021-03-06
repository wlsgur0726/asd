﻿#include "stdafx.h"
#include "asd/log.h"
#include "asd/datetime.h"
#include <fcntl.h>

#if defined(asd_Platform_Windows)
#	include <io.h>
#
#else
#	include <sys/stat.h>
#
#endif

namespace asd
{
#if defined(asd_Platform_Windows)
	#define asd_CCS ",ccs=UTF-16LE"
	inline FILE* fopen(const FChar* a_path,
					   const FChar* a_mode)
	{
		FILE* fp;
		if (0 != _wfopen_s(&fp, a_path, a_mode))
			fp = nullptr;
		return fp;
	}

	inline bool Flush(FILE* a_fp)
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
	#define asd_CCS
	inline bool Flush(FILE* a_fp)
	{
		if (fflush(a_fp) != 0)
			return false;
		int fd = fileno(a_fp);
		if (fd == -1)
			return false;
		return fsync(fd) == 0;
	}

#endif


	auto Logger_writer_option = []()
	{
		ThreadPoolOption option;
		option.ThreadCount = 1;
		option.UseNotifier = true;
		return option;
	};

	Logger::Logger()
		: m_writer(Logger_writer_option())
	{
		m_writer.Start();
		Init(_F(""), _F("log"));
	}


	Logger::~Logger()
	{
		m_writer.Stop();
	}


	std::shared_ptr<FILE> Logger::RefreshLogFile(const Date& a_today)
	{
		if (a_today != m_today) {
			m_today = a_today;
			asd_mkdir(m_outDir);
			FString fn = FString::Format(_F("{}{}_{:04d}-{:02d}-{:02d}.txt"),
										 m_outDir, m_logName,
										 a_today.Year(), a_today.Month(), a_today.Day());
			FILE* fp = fopen(fn, _F("a" asd_CCS));
			if (fp != nullptr)
				m_logFile.reset(fp, [](FILE* a_fp) { fclose(a_fp); });
		}
		return m_logFile;
	}


	void Logger::SetGenHeadDelegate(GenText&& a_delegate)
	{
		auto lock = GetLock(m_lock);
		if (a_delegate == nullptr)
			m_genHead.reset();
		else
			m_genHead.reset(new GenText(std::move(a_delegate)));
	}


	void Logger::SetGenTailDelegate(GenText&& a_delegate)
	{
		auto lock = GetLock(m_lock);
		if (a_delegate == nullptr)
			m_genTail.reset();
		else
			m_genTail.reset(new GenText(std::move(a_delegate)));
	}


	void Logger::PushLog(asd::Log&& a_log)
	{
		auto lock = GetLock(m_lock);
		asd::Log* log = m_logObjPool.Alloc();
		*log = std::move(a_log);

		m_writer.Push([this, log]()
		{
			Print(log);
		});
	}


	void Logger::Print(asd::Log* a_log)
	{
		auto lock = GetLock(m_lock);

		asd::Log log = std::move(*a_log);
		m_logObjPool.Free(a_log);
		auto logNumber = ++m_logNumber;
		auto genHead = m_genHead;
		auto genTail = m_genTail;

		DateTime now = DateTime::Now();
		auto fp = RefreshLogFile(now.m_date);
		int e = fp!=nullptr ? 0 : errno;

		lock.unlock();

		FString msg;
		if (genHead != nullptr)
			msg = (*genHead)(logNumber, now);

		msg << ConvToF(log);

		if (genTail != nullptr)
			msg << (*genTail)(logNumber, now);
		if (log.m_file != nullptr)
			msg << _F(" (") << ConvToF(log.m_file) << _F(':') << log.m_line << _F(")\n");
		msg << _F('\n');

		if (fp != nullptr) {
			if (asd::fputs(msg, fp.get()) < 0)
				e = errno;
			else if (log.m_flush) {
				if (Flush(fp.get()) == false)
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

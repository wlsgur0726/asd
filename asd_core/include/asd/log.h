#pragma once
#include "asdbase.h"
#include "filedef.h"
#include "semaphore.h"
#include "objpool.h"
#include "threadpool.h"
#include "datetime.h"
#include "iconvwrap.h"
#include "util.h"
#include <thread>
#include <unordered_set>
#include <stddef.h>


namespace asd
{
	struct Log final
	{
		struct Sync
		{
			Semaphore	m_event;
			int			m_errno = 0;
			inline int Wait(IN uint32_t a_timeoutMs = 1000)
			{
				if (m_event.Wait(a_timeoutMs)==false && m_errno==0)
					m_errno = ETIMEDOUT;
				return m_errno;
			}
		};

		MString					m_msgM;
		WString					m_msgW;
		FILE*					m_console = stdout;
		bool					m_flush = false;
		std::shared_ptr<Sync>	m_sync;
		const char*				m_file = nullptr;
		int						m_line = 0;

		inline void SetMsg(IN const MString& a_msg) asd_noexcept
		{
			m_msgM = a_msg;
		}

		inline void SetMsg(IN const WString& a_msg) asd_noexcept
		{
			m_msgW = a_msg;
		}
	};



#if defined(asd_Platform_Windows)
	inline FString ConvToF(IN const char* a_str)
	{
		return ConvToW(a_str);
	}

	inline FString ConvToF(IN const wchar_t* a_str)
	{
		return a_str;
	}

	inline FString ConvToF(IN const Log& a_log)
	{
		if (a_log.m_msgW.size() > 0)
			return a_log.m_msgW;
		return ConvToF(a_log.m_msgM);
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

	inline FString ConvToF(IN const Log& a_log)
	{
		if (a_log.m_msgM.size() > 0)
			return a_log.m_msgM;
		return ConvToF(a_log.m_msgW);
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



	class Logger
		: public Global<Logger>
	{
	public:
		typedef std::function<FString(IN uint64_t a_logNumber, IN DateTime a_now)> GenText;


	private:
		Mutex m_lock;
		FString m_outDir;
		FString m_logName;
		std::shared_ptr<FILE> m_logFile = nullptr;
		Date m_today; // 로그파일명 리프레시를 위한 플래그
		uint64_t m_logNumber = 0;

		ThreadPool m_writer = ThreadPool(1);
		ObjectPool<asd::Log> m_logObjPool = ObjectPool<asd::Log>(100);

		std::shared_ptr<GenText> m_genHead;
		std::shared_ptr<GenText> m_genTail;

		std::shared_ptr<FILE> RefreshLogFile(IN const Date& a_today) asd_noexcept;
		void PushLog(MOVE Log&& a_log) asd_noexcept;
		void Print(IN Log* a_log) asd_noexcept;


	public:
		Logger();
		virtual ~Logger() asd_noexcept;


		template <typename CharType>
		bool Init(const CharType* a_outDir,
				  const CharType* a_logName)
		{
			MtxCtl_asdMutex lock(m_lock);
			m_outDir = AddDelimiter(ConvToF(a_outDir));
			m_logName = ConvToF(a_logName);
			m_today = Date();
			return RefreshLogFile(Date::Now()) != nullptr;
		}


		void SetGenHeadDelegate(MOVE GenText&& a_delegate) asd_noexcept;
		void SetGenTailDelegate(MOVE GenText&& a_delegate) asd_noexcept;


		template <typename CharType, typename... Args>
		inline void Log(IN const CharType* a_format,
						IN const Args&... a_args) asd_noexcept
		{
			asd::Log log;
			log.SetMsg(BasicString<CharType>::Format(a_format, a_args...));
			PushLog(std::move(log));
		}


		template <typename CharType, typename... Args>
		inline int LogSync(IN const CharType* a_format,
						   IN const Args&... a_args)
		{
			auto sync = std::make_shared<asd::Log::Sync>();
			asd::Log log;
			log.m_sync = sync;
			log.SetMsg(BasicString<CharType>::Format(a_format, a_args...));
			PushLog(std::move(log));
			return sync->Wait();
		}


		template <typename CharType, typename... Args>
		inline int LogFlush(IN const CharType* a_format,
							IN const Args&... a_args)
		{
			auto sync = std::make_shared<asd::Log::Sync>();
			asd::Log log;
			log.m_flush = true;
			log.m_sync = sync;
			log.SetMsg(BasicString<CharType>::Format(a_format, a_args...));
			PushLog(std::move(log));
			return sync->Wait();
		}


		template <typename CharType, typename... Args>
		inline int _ErrorLog(IN const char* a_filename,
							 IN int a_line,
							 IN const CharType* a_format,
							 IN const Args&... a_args)
		{
			auto sync = std::make_shared<asd::Log::Sync>();
			asd::Log log;
			log.m_file = a_filename;
			log.m_line = a_line;
			log.m_console = stderr;
			log.m_flush = true;
			log.m_sync = sync;
			log.SetMsg(BasicString<CharType>::Format(a_format, a_args...));
			PushLog(std::move(log));
			return sync->Wait();
		}
#if defined(asd_Compiler_MSVC)
#	define ErrorLog(format, ...) _ErrorLog(__FILE__, __LINE__, format, __VA_ARGS__)
#else
#	define ErrorLog(...) _ErrorLog(__FILE__, __LINE__, __VA_ARGS__)
#endif
	};
}

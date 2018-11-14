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
			inline int Wait(uint32_t a_timeoutMs = 1000)
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

		inline void SetMsg(const MString& a_msg)
		{
			m_msgM = a_msg;
		}

		inline void SetMsg(const WString& a_msg)
		{
			m_msgW = a_msg;
		}
	};



#if defined(asd_Platform_Windows)
	inline FString ConvToF(const char* a_str)
	{
		return ConvToW(a_str);
	}

	inline FString ConvToF(const wchar_t* a_str)
	{
		return a_str;
	}

	inline FString ConvToF(const Log& a_log)
	{
		if (a_log.m_msgW.size() > 0)
			return a_log.m_msgW;
		return ConvToF(a_log.m_msgM);
	}

#else
	inline FString ConvToF(const char* a_str)
	{
		return a_str;
	}

	inline FString ConvToF(const wchar_t* a_str)
	{
		return ConvToM(a_str);
	}

	inline FString ConvToF(const Log& a_log)
	{
		if (a_log.m_msgM.size() > 0)
			return a_log.m_msgM;
		return ConvToF(a_log.m_msgW);
	}

#endif


	inline FString AddDelimiter(FString&& a_path)
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
		using GenText = std::function<FString(uint64_t a_logNumber, DateTime a_now)>;


	private:
		Mutex m_lock;
		FString m_outDir;
		FString m_logName;
		std::shared_ptr<FILE> m_logFile = nullptr;
		Date m_today; // 로그파일명 리프레시를 위한 플래그
		uint64_t m_logNumber = 0;

		ThreadPool m_writer;
		ObjectPool<asd::Log> m_logObjPool = ObjectPool<asd::Log>(100);

		std::shared_ptr<GenText> m_genHead;
		std::shared_ptr<GenText> m_genTail;

		std::shared_ptr<FILE> RefreshLogFile(const Date& a_today);
		void PushLog(Log&& a_log);
		void Print(Log* a_log);


	public:
		Logger();
		virtual ~Logger();


		template <typename CharType>
		bool Init(const CharType* a_outDir,
				  const CharType* a_logName)
		{
			auto lock = GetLock(m_lock);
			m_outDir = AddDelimiter(ConvToF(a_outDir));
			m_logName = ConvToF(a_logName);
			m_today = Date();
			return RefreshLogFile(Date::Now()) != nullptr;
		}


		void SetGenHeadDelegate(GenText&& a_delegate);
		void SetGenTailDelegate(GenText&& a_delegate);


		template <typename CharType, typename... Args>
		inline void Log(const BasicString<CharType>& a_format,
						const Args&... a_args)
		{
			Log(a_format.c_str(), a_args...);
		}

		template <typename CharType, typename... Args>
		inline void Log(const CharType* a_format,
						const Args&... a_args)
		{
			asd::Log log;
			log.SetMsg(BasicString<CharType>::Format(a_format, a_args...));
			PushLog(std::move(log));
		}


		template <typename CharType, typename... Args>
		inline int LogSync(const BasicString<CharType>& a_format,
						   const Args&... a_args)
		{
			return LogSync(a_format.c_str(), a_args...);
		}

		template <typename CharType, typename... Args>
		inline int LogSync(const CharType* a_format,
						   const Args&... a_args)
		{
			auto sync = std::make_shared<asd::Log::Sync>();
			asd::Log log;
			log.m_sync = sync;
			log.SetMsg(BasicString<CharType>::Format(a_format, a_args...));
			PushLog(std::move(log));
			return sync->Wait();
		}


		template <typename CharType, typename... Args>
		inline int LogFlush(const BasicString<CharType> a_format,
							const Args&... a_args)
		{
			return LogFlush(a_format.c_str(), a_args...);
		}

		template <typename CharType, typename... Args>
		inline int LogFlush(const CharType* a_format,
							const Args&... a_args)
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
		inline int _ErrorLog(const char* a_filename,
							 int a_line,
							 const BasicString<CharType>& a_format,
							 const Args&... a_args)
		{
			return _ErrorLog(a_filename, a_line, a_format.c_str(), a_args...);
		}

		template <typename CharType, typename... Args>
		inline int _ErrorLog(const char* a_filename,
							 int a_line,
							 const CharType* a_format,
							 const Args&... a_args)
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

		// __VA_ARGS__ : format, ...
		#define ErrorLog(...) _ErrorLog(__FILE__, __LINE__, __VA_ARGS__)
	};
}

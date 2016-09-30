#pragma once
#include "asdbase.h"
#include "filedef.h"
#include "semaphore.h"
#include "objpool.h"
#include "threadpool.h"
#include "datetime.h"
#include <thread>
#include <queue>

namespace asd
{
	struct Log
	{
		struct Sync
		{
			Semaphore	m_event;
			errno_t		m_errno = 0;
		};

		FString		m_message;
		FILE*		m_console = nullptr;
		bool		m_flush = false;
		Sync*		m_sync = nullptr;
		const char*	m_file = nullptr;
		int			m_line = 0;
	};



	class Logger
	{
		Mutex m_lock;
		const FString m_outDir;
		const FString m_logName;
		FILE* m_logFile = nullptr;
		Date m_today;

		ThreadPool m_writer = ThreadPool(1);
		ObjectPool<Log, false> m_logObjPool = ObjectPool<Log, false>(100);

		std::shared_ptr<std::function<FString()>> m_genHead;
		std::shared_ptr<std::function<FString()>> m_genTail;

		void Init();
		FILE* RefreshLogFile(IN const Date& a_today) asd_noexcept;
		void Print(IN Log* a_log) asd_noexcept;


	public:
		Logger(IN const char* a_outDir,
			   IN const char* a_logName);

		Logger(IN const wchar_t* a_outDir,
			   IN const wchar_t* a_logName);

		virtual ~Logger() asd_noexcept;

		void SetGenHeadDelegate(MOVE std::function<FString()>&& a_delegate) asd_noexcept;
		void SetGenTailDelegate(MOVE std::function<FString()>&& a_delegate) asd_noexcept;
		void PushLog(IN const Log& a_log) asd_noexcept;
	};
}

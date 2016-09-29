#pragma once
#include "asdbase.h"
#include "string.h"
#include "semaphore.h"
#include "objpool.h"
#include <thread>
#include <queue>

namespace asd
{
	struct Log
	{
		MString		m_message;
		FILE*		m_console;
		bool		m_flush;
		Semaphore*	m_sync;
		const char*	m_file;
		int			m_line;

		inline Log(IN const MString& a_message = MString(),
				   IN FILE* a_console = nullptr,
				   IN bool a_flush = false,
				   IN Semaphore* a_sync = nullptr,
				   IN const char* a_file = nullptr,
				   IN int a_line = 0)
			: m_message(a_message)
			, m_console(a_console)
			, m_flush(a_flush)
			, m_sync(a_sync)
			, m_file(a_file)
			, m_line(a_line)
		{
		}
	};



	class Logger
	{
		// Windows에서 MString은 유니코드가 아니지만 파일경로는 유니코드가 필수이므로 wide 사용
		const WString m_outDir;
		const WString m_logName;
		FILE* m_logFile = nullptr;

		Mutex m_lock;
		ObjectPool<Log, false> m_logObjPool;
		std::queue<Log*> m_queue;
		bool m_stop = true;
		std::thread m_writer;

		std::function<MString()> m_genHead;
		std::function<MString()> m_genTail;

		void Init();

	public:
		Logger(IN const MString& a_outDir,
			   IN const MString& a_logName);

		Logger(IN const WString& a_outDir,
			   IN const WString& a_logName);

		virtual ~Logger() asd_noexcept;

		void SetGenHeadDelegate(MOVE std::function<MString()>&& a_delegate) asd_noexcept;
		void SetGenTailDelegate(MOVE std::function<MString()>&& a_delegate) asd_noexcept;
		void PushLog(IN const Log& a_log) asd_noexcept;
	};
}

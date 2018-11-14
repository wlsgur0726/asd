#include "stdafx.h"
#include "asd/filedef.h"
#include "asd/lock.h"
#include "asd/memdump.h"
#include "asd/iconvwrap.h"
#include "asd/datetime.h"
#include "asd/util.h"

#if asd_Platform_Windows
#	include <DbgHelp.h>
#	pragma comment(lib,"DbgHelp.lib")
#
#else
#	include <sys/stat.h>
#	include <sys/types.h>
#	include <unistd.h>
#
#endif

namespace asd
{
	namespace MemDump
	{
#if asd_Platform_Windows
		const FChar g_dump_pre[] = _F("");
		const FChar g_dump_ext[] = _F(".dmp");

		void CopyToBuf_Internal(const wchar_t* a_str,
								FChar* a_dst /*Out*/,
								size_t a_dstBufCnt)
		{
			asd::strcpy(a_dst, a_str, a_dstBufCnt);
		}

		void GetProcessName_Internal(FChar* a_dst /*Out*/,
									 DWORD a_dstBufCnt)
		{
			auto len = ::GetModuleFileNameW(NULL, a_dst, a_dstBufCnt);
			for (auto i=len; i>0;) {
				if (a_dst[--i] == asd_fs_delimiter) {
					for (auto j=++i; j<=len; ++j)
						a_dst[j-i] = a_dst[j];
					break;
				}
			}
		}

#else
		const FChar g_dump_pre[] = _F("core.");
		const FChar g_dump_ext[] = _F("");

		void CopyToBuf_Internal(const wchar_t* a_str,
								FChar* a_dst /*Out*/,
								size_t a_dstBufCnt)
		{
			asd_DAssert(Encoding::UTF8 == GetDefaultEncoding<char>());
			MString conv = ConvToM(a_str);
			asd::strcpy(a_dst, conv.c_str(), a_dstBufCnt);
		}

		void GetProcessName_Internal(FChar* a_dst /*Out*/,
									 size_t a_dstBufCnt)
		{
			const char* name = getenv("_");
			asd::strcpy(a_dst, name, a_dstBufCnt);
			auto len = asd::strlen(a_dst);
			for (auto i=len; i>0;) {
				if (a_dst[--i] == asd_fs_delimiter) {
					for (auto j=++i; j<=len; ++j)
						a_dst[j-i] = a_dst[j];
					break;
				}
			}
		}

#endif

#define asd_CopyToBuf(src, dst) CopyToBuf_Internal(src, dst, sizeof(dst)/sizeof(dst[0]))
#define asd_GetProcessName(dst) GetProcessName_Internal(dst, sizeof(dst)/sizeof(dst[0]))

		const size_t LEN_PATH = 4096;
		const size_t LEN_NAME = 256;

		Mutex g_lock;

		FChar g_path[LEN_PATH] = {0};
		FChar g_path_temp[LEN_PATH] = {0};

		FChar g_name[LEN_NAME] = {0};
		FChar g_name_temp[LEN_NAME] = {0};


		void SetOutPath(const wchar_t* a_path)
		{
			auto lock = GetLock(g_lock);
			asd_CopyToBuf(a_path, g_path);
		}


		void SetDefaultName(const wchar_t* a_name)
		{
			auto lock = GetLock(g_lock);
			asd_CopyToBuf(a_name, g_name);
		}


		void Ready(const wchar_t* a_name)
		{
			asd_mkdir(g_path);

			FChar* name;
			if (a_name != nullptr) {
				asd_CopyToBuf(a_name, g_name_temp);
				name = g_name_temp;
			}
			else {
				if (g_name[0] == '\0')
					asd_GetProcessName(g_name);
				name = g_name;
			}

			DateTime now = DateTime::Now();
			uint32_t pid = GetCurrentProcessID();
			uint32_t tid = GetCurrentThreadID();

			asd::sprintf(g_path_temp,
						 LEN_PATH,
						 //  1 2 3 4     5     6  7    8    9    A    B    C    D
						 _F("%s%c%s%s_pid%u_tid%u_%04d-%02d-%02d_%02dh%02dm%02ds%s"),
						 g_path,                                             // 1
						 g_path[0] != '\0' ? asd_fs_delimiter : _F('\0'),    // 2
						 g_dump_pre, name,                                   // 3, 4
						 pid, tid,                                           // 5, 6
						 now.Year(), now.Month(), now.Day(),                 // 7, 8, 9
						 now.Hour(), now.Minute(), now.Second(),             // A, B, C
						 g_dump_ext);                                        // D
		}


#if asd_Platform_Windows
		thread_local const wchar_t* t_Create_Arg = nullptr;

		void Create(const wchar_t* a_name /*= nullptr*/)
		{
			t_Create_Arg = a_name;
			__try {
				static int z = 0;
				z = 1 / z; // 'divide by zero' 발생시켜서 CreateMiniDump 함수 호출
			}
			__except (CreateMiniDump(GetExceptionInformation())) {
			}
		}

		long CreateMiniDump(void* a_PEXCEPTION_POINTERS)
		{
			auto lock = GetLock(g_lock);
			Ready(t_Create_Arg);
			t_Create_Arg = nullptr;

			HANDLE dumpFile = ::CreateFileW(g_path_temp,
											GENERIC_WRITE,
											0,
											nullptr,
											CREATE_ALWAYS,
											FILE_ATTRIBUTE_NORMAL,
											nullptr);
			if (dumpFile == INVALID_HANDLE_VALUE)
				return EXCEPTION_EXECUTE_HANDLER;

			MINIDUMP_EXCEPTION_INFORMATION info;
			info.ThreadId = ::GetCurrentThreadId();
			info.ExceptionPointers = (PEXCEPTION_POINTERS)a_PEXCEPTION_POINTERS;
			info.ClientPointers = FALSE;

			MINIDUMP_TYPE option = (MINIDUMP_TYPE)(MiniDumpWithFullMemory |
												   MiniDumpWithHandleData |
												   MiniDumpWithProcessThreadData |
												   MiniDumpWithFullMemoryInfo |
												   MiniDumpWithThreadInfo);

			::MiniDumpWriteDump(::GetCurrentProcess(),
								::GetCurrentProcessId(),
								dumpFile,
								option,
								&info,
								nullptr,
								nullptr);

			::CloseHandle(dumpFile);
			return EXCEPTION_EXECUTE_HANDLER;
		}

#else
		void Create(const wchar_t* a_name /*= nullptr*/)
		{
			auto lock = GetLock(g_lock);
			Ready(a_name);

			// gcore가 설치되어 있어야 함
			// 추후 google-coredumper (https://code.google.com/archive/p/google-coredumper/) 도입 검토 요망
			static FChar g_command[LEN_PATH] = {0};
			asd::sprintf(g_command,
						 LEN_PATH,
						 "gcore -o \"%s\" %u",
						 g_path_temp,
						 ::getpid());
			::system(g_command);
		}

#endif
	};
}

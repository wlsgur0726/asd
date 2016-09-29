#include "asd_pch.h"
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
		#define asd_str(str) L ## str
		#define asd_mkdir(path) _wmkdir(path)
		typedef wchar_t Char;
		const Char g_path_delimiter[] = asd_str("\\");
		const Char g_dump_ext[] = asd_str(".dmp");

		void CopyToBuf_Internal(IN const wchar_t* a_str,
								OUT Char* a_dst,
								IN size_t a_dstBufCnt)
		{
			asd::strcpy(a_dst, a_str, a_dstBufCnt);
		}

		void GetProcessName_Internal(OUT Char* a_dst,
									 IN size_t a_dstBufCnt)
		{
			auto len = GetModuleFileNameW(NULL, a_dst, a_dstBufCnt);
			for (auto i=len; i>0;) {
				if (a_dst[--i] == g_path_delimiter[0]) {
					for (auto j=++i; j<=len; ++j)
						a_dst[j-i] = a_dst[j];
					break;
				}
			}
		}

#else
		#define asd_str(str) str
		#define asd_mkdir(path) mkdir(path, 0777)
		typedef char Char;
		const Char g_path_delimiter[] = asd_str("/");
		const Char g_dump_ext[] = asd_str(".core");

		void CopyToBuf_Internal(IN const wchar_t* a_str,
								OUT Char* a_dst,
								IN size_t a_dstBufCnt)
		{
			assert(Encoding::UTF8 == GetDefaultEncoding<char>());
			MString conv = ConvToM(a_str);
			asd::strcpy(a_dst, conv.c_str(), a_dstBufCnt);
		}

		void GetProcessName_Internal(OUT Char* a_dst,
									 IN size_t a_dstBufCnt)
		{
			const char* name = getenv("_");
			asd::strcpy(a_dst, name, a_dstBufCnt);
			auto len = asd::strlen(a_dst);
			for (auto i=len; i>0;) {
				if (a_dst[--i] == g_path_delimiter[0]) {
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

		Char g_path[LEN_PATH] = {0};
		Char g_path_temp[LEN_PATH] = {0};

		Char g_name[LEN_NAME] = {0};
		Char g_name_temp[LEN_NAME] = {0};


		void SetOutPath(IN const wchar_t* a_path)
		{
			MtxCtl_asdMutex lock(g_lock);
			asd_CopyToBuf(a_path, g_path);
		}


		void SetDefaultName(IN const wchar_t* a_name)
		{
			MtxCtl_asdMutex lock(g_lock);
			asd_CopyToBuf(a_name, g_name);
		}


		void Ready(IN const wchar_t* a_name)
		{
			asd_mkdir(g_path);

			Char* name;
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
						 asd_str("%s%s%s_pid%u_tid%u_%04d-%02d-%02d_%02dh%02dm%02ds%s"),
						 g_path,
						 g_path[0] != '\0' ? g_path_delimiter : asd_str(""),
						 name,
						 pid, tid,
						 now.Year(), now.Month(), now.Day(),
						 now.Hour(), now.Minute(), now.Second(),
						 g_dump_ext);
		}


#if asd_Platform_Windows
		const wchar_t* g_Create_Arg = nullptr;

		void Create(IN const wchar_t* a_name /*= nullptr*/)
		{
			g_Create_Arg = a_name;
			__try {
				static int z = 0;
				z = 1 / z; // 'divide by zero' 발생시켜서 CreateMiniDump 함수 호출
			}
			__except (CreateMiniDump(GetExceptionInformation())) {
			}
		}

		long CreateMiniDump(IN void* a_PEXCEPTION_POINTERS)
		{
			MtxCtl_asdMutex lock(g_lock);
			Ready(g_Create_Arg);
			g_Create_Arg = nullptr;

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
		void Create(IN const wchar_t* a_name /*= nullptr*/)
		{
			MtxCtl_asdMutex lock(g_lock);
			Ready(a_name);

			// gcore가 설치되어 있어야 함
			// 추후 google-coredumper (https://code.google.com/archive/p/google-coredumper/) 도입 검토 요망
			static Char g_command[LEN_PATH] = {0};
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

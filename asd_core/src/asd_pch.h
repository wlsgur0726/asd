// PreCompiled Header
#pragma once

#include "asd/asdbase.h"
#include "asd/exception.h"


#if defined(asd_Platform_Windows)
#	if defined(asd_Compiler_MSVC)
#		define _WINSOCK_DEPRECATED_NO_WARNINGS
#	endif
#	define UMDF_USING_NTSTATUS
#	include <ntstatus.h>
#	include <WinSock2.h>
#	include <Windows.h>
#
#endif


#if defined(asd_Platform_Android) || defined(asd_Platform_Linux)
#	include <pthread.h>
#	include <unistd.h>
#
#endif

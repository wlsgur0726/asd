#pragma once
#include "asdbase.h"

namespace asd
{
	namespace MemDump
	{
		void SetOutPath(const wchar_t* a_path);
		void SetDefaultName(const wchar_t* a_name);
		void Create(const wchar_t* a_name = nullptr);

#if asd_Platform_Windows
		long CreateMiniDump(void* a_PEXCEPTION_POINTERS);
#endif
	};
}

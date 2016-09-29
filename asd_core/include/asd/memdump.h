#pragma once
#include "asdbase.h"


namespace asd
{
	namespace MemDump
	{
		void SetOutPath(IN const wchar_t* a_path);
		void SetDefaultName(IN const wchar_t* a_name);
		void Create(IN const wchar_t* a_name = nullptr);
	};
}
#pragma once
#include "asdbase.h"
#include "string.h"
#include <fstream>


// 파일경로를 다룰 때 유니코드를 사용해야 하지만
// Windows에서 char는 유니코드가 아니기 때문에 wchar_t와 wide버전 함수를 사용한다.

#if defined(asd_Platform_Windows)
#	define _F(txt) L ## txt
#	define asd_fs_delimiter '\\'
#	define asd_fs_delimiter2 '/'
#	define asd_newline L"\r\n"
#	define asd_mkdir(path) _wmkdir(path)
#
#else
#	define _F(txt) txt
#	define asd_fs_delimiter '/'
#	define asd_fs_delimiter2 '\\'
#	define asd_newline "\n"
#	define asd_mkdir(path) mkdir(path, 0777)
#
#endif

namespace asd
{
#if defined(asd_Platform_Windows)
	typedef wchar_t			FChar;
	typedef WString			FString;
	typedef std::wstring	Fstring;
	typedef std::wfstream	fstream;
	typedef std::wifstream	ifstream;
	typedef std::wofstream	ofstream;

#else
	typedef char			FChar;
	typedef MString			FString;
	typedef std::string		Fstring;
	typedef std::fstream	fstream;
	typedef std::ifstream	ifstream;
	typedef std::ofstream	ofstream;

#endif
}
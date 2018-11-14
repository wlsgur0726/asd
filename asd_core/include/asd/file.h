#pragma once
#include "asdbase.h"
#include "filedef.h"
#include <memory>
#include <cstdio>


namespace asd
{
	class File
	{
	public:
		File();

		File(const char* a_path,
			 const char* a_mode = "");

		File(const wchar_t* a_path,
			 const wchar_t* a_mode = L"");

		void Open(const char* a_path,
				  const char* a_mode = "");

		void Open(const wchar_t* a_path,
				  const wchar_t* a_mode = L"");

		size_t Read(void* a_buffer /*Out*/,
					size_t a_elemSize,
					size_t a_elemCount);

		size_t Write(const void* a_buffer,
					 size_t a_elemSize,
					 size_t a_elemCount);

		int GetLastError() const;

		static_assert(sizeof(off_t)==8 || sizeof(off_t)==4,
					  "unexpected size of off_t");

		int Seek(off_t a_offset,
				 int a_whence);

		off_t Tell();

		void Close();

	private:
		std::shared_ptr<FILE> m_file;
		int m_lastError = -1;
	};
}

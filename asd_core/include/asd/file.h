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
		File() asd_noexcept;

		File(IN const char* a_path,
			 IN const char* a_mode = "") asd_noexcept;

		File(IN const wchar_t* a_path,
			 IN const wchar_t* a_mode = L"") asd_noexcept;

		void Open(IN const char* a_path,
				  IN const char* a_mode = "") asd_noexcept;

		void Open(IN const wchar_t* a_path,
				  IN const wchar_t* a_mode = L"") asd_noexcept;

		size_t Read(OUT void* a_buffer,
					IN size_t a_elemSize,
					IN size_t a_elemCount) asd_noexcept;

		size_t Write(IN const void* a_buffer,
					 IN size_t a_elemSize,
					 IN size_t a_elemCount) asd_noexcept;

		int GetLastError() const;

		static_assert(sizeof(off_t)==8 || sizeof(off_t)==4,
					  "unexpected size of off_t");

		int Seek(IN off_t a_offset,
				 IN int a_whence);

		off_t Tell();

		void Close() asd_noexcept;

	private:
		std::shared_ptr<FILE> m_file;
		int m_lastError = -1;
	};
}

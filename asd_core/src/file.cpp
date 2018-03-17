#include "asd_pch.h"
#include "asd/file.h"
#include "asd/iconvwrap.h"


namespace asd
{
	inline void SetPointer(REF std::shared_ptr<FILE>& a_ptr,
						   IN FILE* a_fp)
	{
		a_ptr.reset(a_fp, [](IN FILE* a_del)
		{
			::fclose(a_del);
		});
	}


	File::File()
	{
	}

	File::File(IN const char* a_path,
			   IN const char* a_mode /*= ""*/)
	{
		Open(a_path, a_mode);
	}

	File::File(IN const wchar_t* a_path,
			   IN const wchar_t* a_mode /*= L""*/)
	{
		Open(a_path, a_mode);
	}

	void File::Open(IN const char* a_path,
					IN const char* a_mode /*= ""*/)
	{
#if asd_Platform_Windows
		FILE* fp;
		m_lastError = ::fopen_s(&fp, a_path, a_mode);
		if (m_lastError != 0)
			return;
#else
		auto fp = ::fopen(a_path, a_mode);
		if (fp == nullptr) {
			m_lastError = errno;
			return;
		}
		m_lastError = 0;
#endif
		SetPointer(m_file, fp);
	}

	void File::Open(IN const wchar_t* a_path,
					IN const wchar_t* a_mode /*= L""*/)
	{
#if asd_Platform_Windows
		FILE* fp;
		m_lastError = ::_wfopen_s(&fp, a_path, a_mode);
		if (m_lastError != 0)
			return;
		SetPointer(m_file, fp);

#else
		auto path = ConvToM(a_path);
		auto mode = ConvToM(a_mode);
		Open(path, mode);

#endif
	}

	size_t File::Read(OUT void* a_buffer,
					  IN size_t a_elemSize,
					  IN size_t a_elemCount)
	{
		if (m_file == nullptr) {
			m_lastError = EBADF;
			return 0;
		}

		size_t ret = ::fread(a_buffer,
							 a_elemSize,
							 a_elemCount,
							 m_file.get());
		m_lastError = errno;
		return ret;
	}

	size_t File::Write(IN const void* a_buffer,
					   IN size_t a_elemSize,
					   IN size_t a_elemCount)
	{
		if (m_file == nullptr) {
			m_lastError = EBADF;
			return 0;
		}

		size_t ret = ::fwrite(a_buffer,
							  a_elemSize,
							  a_elemCount,
							  m_file.get());
		m_lastError = errno;
		return ret;
	}

	int File::GetLastError() const
	{
		return m_lastError;
	}

	int asd::File::Seek(IN off_t a_offset,
						IN int a_whence)
	{
		if (m_file == nullptr) {
			m_lastError = EBADF;
			return -1;
		}

		int ret;
#if asd_Platform_Windows
		if (sizeof(off_t) == 8)
			ret = ::_fseeki64(m_file.get(), a_offset, a_whence);
		else
			ret = ::fseek(m_file.get(), a_offset, a_whence);
#else
		ret = ::fseeko(m_file.get(), a_offset, a_whence);
#endif
		if (ret == 0)
			m_lastError = 0;
		else
			m_lastError = errno;
		return ret;
	}

	off_t asd::File::Tell()
	{
		if (m_file == nullptr) {
			m_lastError = EBADF;
			return -1;
		}

		off_t offset;
#if asd_Platform_Windows
		if (sizeof(off_t) == 8)
			offset = (off_t)::_ftelli64(m_file.get());
		else
			offset = (off_t)::ftell(m_file.get());
#else
		offset = ::ftello(m_file.get());
#endif
		if (offset >= 0)
			m_lastError = 0;
		else
			m_lastError = errno;
		return offset;
	}

	void File::Close()
	{
		m_file.reset();
	}
}

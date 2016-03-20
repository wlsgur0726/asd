﻿#include "stdafx.h"
#include "asd/exception.h"

namespace asd
{
	const char DebugInfo::ToStringFormat[] = "[%s(%d) %s] %s ";

	DebugInfo::DebugInfo(IN const char* a_file,
						 IN const int a_line,
						 IN const char* a_function,
						 IN const char* a_comment /*= ""*/,
						 IN ...) asd_noexcept
						 : m_file(a_file)
						 , m_line(a_line)
						 , m_function(a_function)
	{
		assert(a_file != nullptr);
		assert(m_line > 0);
		assert(a_function != nullptr);

		va_list args;
		va_start(args, a_comment);
		m_comment.FormatV(a_comment, args);
		va_end(args);
	}

	MString DebugInfo::ToString() const asd_noexcept
	{
		MString s;
		s.Format(ToStringFormat,
				 m_file, 
				 m_line, 
				 m_function, 
				 m_comment.GetData());
		return s;
	}

	Exception::Exception()  asd_noexcept
	{
		m_what = "unknown asd::Exception ";
	}

	Exception::Exception(const char* a_what) asd_noexcept
	{
		m_what = a_what;
	}

	Exception::Exception(const MString& a_what) asd_noexcept
	{
		m_what = a_what;
	}

	Exception::Exception(const DebugInfo& a_dbginfo) asd_noexcept
	{
		m_what = a_dbginfo.ToString();
	}
	
	Exception::~Exception() asd_noexcept
	{
	}

	const char* Exception::what() const asd_noexcept
	{
		return m_what;
	}
}
#include "asd_pch.h"
#include "asd/trace.h"
#include "asd/threadutil.h"


namespace asd
{
	Trace::Trace(IN const char* a_file,
				 IN int a_line,
				 IN const char* a_function) asd_noexcept
	{
		Time = std::chrono::system_clock::now();
		TID = GetCurrentThreadID();
		File = a_file;
		Line = a_line;
		Function = a_function;
	}

	MString Trace::ToString() const asd_noexcept
	{
		auto now = std::chrono::system_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - Time);
		return MString::Format("[{}ms ago][TID:{}] {}:{} {}",
							   elapsed.count(),
							   TID,
							   File,
							   Line,
							   Function);
	}
}

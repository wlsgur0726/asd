#include "asd_pch.h"
#include "asd/trace.h"
#include "asd/threadutil.h"

#if asd_Compiler_MSVC
#	include "asd/iconvwrap.h"
#	include <DbgHelp.h>
#
#elif asd_Compiler_GCC
#	include <execinfo.h>
#
#endif


namespace asd
{
	MString g_unknown = "?";

#if asd_Compiler_MSVC
	MString g_currentFileName;
	HANDLE g_currentProcessHandle = NULL;

	struct Init
	{
		Init()
		{
			g_currentProcessHandle = ::OpenProcess(PROCESS_ALL_ACCESS, FALSE, ::GetCurrentProcessId());
			if (g_currentProcessHandle == NULL) {
				auto e = ::GetLastError();
				std::terminate();
			}

			// 실행파일과 pdb파일이 같은 경로에 있다고 가정
			wchar_t buf[4096];
			::GetModuleFileNameW(NULL, buf, sizeof(buf)/sizeof(buf[0]));
			g_currentFileName = ConvToM(buf);

			WString path = buf;
			size_t del = 0;
			for (auto it=path.rbegin(); it!=path.rend(); ++it) {
				auto c = *it;
				if (c=='/' || c=='\\' || c==':')
					break;
				++del;
			}
			size_t cut = path.size() - del;
			path.resize(cut);

			if (FALSE == ::SymInitializeW(g_currentProcessHandle, path.c_str(), TRUE)) {
				auto e = ::GetLastError();
				std::terminate();
			}
			::SymSetOptions(SYMOPT_LOAD_LINES);
		}

		~Init()
		{
			if (g_currentProcessHandle == NULL)
				return;
			::SymCleanup(g_currentProcessHandle);
			::CloseHandle(g_currentProcessHandle);
		}
	} g_init;

	StackTrace::StackTrace(IN uint32_t a_skip /*= 0*/,
						   IN uint32_t a_count /*= 10*/) asd_noexcept
	{
		thread_local std::vector<void*> frames;
		frames.resize(a_count);
		auto frameCount = ::CaptureStackBackTrace(a_skip+1, a_count, frames.data(), NULL);

		for (int i=0; i<frameCount; ++i) {
			StackFrame frame;
			uint8_t symbol_buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
			SYMBOL_INFO* symbol = (SYMBOL_INFO*)symbol_buffer;

			symbol->MaxNameLen = MAX_SYM_NAME;
			symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
			if (FALSE == ::SymFromAddr(g_currentProcessHandle, (DWORD64)(frames[i]), 0, symbol)) {
				auto e = ::GetLastError();
				frame.Function = g_unknown;
				frame.Module = g_unknown;
			}
			else {
				frame.Function = symbol->Name;
				frame.Module = g_currentFileName;
			}

			DWORD d;
			IMAGEHLP_LINE64 line;
			if (FALSE == ::SymGetLineFromAddr64(g_currentProcessHandle, (DWORD64)(frames[i]), &d, &line)) {
				auto e = ::GetLastError();
				frame.File = g_unknown;
				frame.Line = 0;
			}
			else {
				frame.File = line.FileName;
				frame.Line = line.LineNumber;
			}
			emplace_back(std::move(frame));
		}
	}

#elif asd_Compiler_GCC
	StackTrace::StackTrace(IN uint32_t a_skip /*= 0*/,
						   IN uint32_t a_count /*= 10*/) asd_noexcept
	{
		std::vector<void*> trace;
		trace.resize(a_skip + a_count + 1);
		int count = ::backtrace(trace.data(), trace.size());

		std::vector<MString> symbols;
		{
			char** s = backtrace_symbols(trace.data(), count);
			if (s == NULL) {
				// ...
				return;
			}
			for (int i=1; i<count; ++i)
				symbols.emplace_back(s[i]);
			::free(s);
		}

		auto execute = [](const char* cmd, MString& out)
		{
			FILE* fp = ::popen(cmd, "r");
			if (!fp)
				return errno;
			const size_t BUFSIZE = 1023;
			char buf[BUFSIZE+1];
			size_t sz;
			do {
				sz = ::fread(buf, sizeof(char), BUFSIZE, fp);
				if (sz > 0) {
					buf[sz] = '\0';
					out += buf;
				}
			} while (sz == BUFSIZE);
			pclose(fp);
			return 0;
		};

		for (int i=1; i<count; ++i) {
			StackFrame frame;
			size_t cut1;
			size_t cut2;
			auto& symbol = symbols[i-1];

			cut1 = symbol.find('(');
			frame.Module = symbol;
			frame.Module.resize(cut1);

			MString addr;
			if (symbol[++cut1] == '+') {
				cut2 = symbol.find(')', ++cut1);
				addr = symbol.substr(cut1, cut2-cut1);
			}
			else {
				addr = trace[i];
			}
			auto cmd = MString::Format("addr2line -Cfe \"{}\" {}",
									   frame.Module,
									   addr);
			MString out;
			if (0 != execute(cmd.c_str(), out)) {
				frame.Function = g_unknown;
				frame.File = g_unknown;
				continue;
			}

			cut1 = out.find('\n');
			cut2 = out.find(':', cut1);
			frame.Function = out.substr(0, cut1++);
			frame.File = out.substr(cut1, cut2-cut1);

			++cut2;
			auto line = out.substr(cut2, out.size()-cut2);
			frame.Line = std::atoi(line);

			emplace_back(std::move(frame));
		}
	}

#endif

	MString StackFrame::ToString() const asd_noexcept
	{
		return MString::Format("{}@{} ({}:{})", Module, Function, File, Line);
	}

	MString StackTrace::ToString(IN const ToStrOpt& a_opt /*= ToStrOpt()*/) const asd_noexcept
	{
		MString indent;
		indent.resize(a_opt.Indent);
		std::memset(indent.data(), ' ', a_opt.Indent);

		MString ret;
		if (a_opt.NewlineHead)
			ret << '\n';

		bool flag = false;
		for (auto& stack : *this) {
			if (!flag) {
				flag = true;
				if (a_opt.IgnoreFirstIndent) {
					ret << stack.ToString() << '\n';
					continue;
				}
			}
			ret << indent << stack.ToString() << '\n';
		}

		if (flag && !a_opt.NewlineTail)
			ret.resize(ret.size() - 1);
		else if (!flag && a_opt.NewlineTail)
			ret << '\n';

		return ret;
	}



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

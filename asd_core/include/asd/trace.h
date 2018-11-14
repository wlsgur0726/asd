#pragma once
#include "asdbase.h"
#include "string.h"
#include <deque>
#include <chrono>


namespace asd
{
	struct StackFrame
	{
		MString Module;
		MString Function;
		MString	File;
		int		Line = 0;

		MString ToString() const;
	};


	class StackTrace : public std::deque<StackFrame>
	{
	public:
		StackTrace(uint32_t a_skip = 0,
				   uint32_t a_count = 10);

		struct ToStrOpt
		{
			ToStrOpt() {}
			uint32_t	Indent = 4;
			bool		IgnoreFirstIndent = false;
			bool		NewlineHead = false;
			bool		NewlineTail = false;
		};
		MString ToString(const ToStrOpt& a_opt = ToStrOpt()) const;
	};



	struct Trace
	{
		using TimePoint = std::chrono::system_clock::time_point;

		TimePoint		Time;
		uint32_t		TID;
		const char*		File;
		int				Line;
		const char*		Function;

		Trace(const char* a_file,
			  int a_line,
			  const char* a_function);

		virtual MString ToString() const;
	};


	struct Tracer : public std::deque<Trace>
	{
		typedef std::deque<Trace> Base;
		using Base::Base;

		size_t m_limit;

		inline Tracer(size_t a_limit = std::numeric_limits<size_t>::max())
			: m_limit(a_limit)
		{
		}
	};


	void PushTrace(Tracer& a_tracer,
				   const Trace& a_trace);
#define asd_Trace\
	asd::Trace(__FILE__, __LINE__, __FUNCTION__)

#define asd_PushTrace(Tracer, Lock)				\
	do {										\
		Lock.lock();							\
		asd::PushTrace(Tracer, asd_Trace);		\
		Lock.unlock();							\
	} while (false);							\



	struct DebugInfo : public Trace
	{
		const MString Comment;

		template<typename... ARGS>
		inline DebugInfo(const char* a_file,
						 const int a_line,
						 const char* a_function,
						 const char* a_comment = "",
						 const ARGS&... a_args)
			: Trace(a_file, a_line, a_function)
			, Comment(MString::Format(a_comment, a_args...))
		{
		}

		virtual MString ToString() const override;
	};

	using DebugTracer = std::deque<DebugInfo>;

	void PushDebugTrace(DebugTracer& a_tracer,
						DebugInfo&& a_trace);

	// __VA_ARGS__ : format, ...
#define asd_DebugInfo(...)\
	asd::DebugInfo(__FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

#define asd_DebugTrace(TRACE, ...)\
	asd::PushDebugTrace(TRACE, asd_DebugInfo(__VA_ARGS__))

#define asd_PrintStdErr(...)\
	asd::fputs(asd_DebugInfo(__VA_ARGS__).ToString(), stderr)
}


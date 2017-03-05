#pragma once
#include "asdbase.h"
#include "string.h"
#include "lock.h"
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

		MString ToString() const asd_noexcept;
	};


	class StackTrace : public std::deque<StackFrame>
	{
	public:
		StackTrace(IN uint32_t a_skip = 0,
				   IN uint32_t a_count = 10) asd_noexcept;
		MString ToString(IN uint32_t a_indent = 4) const asd_noexcept;
	};


	struct Trace
	{
		std::chrono::system_clock::time_point	Time;
		uint32_t								TID;
		const char*								File;
		int										Line;
		const char*								Function;

		Trace(IN const char* a_file,
			  IN int a_line,
			  IN const char* a_function) asd_noexcept;

		MString ToString() const asd_noexcept;
	};


	template <typename MutexType = asd::NoLock>
	struct Tracer : public std::deque<Trace>
	{
		typedef std::deque<Trace> Base;
		using Base::Base;

		mutable MutexType m_lock;
		size_t m_limit;

		inline Tracer(IN size_t a_limit = std::numeric_limits<size_t>::max())
			: m_limit(a_limit)
		{
		}
	};


	template <typename MutexType>
	inline void PushTrace(REF Tracer<MutexType>& a_tracer,
						  IN const Trace& a_trace) asd_noexcept
	{
		auto lock = asd::GetLock(a_tracer.m_lock);
		if (a_tracer.size() >= a_tracer.m_limit)
			a_tracer.pop_front();
		a_tracer.emplace_back(a_trace);
	}
}

#define asd_Trace				asd::Trace(__FILE__, __LINE__, __FUNCTION__)
#define asd_PushTrace(Tracer)	asd::PushTrace(Tracer, asd_Trace)

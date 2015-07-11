#pragma once
#include "../../asd/include/asdbase.h"
#include <functional>

namespace asd
{
	struct FinallyWork
	{
		std::function<void()> m_work = nullptr;
		FinallyWork(IN std::function<void()> a_work) asd_NoThrow
		{
			m_work = a_work;
		}

		~FinallyWork() asd_NoThrow 
		{
			if (m_work != nullptr)
				m_work();
		}
	};
}
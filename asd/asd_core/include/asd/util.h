#pragma once
#include "asd/asdbase.h"
#include <functional>

namespace asd
{
	struct FinallyWork
	{
		std::function<void()> m_work = nullptr;
		FinallyWork(IN std::function<void()> a_work) noexcept
		{
			m_work = a_work;
		}

		~FinallyWork() noexcept 
		{
			if (m_work != nullptr)
				m_work();
		}
	};
}
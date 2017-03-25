#include "asd_pch.h"
#include "asd/task.h"

namespace asd
{
	Task::Task() asd_noexcept
	{
		m_cancel = false;
	}

	Task::~Task() asd_noexcept
	{
		Cancel(false);
	}

	bool Task::Cancel(IN bool a_call /*= false*/) asd_noexcept
	{
		asd_BeginTry();
		bool exp = false;
		if (m_cancel.compare_exchange_strong(exp, true)) {
			if (a_call)
				OnExecute();
			return true;
		}
		asd_EndTryUnknown_Default();
		return false;
	}

	void Task::Execute() asd_noexcept
	{
		Cancel(true);
	}
}

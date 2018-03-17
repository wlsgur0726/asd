#include "asd_pch.h"
#include "asd/task.h"

namespace asd
{
	Task::Task()
	{
		m_cancel = false;
	}

	Task::~Task()
	{
		Cancel(false);
	}

	bool Task::Cancel(IN bool a_call /*= false*/)
	{
		bool exp = false;
		if (m_cancel.compare_exchange_strong(exp, true)) {
			if (a_call)
				OnExecute();
			return true;
		}
		return false;
	}

	void Task::Execute()
	{
		Cancel(true);
	}
}

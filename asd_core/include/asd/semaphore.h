#pragma once
#include "asdbase.h"
#include "exception.h"

namespace asd
{
	struct SemaphoreData;
	class Semaphore final
	{
		std::unique_ptr<SemaphoreData> m_data;
		Semaphore(const Semaphore&) = delete;
		Semaphore& operator = (const Semaphore&) = delete;

	public:
		const static uint32_t Infinite = 0xFFFFFFFF;

		Semaphore(uint32_t a_initCount = 0);

		Semaphore(Semaphore&& a_rval);

		Semaphore& operator = (Semaphore&& a_rval);

		~Semaphore();

		uint32_t GetCount() const;

		bool Wait(uint32_t a_timeoutMs = Infinite);

		void Post(uint32_t a_count = 1);

	};
}

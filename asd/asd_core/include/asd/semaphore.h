#pragma once
#include "asd/asdbase.h"
#include "asd/exception.h"

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

		Semaphore(IN uint32_t a_initCount = 0);

		Semaphore(MOVE Semaphore&& a_rval);

		Semaphore& operator = (MOVE Semaphore&& a_rval);

		~Semaphore() noexcept;

		uint32_t GetCount() const;

		bool Wait(IN uint32_t a_timeoutMs = Infinite);

		void Post(IN uint32_t a_count = 1);
	};
}

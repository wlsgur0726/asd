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

		Semaphore(IN uint32_t a_initCount = 0)
			noexcept(false);

		Semaphore(MOVE Semaphore&& a_rval)
			noexcept(false);

		Semaphore& operator = (MOVE Semaphore&& a_rval)
			noexcept(false);

		~Semaphore()
			noexcept;

		uint32_t GetCount() const 
			noexcept(false);

		bool Wait(IN uint32_t a_timeoutMs = Infinite) 
			noexcept(false);

		void Post(IN uint32_t a_count = 1) 
			noexcept(false);
	};
}

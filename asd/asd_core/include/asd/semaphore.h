#pragma once
#include "asd/asdbase.h"
#include "asd/exception.h"

namespace asd
{
	struct SemaphoreData;
	class Semaphore final
	{
		SemaphoreData* m_data = nullptr;
		Semaphore(const Semaphore&) = delete;
		Semaphore& operator = (const Semaphore&) = delete;

	public:
		const static uint32_t Infinite = 0xFFFFFFFF;

		Semaphore(IN uint32_t a_initCount = 0)
			asd_Throws(asd::Exception);

		Semaphore(MOVE Semaphore&& a_rval)
			asd_Throws(asd::Exception);

		Semaphore& operator = (MOVE Semaphore&& a_rval)
			asd_Throws(asd::Exception);

		~Semaphore()
			asd_NoThrow;

		uint32_t GetCount() const 
			asd_Throws(asd::Exception);

		bool Wait(IN uint32_t a_timeoutMs = Infinite) 
			asd_Throws(asd::Exception);

		void Post(IN uint32_t a_count = 1) 
			asd_Throws(asd::Exception);
	};
}

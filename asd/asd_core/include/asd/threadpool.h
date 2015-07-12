#pragma once

#include <stdint.h>
#include <functional>

namespace asd
{
	class ThreadPool final
	{
	public:
		typedef void* Tag;
		typedef std::function<void(void*)> Routine;

	private:
		struct ThreadPoolData;
		
		ThreadPoolData* m_pool = nullptr;

	public:
		ThreadPool();
		~ThreadPool();
		void AddThread();
	};
}
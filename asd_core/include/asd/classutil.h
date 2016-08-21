#pragma once
#include "asdbase.h"
#include "exception.h"
#include <memory>
#include <atomic>
#include <cassert>


namespace asd
{
	template <typename T>
	class Global
	{
	public:
		static T& GlobalInstance()
		{
			enum class InitState : int
			{
				Need = 0,
				Progress,
				Complete,
			};
			static std::atomic<InitState> g_initState(InitState::Need);
			static std::unique_ptr<T> g_globalObject = nullptr;

			do {
				const InitState state = g_initState.load(std::memory_order_relaxed);
				switch (state) {
					case InitState::Need: {
						InitState cmp = InitState::Need;
						if (false == g_initState.compare_exchange_strong(cmp, InitState::Progress))
							continue;

						assert(g_globalObject == nullptr);
						g_globalObject.reset(new T);

						cmp = InitState::Progress;
						if (false == g_initState.compare_exchange_strong(cmp, InitState::Complete))
							asd_RaiseException("fail init, {}", __FUNCTION__);
					}
					case InitState::Complete: {
						return *g_globalObject;
					}
					case InitState::Progress: {
						continue;
					}
					default: {
						asd_RaiseException("invalid InitState : {}, {}",
										   (int)state,
										   __FUNCTION__);
					}
				}
			} while (true);
			assert(false);
		}
	};



	template <typename T>
	class ThreadLocal
	{
	public:
		static T& ThreadLocalInstance()
		{
			thread_local std::unique_ptr<T> t_threadLocalObject = nullptr;
			if (t_threadLocalObject == nullptr)
				t_threadLocalObject.reset(new T);
			return *t_threadLocalObject;
		}
	};



	template <typename T, typename... Args>
	inline void Reset(REF T& a_target,
					  IN const Args&... a_constructorArgs)
	{
		a_target.~T();
		new(&a_target) T(a_constructorArgs...);
	}
}

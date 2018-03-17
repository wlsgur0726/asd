#pragma once
#include "asdbase.h"
#include "objpool.h"


namespace asd
{
	class Task
	{
	public:
		Task();
		virtual ~Task();

		// 큐잉한 task를 취소
		// task가 아직 실행되지 않았다면 true 리턴
		// a_call에 true를 넘기면 task가 아직 실행되지 않은 경우 실행
		bool Cancel(IN bool a_call = false);

		// 실행 (1회만 실행하는 것을 보장)
		void Execute();

	private:
		virtual void OnExecute() = 0;
		std::atomic_bool m_cancel;
	};


	template <typename FUNC, typename... PARAMS>
	class TaskTemplate : public Task
	{
	public:
		TaskTemplate(FUNC&& a_func,
					 PARAMS&&... a_params)
			: m_func(std::forward<FUNC>(a_func))
			, m_params(std::forward<PARAMS>(a_params)...)
		{
		}

	protected:
		using Func = typename std::remove_reference<FUNC>::type;
		using Params = std::tuple<typename std::remove_reference<PARAMS>::type...>;
		Func m_func;
		Params m_params;

		template<size_t... Remains>
		struct seq {};

		template <size_t N, size_t... Remains>
		struct gen_seq : gen_seq<N-1, N-1, Remains...> {};

		template <size_t... Remains>
		struct gen_seq<0, Remains...> : seq <Remains...> {};

		template <size_t... Is>
		inline void Call(IN seq<Is...>)
		{
			m_func(std::get<Is>(m_params)...);
		}

		virtual void OnExecute() override
		{
			Call(gen_seq<sizeof...(PARAMS)>());
		}
	};


	template <typename FUNC>
	class TaskTemplate<FUNC> : public Task
	{
	public:
		TaskTemplate(FUNC&& a_func)
			: m_func(std::forward<FUNC>(a_func))
		{
		}

	protected:
		using Func = typename std::remove_reference<FUNC>::type;
		Func m_func;

		virtual void OnExecute() override
		{
			m_func();
		}
	};


	using Task_ptr = std::shared_ptr<Task>;


	template <typename FUNC, typename... PARAMS>
	static Task_ptr CreateTask(FUNC&& a_func,
							   PARAMS&&... a_params)
	{
		using TASK = TaskTemplate<FUNC, PARAMS...>;
#if 0
		return Task_ptr(new TASK(std::forward<FUNC>(a_func),
								 std::forward<PARAMS>(a_params)...));
#else
		using POOL = ObjectPoolShardSet< ObjectPool<TASK, Mutex> >;
		static auto& s_pool = Global<POOL>::Instance();
		auto task = s_pool.Alloc(std::forward<FUNC>(a_func),
								 std::forward<PARAMS>(a_params)...);
		return Task_ptr(task, [](Task* p) { s_pool.Free((TASK*)p); });
#endif
	}
}

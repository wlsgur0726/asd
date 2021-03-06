﻿#pragma once
#include "asdbase.h"
#include "threadpool.h"
#include "container.h"
#include "classutil.h"
#include <unordered_set>


namespace asd
{
	template <typename DATA>
	class Actx
	{
	public:
		using Data = DATA;
		using ThisType = Actx<Data>;
		class Context;
		using Ctx = std::shared_ptr<Context>;
		using Task = std::function<void(Ctx)>;
		using WhileCondition = std::function<bool(Ctx)>;

		class Context : public std::enable_shared_from_this<Context>
		{
			using TopRef = Global<ShardedHashMap<uintptr_t, Context>>;

		public:
			friend class Actx<DATA>;

			Context(const Trace& a_start,
					std::shared_ptr<Data>& a_data,
					std::shared_ptr<Mutex>& a_ownerLock)
				: data(a_data)
				, m_lock(a_ownerLock)
				, m_dbg(a_start)
			{
				if (m_lock == nullptr)
					asd_OnErr("unknown error");
			}

			Context(Context* a_owner)
				: m_owner(a_owner)
				, data(a_owner->data)
				, m_lock(a_owner->m_lock)
				, m_dbg(a_owner->m_dbg.startPoint)
			{
				if (m_owner == nullptr) {
					asd_OnErr("unknown error");
					return;
				}
				m_dbg.loopDepth = m_owner->m_dbg.loopDepth + 1;
			}

			virtual ~Context()
			{
				Clear();
			}

			std::shared_ptr<Data> data;

			nullptr_t Next()
			{
				auto lock = GetLock(*m_lock);
				if (m_fin)
					return nullptr;
				Invoke();
				return nullptr;
			}
			template <typename ThreadPool>
			inline auto Next(ThreadPool& a_threadPool)
			{
				return a_threadPool.Push([ctx=shared_from_this()]()
				{
					ctx->Next();
				});
			}
			template <typename ThreadPool, typename SeqKey>
			inline auto Next(ThreadPool& a_threadPool,
							 SeqKey&& a_seqKey)
			{
				return a_threadPool.PushSeq(std::forward<SeqKey>(a_seqKey), [ctx=shared_from_this()]()
				{
					ctx->Next();
				});
			}

			nullptr_t Finish()
			{
				auto lock = GetLock(*m_lock);
				Finish_Internal();
				return Next();
			}
			template <typename ThreadPool>
			inline auto Finish(ThreadPool& a_threadPool)
			{
				return a_threadPool.Push([ctx=shared_from_this()]()
				{
					ctx->Finish();
				});
			}
			template <typename ThreadPool, typename SeqKey>
			inline auto Finish(ThreadPool& a_threadPool,
							   SeqKey&& a_seqKey)
			{
				return a_threadPool.Push(std::forward<SeqKey>(a_seqKey), [ctx=shared_from_this()]()
				{
					ctx->Finish();
				});
			}

			nullptr_t Continue()
			{
				return Finish();
			}
			template <typename ThreadPool>
			inline auto Continue(ThreadPool& a_threadPool)
			{
				return a_threadPool.Push([ctx=shared_from_this()]()
				{
					ctx->Continue();
				});
			}
			template <typename ThreadPool, typename SeqKey>
			inline auto Continue(ThreadPool& a_threadPool,
								 SeqKey&& a_seqKey)
			{
				return a_threadPool.PushSeq(std::forward<SeqKey>(a_seqKey), [ctx=shared_from_this()]()
				{
					ctx->Continue();
				});
			}

			nullptr_t Break()
			{
				auto lock = GetLock(*m_lock);
				Break_Internal();
				return Finish();
			}
			template <typename ThreadPool>
			inline auto Break(ThreadPool& a_threadPool)
			{
				return a_threadPool.Push([ctx=shared_from_this()]()
				{
					ctx->Break();
				});
			}
			template <typename ThreadPool, typename SeqKey>
			inline auto Break(ThreadPool& a_threadPool,
							  SeqKey&& a_seqKey)
			{
				return a_threadPool.Push(std::forward<SeqKey>(a_seqKey), [ctx=shared_from_this()]()
				{
					ctx->Break();
				});
			}

		private:
			inline void Invoke()
			{
				if (m_fin)
					return;

				auto ctx = shared_from_this();
				m_fin = m_cur >= m_tasks.size();
				if (!m_fin) {
					if (m_owner) {
						Task& task = m_tasks[m_cur++];
						asd_DAssert(task);
						if (task)
							task(ctx);
					}
					else {
						Task task = std::move(m_tasks.front());
						m_tasks.pop_front();
						asd_DAssert(task);
						if (task)
							task(ctx);
					}
				}
				else {
					if (m_finally)
						m_finally(ctx);

					if (m_owner && m_fin) {
						if (m_continue) {
							Reset();
							Next();
						}
						else {
							auto owner = m_owner;
							if (owner->m_owner == nullptr) {
								Clear();
								owner->m_loops.erase(ctx);
							}
							owner->Next();
						}
						return;
					}
					if (!m_owner) {
						Clear();
						TopRef::Instance().Erase((uintptr_t)this);
					}
				}
			}

			inline void Finish_Internal()
			{
				if (m_owner)
					m_cur = m_tasks.size();
				else
					m_tasks.clear();
			}

			inline void Break_Internal()
			{
				asd_DAssert(m_owner);
				m_continue = false;
			}

			void Reset()
			{
				asd_DAssert(m_owner);
				m_fin = false;
				m_cur = 0;
				m_continue = true;
			}

			void Clear()
			{
				m_fin = true;
				while (m_loops.size() > 0) {
					auto it = m_loops.begin();
					(*it)->Clear();
					m_loops.erase(it);
				}
				m_tasks.clear();
				m_finally = nullptr;
			}

			mutable std::shared_ptr<Mutex> m_lock;
			Context* m_owner = nullptr;
			std::unordered_set<Ctx> m_loops;
			bool m_fin = false;
			size_t m_cur = 0;
			std::deque<Task> m_tasks;
			Task m_finally;
			bool m_continue = true;

			struct Dbg
			{
				Trace startPoint;
				int loopDepth = 0;
				Dbg(const Trace& a_start)
					: startPoint(a_start)
				{
				}
			} m_dbg;
		};


		Actx(const Trace& a_start,
			 std::shared_ptr<Data> a_data,
			 std::shared_ptr<Mutex> a_ownerLock)
			: m_lock(std::move(a_ownerLock))
		{
			if (m_lock == nullptr)
				m_lock.reset(new Mutex);
			m_lock->lock();
			m_ctx.reset(new Context(a_start, a_data, m_lock));
		}

		Actx(ThisType* a_owner)
		{
			m_owner = a_owner;
			InitLoop();
		}

		Actx(const ThisType&) = delete;
		ThisType& operator=(const ThisType&) = delete;

		Actx(ThisType&& a_actx) = default;
		ThisType& operator=(ThisType&& a_actx) = default;

		ThisType& Then(Task&& a_task)
		{
			if (m_ctx == nullptr) {
				asd_OnErr("already started");
				return *this;
			}
			if (a_task == nullptr) {
				asd_OnErr("invalid task");
				return *this;
			}
			m_ctx->m_tasks.emplace_back(std::forward<Task>(a_task));
			return *this;
		}

		ThisType& Loop()
		{
			if (m_ctx == nullptr) {
				asd_OnErr("already started");
				return *this;
			}
			if (m_loop && m_loop->m_ctx) {
				asd_OnErr("you maybe forgot to call EndLoop() at sub loop");
				return *this;
			}
			if (m_loop)
				m_loop->InitLoop();
			else
				m_loop.reset(new ThisType(this));
			return *m_loop;
		}

		ThisType& While(WhileCondition&& a_cond)
		{
			if (m_ctx == nullptr) {
				asd_OnErr("already started");
				return *this;
			}
			if (a_cond == nullptr) {
				asd_OnErr("invalid task");
				return *this;
			}
			return Loop().Then([this, cond=std::forward<WhileCondition>(a_cond)](Ctx ctx)
			{
				if (cond(ctx)) {
					ctx->Next();
					return;
				}

				auto ownerCtx = ctx->m_owner;
				if (ownerCtx->m_owner == nullptr) {
					// is top level ctx
					ctx->Clear();
					ownerCtx->m_loops.erase(ctx);
				}
				ownerCtx->Next();
			});
		}

		ThisType& EndLoop()
		{
			if (m_ctx == nullptr) {
				asd_OnErr("already started");
				return *this;
			}
			if (m_owner == nullptr) {
				asd_OnErr("is not Loop");
				return *this;
			}
			if (m_loop && m_loop->m_ctx) {
				asd_OnErr("you maybe forgot to call EndLoop() at sub loop");
				return *this;
			}

			return m_owner->Then([ctx=std::move(m_ctx)](Ctx)
			{
				ctx->Reset();
				ctx->Next();
			});
		}

		ThisType& Finally(Task&& a_task)
		{
			if (m_ctx == nullptr) {
				asd_OnErr("already started");
				return *this;
			}
			asd_DAssert(m_ctx->m_finally == nullptr);
			m_ctx->m_finally = std::forward<Task>(a_task);
			return *this;
		}

		template <typename... ThreadPoolArgs>
		auto Run(ThreadPoolArgs&&... a_args) -> decltype(Ctx()->Next(std::forward<ThreadPoolArgs>(a_args)...))
		{
			if (m_ctx == nullptr) {
				asd_OnErr("already started");
				return nullptr;
			}
			if (m_loop && m_loop->m_ctx) {
				asd_OnErr("you maybe forgot to call EndLoop() at sub loop");
				return nullptr;
			}
			auto ctx = std::move(m_ctx);
			auto lock = std::move(m_lock);
			Context::TopRef::Instance().Insert((uintptr_t)ctx.get(), ctx);
			auto ret = ctx->Next(std::forward<ThreadPoolArgs>(a_args)...);
			if (lock)
				lock->unlock();
			return ret;
		}

		virtual ~Actx()
		{
			asd_DAssert(m_loop==nullptr || m_loop->m_ctx==nullptr);
			if (m_ctx)
				Run();
			if (m_lock)
				m_lock->unlock();
		}

		Ctx CtxT() const
		{
			return Ctx();
		}

	private:
		void InitLoop()
		{
			m_ctx.reset(new Context(m_owner->m_ctx.get()));
			m_owner->m_ctx->m_loops.emplace(m_ctx);
		}

		std::shared_ptr<Mutex> m_lock;
		Ctx m_ctx;
		ThisType* m_owner = nullptr;
		std::shared_ptr<ThisType> m_loop;
	};


	template <typename Data>
	Actx<Data> CreateActx(const Trace& a_start,
						  std::shared_ptr<Data> a_data)
	{
		return Actx<Data>(a_start, a_data, nullptr);
	}


	Actx<nullptr_t> CreateActx(const Trace& a_start)
	{
		return Actx<nullptr_t>(a_start, nullptr, nullptr);
	}
}

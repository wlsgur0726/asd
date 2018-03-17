#pragma once
#include "asdbase.h"
#include "threadpool.h"
#include "container.h"
#include "classutil.h"

namespace asd
{
	template <typename THREAD_POOL, typename DATA>
	class Actx
	{
	public:
		using ThreadPool = THREAD_POOL;
		using SeqKey = typename ThreadPool::SeqKey;
		using Data = DATA;
		using ThisType = Actx<ThreadPool, Data>;
		using ContextOwner = ThisType;
		class Context;
		using Ctx = std::shared_ptr<Context>;
		using Task = std::function<void(Ctx)>;
		using WhileCondition = std::function<bool(Ctx)>;

		class Context : public std::enable_shared_from_this<Context>
		{
			using TopRef = Global<ShardedHashMap<uintptr_t, Context>>;

		public:
			friend class ContextOwner;

			Context(IN const Trace& a_start,
					REF ThreadPool& a_threadPool,
					REF std::shared_ptr<Data>& a_data,
					REF std::shared_ptr<Mutex>& a_ownerLock)
				: threadPool(a_threadPool)
				, data(a_data)
				, m_lock(a_ownerLock)
				, m_dbg(a_start)
			{
				asd_ChkErrAndRet(m_lock == nullptr);
			}

			Context(REF Context* a_owner)
				: m_owner(a_owner)
				, threadPool(a_owner->threadPool)
				, data(a_owner->data)
				, m_lock(a_owner->m_lock)
				, m_useDefaultSeqKey(a_owner->m_useDefaultSeqKey)
				, m_dbg(a_owner->m_dbg.startPoint)
			{
				asd_ChkErrAndRet(m_owner == nullptr);
				if (m_useDefaultSeqKey)
					m_defaultSeqKey = m_owner->m_defaultSeqKey;
				m_dbg.loopDepth = m_owner->m_dbg.loopDepth + 1;
			}

			virtual ~Context()
			{
				Clear();
			}

			ThreadPool& threadPool;
			std::shared_ptr<Data> data;

			void Next()
			{
				auto lock = GetLock(*m_lock);
				if (m_fin)
					return;

				if (m_useDefaultSeqKey)
					threadPool.PushSeq(m_defaultSeqKey, InvokeFunc());
				else
					threadPool.Push(InvokeFunc());
			}

			void Next(SeqKey&& a_seqKey)
			{
				auto lock = GetLock(*m_lock);
				if (m_fin)
					return;

				threadPool.PushSeq(std::forward<SeqKey>(a_seqKey),
								   InvokeFunc());
			}

			void Finish()
			{
				auto lock = GetLock(*m_lock);
				Finish_Internal();
				Next();
			}

			void Finish(SeqKey&& a_seqKey)
			{
				auto lock = GetLock(*m_lock);
				Finish_Internal();
				Next(std::forward<SeqKey>(a_seqKey));
			}

			void Continue()
			{
				Finish();
			}

			void Continue(SeqKey&& a_seqKey)
			{
				Finish(std::forward<SeqKey>(a_seqKey));
			}

			void Break()
			{
				auto lock = GetLock(*m_lock);
				Break_Internal();
				Finish();
			}

			void Break(SeqKey&& a_seqKey)
			{
				auto lock = GetLock(*m_lock);
				Break_Internal();
				Finish(std::forward<SeqKey>(a_seqKey));
			}

		private:
			inline auto InvokeFunc()
			{
				asd_DAssert(++m_dbg.callNext == 1);
				return [this, ctx=shared_from_this()]() mutable
				{
					auto lock = GetLock(*m_lock);

					asd_DAssert(--m_dbg.callNext == 0);
					if (m_fin)
						return;

					m_fin = m_cur >= m_tasks.size();
					if (!m_fin) {
						Task& task = m_tasks[m_cur];
						asd_DAssert(task);
						if (task)
							task(ctx);

						if (m_owner)
							++m_cur;
						else
							m_tasks.pop_front();
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
				};
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
			bool m_useDefaultSeqKey = false;
			SeqKey m_defaultSeqKey;

			struct Dbg
			{
				Trace startPoint;
				int loopDepth = 0;
				int callNext = 0;
				Dbg(IN const Trace& a_start)
					: startPoint(a_start)
				{
				}
			} m_dbg;
		};


		Actx(IN const Trace& a_start,
			 REF ThreadPool& a_threadPool,
			 REF std::shared_ptr<Data> a_data,
			 REF std::shared_ptr<Mutex> a_ownerLock)
			: m_lock(std::move(a_ownerLock))
		{
			if (m_lock == nullptr)
				m_lock.reset(new Mutex);
			m_lock->lock();
			m_ctx.reset(new Context(a_start, a_threadPool, a_data, m_lock));
		}

		Actx(REF ThisType* a_owner)
		{
			m_owner = a_owner;
			InitLoop();
		}

		Actx(IN const ThisType&) = delete;
		ThisType& operator=(IN const ThisType&) = delete;

		Actx(MOVE ThisType&& a_actx) = default;
		ThisType& operator=(MOVE ThisType&& a_actx) = default;

		ThisType& EnableDefaultSeqKey(SeqKey&& a_key)
		{
			asd_ChkErrAndRetVal(m_ctx == nullptr, *this);
			m_ctx->m_useDefaultSeqKey = true;
			m_ctx->m_defaultSeqKey = std::forward<SeqKey>(a_key);
		}

		ThisType& Then(Task&& a_task)
		{
			asd_ChkErrAndRetVal(m_ctx == nullptr, *this);
			asd_ChkErrAndRetVal(a_task == nullptr, *this);
			m_ctx->m_tasks.emplace_back(std::forward<Task>(a_task));
			return *this;
		}

		ThisType& Loop()
		{
			asd_ChkErrAndRetVal(m_ctx == nullptr, *this);
			asd_ChkErrAndRetVal(m_loop && m_loop->m_ctx, *this);
			if (m_loop)
				m_loop->InitLoop();
			else
				m_loop.reset(new ThisType(this));
			return *m_loop;
		}

		ThisType& While(WhileCondition&& a_cond)
		{
			asd_ChkErrAndRetVal(m_ctx == nullptr, *this);
			asd_ChkErrAndRetVal(a_cond == nullptr, *this);
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
			asd_ChkErrAndRetVal(m_ctx == nullptr, *this);
			asd_ChkErrAndRetVal(m_owner == nullptr, *this);
			asd_ChkErrAndRetVal(m_loop && m_loop->m_ctx, *this);

			return m_owner->Then([ctx=std::move(m_ctx)](Ctx)
			{
				ctx->Reset();
				ctx->Next();
			});
		}

		ThisType& Finally(Task&& a_task)
		{
			asd_ChkErrAndRetVal(m_ctx == nullptr, *this);
			asd_DAssert(m_ctx->m_finally == nullptr);
			m_ctx->m_finally = std::forward<Task>(a_task);
			return *this;
		}

		void Run()
		{
			asd_ChkErrAndRet(m_ctx == nullptr);
			asd_ChkErrAndRet(m_loop && m_loop->m_ctx);
			auto ctx = std::move(m_ctx);
			auto lock = std::move(m_lock);
			Context::TopRef::Instance().Insert((uintptr_t)ctx.get(), ctx);
			ctx->Next();
			if (lock)
				lock->unlock();
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


	template <typename ThreadPool, typename Data>
	Actx<ThreadPool, Data> CreateActx(IN const Trace& a_start,
									  REF ThreadPool& a_threadPool,
									  REF std::shared_ptr<Data> a_data)
	{
		return Actx<ThreadPool, Data>(a_start, a_threadPool, a_data, nullptr);
	}


	template <typename ThreadPool>
	Actx<ThreadPool, nullptr_t> CreateActx(IN const Trace& a_start,
										   REF ThreadPool& a_threadPool)
	{
		return Actx<ThreadPool, nullptr_t>(a_start, a_threadPool, nullptr, nullptr);
	}
}

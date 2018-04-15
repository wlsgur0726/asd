#include "stdafx.h"
#include "asd/string.h"
#include "asd/actx.h"


namespace asdtest_actx
{
	struct TestData
	{
		int before = 0;
		asd::Semaphore event;

		void PrintLine(int line)
		{
			auto tid = asd::GetCurrentThreadID();
			::printf("  tid:%u, %d -> %d\n", tid, before, line);
			before = line;
		}
		~TestData()
		{
			::printf("delete data\n");
		}
	};


	TEST(Actx, Simple)
	{
		asd::ThreadPoolOption option;
		asd::ThreadPool<int> tp(option);
		tp.Start();

		auto data = std::make_shared<TestData>();
		auto actx = asd::CreateActx(asd_Trace, data);
		using Ctx = decltype(actx.CtxT());
		actx
		.Then([&](Ctx ctx){
			ctx->data->PrintLine(__LINE__);
			ctx->Next(tp);
		})
		.Then([&](Ctx ctx){
			ctx->data->PrintLine(__LINE__);
			ctx->Finish(tp);
		})
		.Then([&](Ctx ctx){
			ctx->data->PrintLine(__LINE__);
			FAIL();
			ctx->Next(tp);
		})
		.Finally([](Ctx ctx){
			ctx->data->PrintLine(__LINE__);
			ctx->data->event.Post();
		});

		data->before = __LINE__;
		actx.Run(tp);

		data->event.Wait();
		data.reset();
		::printf("end\n");
	}


	TEST(Actx, While)
	{
		asd::ThreadPoolOption option;
		asd::ThreadPool<int> tp(option);
		tp.Start();

		struct WhileTestData : public TestData
		{
			int loopA = 0;
			int loopB = 0;
			int loopC = 0;
		};

		auto data = std::make_shared<WhileTestData>();
		auto actx = asd::CreateActx(asd_Trace, data);
		using Ctx = decltype(actx.CtxT());
		actx
		.Then([&](Ctx ctx){
			ctx->data->PrintLine(__LINE__);
			::printf("normal test\n");
			ctx->Next(tp);
		})
		.While([](Ctx ctx){
			ctx->data->PrintLine(__LINE__);
			int loop = ++ctx->data->loopA;
			::printf(" loopA %d\n", loop);
			return loop <= 2;
		})
			.Then([&](Ctx ctx){
				ctx->data->PrintLine(__LINE__);
				ctx->Next(tp);
			})
			.Then([&](Ctx ctx){
				ctx->data->PrintLine(__LINE__);
				ctx->Next(tp);
			})
			.Finally([](Ctx ctx){
				ctx->data->PrintLine(__LINE__);
			})
		.EndLoop()

		.Then([&](Ctx ctx){
			ctx->data->PrintLine(__LINE__);
			EXPECT_EQ(ctx->data->loopA, 3);
			::printf("Break test\n");
			ctx->Next(tp);
		})
		.While([&](Ctx ctx){
			ctx->data->PrintLine(__LINE__);
			int loop = ++ctx->data->loopB;
			::printf("loopB %d\n", loop);
			return loop <= 100;
		})
			.Then([&](Ctx ctx){
				ctx->data->PrintLine(__LINE__);
				ctx->Break(tp);
			})
			.Then([&](Ctx ctx){
				ctx->data->PrintLine(__LINE__);
				FAIL();
				ctx->Next(tp);
			})
			.Finally([](Ctx ctx){
				ctx->data->PrintLine(__LINE__);
			})
		.EndLoop()

		.Then([&](Ctx ctx){
			ctx->data->PrintLine(__LINE__);
			EXPECT_EQ(ctx->data->loopB, 1);
			::printf("Continue test\n");
			ctx->Next(tp);
		})
		.While([](Ctx ctx){
			ctx->data->PrintLine(__LINE__);
			int loop = ++ctx->data->loopC;
			::printf("loopC %d\n", loop);
			return loop <= 2;
		})
			.Then([&](Ctx ctx){
				ctx->data->PrintLine(__LINE__);
				ctx->Continue(tp);
			})
			.Then([&](Ctx ctx){
				ctx->data->PrintLine(__LINE__);
				FAIL();
				ctx->Next(tp);
			})
			.Finally([](Ctx ctx){
				ctx->data->PrintLine(__LINE__);
			})
		.EndLoop()

		.Then([&](Ctx ctx){
			ctx->data->PrintLine(__LINE__);
			EXPECT_EQ(ctx->data->loopC, 3);
			ctx->Next(tp);
		})
		.Finally([](Ctx ctx){
			ctx->data->PrintLine(__LINE__);
			ctx->data->event.Post();
		});

		data->before = __LINE__;
		actx.Run(tp);

		data->event.Wait();
		data.reset();
		::printf("end\n");
	}


	TEST(Actx, NestedLoop)
	{
		asd::ThreadPoolOption option;
		asd::ThreadPool<int> tp(option);
		tp.Start();

		struct NestedLoopData : public TestData
		{
			int i = 0;
			int j = 0;
			int k = 0;
		};
		auto data = std::make_shared<NestedLoopData>();
		auto actx = asd::CreateActx(asd_Trace, data);
		using Ctx = decltype(actx.CtxT());
		actx
		.While([](Ctx ctx) { return ++ctx->data->i <= 3; })
			.While([](Ctx ctx) { return ++ctx->data->j <= 3; })
				.While([](Ctx ctx) { return ++ctx->data->k <= 3; })
					.Then([&](Ctx ctx){
						::printf("i:%d, j:%d, k:%d\n",
								 ctx->data->i,
								 ctx->data->j,
								 ctx->data->k);
						ctx->Next(tp);
					})
				.EndLoop()
				.Finally([](Ctx ctx) {
					EXPECT_EQ(ctx->data->k, 4);
					ctx->data->k = 0;
				})
			.EndLoop()	
			.Finally([](Ctx ctx) {
				EXPECT_EQ(ctx->data->j, 4);
				ctx->data->j = 0;
			})
		.EndLoop()	
		.Finally([](Ctx ctx){
			EXPECT_EQ(ctx->data->i, 4);
			ctx->data->event.Post();
		})
		.Run(tp);

		data->event.Wait();
		data.reset();
		::printf("end\n");
	}
}

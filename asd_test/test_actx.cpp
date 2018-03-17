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
			::printf("  %d -> %d\n", before, line);
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
		auto actx = asd::CreateActx(asd_Trace, tp, data);
		using Ctx = decltype(actx.CtxT());
		actx
		.Then([](Ctx ctx){
			ctx->data->PrintLine(__LINE__);
			std::thread([ctx](){ ctx->Next(); }).detach();
		})
		.Then([](Ctx ctx){
			ctx->data->PrintLine(__LINE__);
			std::thread([ctx](){ ctx->Finish(); }).detach();
		})
		.Then([](Ctx ctx){
			ctx->data->PrintLine(__LINE__);
			FAIL();
			std::thread([ctx](){ ctx->Next(); }).detach();
		})
		.Finally([](Ctx ctx){
			ctx->data->PrintLine(__LINE__);
			ctx->data->event.Post();
		});

		data->before = __LINE__;
		actx.Run();

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
		auto actx = asd::CreateActx(asd_Trace, tp, data);
		using Ctx = decltype(actx.CtxT());
		actx
		.Then([](Ctx ctx){
			ctx->data->PrintLine(__LINE__);

			::printf("normal test\n");
			std::thread([ctx](){ ctx->Next(); }).detach();
		})
		.While([](Ctx ctx){
			ctx->data->PrintLine(__LINE__);
			int loop = ++ctx->data->loopA;
			::printf(" loopA %d\n", loop);
			return loop <= 2;
		})
			.Then([](Ctx ctx){
				ctx->data->PrintLine(__LINE__);
				std::thread([ctx](){ ctx->Next(); }).detach();
			})
			.Then([](Ctx ctx){
				ctx->data->PrintLine(__LINE__);
				std::thread([ctx](){ ctx->Next(); }).detach();
			})
			.Finally([](Ctx ctx){
				ctx->data->PrintLine(__LINE__);
			})
		.EndLoop()

		.Then([](Ctx ctx){
			ctx->data->PrintLine(__LINE__);
			EXPECT_EQ(ctx->data->loopA, 3);
			::printf("Break test\n");
			std::thread([ctx](){ ctx->Next(); }).detach();
		})
		.While([](Ctx ctx){
			ctx->data->PrintLine(__LINE__);
			int loop = ++ctx->data->loopB;
			::printf("loopB %d\n", loop);
			return loop <= 100;
		})
			.Then([](Ctx ctx){
				ctx->data->PrintLine(__LINE__);
				std::thread([ctx](){ ctx->Break(); }).detach();
			})
			.Then([](Ctx ctx){
				ctx->data->PrintLine(__LINE__);
				FAIL();
				std::thread([ctx](){ ctx->Next(); }).detach();
			})
			.Finally([](Ctx ctx){
				ctx->data->PrintLine(__LINE__);
			})
		.EndLoop()

		.Then([](Ctx ctx){
			ctx->data->PrintLine(__LINE__);
			EXPECT_EQ(ctx->data->loopB, 1);

			::printf("Continue test\n");
			std::thread([ctx](){ ctx->Next(); }).detach();
		})
		.While([](Ctx ctx){
			ctx->data->PrintLine(__LINE__);
			int loop = ++ctx->data->loopC;
			::printf("loopC %d\n", loop);
			return loop <= 2;
		})
			.Then([](Ctx ctx){
				ctx->data->PrintLine(__LINE__);
				std::thread([ctx](){ ctx->Continue(); }).detach();
			})
			.Then([](Ctx ctx){
				ctx->data->PrintLine(__LINE__);
				FAIL();
				std::thread([ctx](){ ctx->Next(); }).detach();
			})
			.Finally([](Ctx ctx){
				ctx->data->PrintLine(__LINE__);
			})
		.EndLoop()

		.Then([](Ctx ctx){
			ctx->data->PrintLine(__LINE__);
			EXPECT_EQ(ctx->data->loopC, 3);
			std::thread([ctx](){ ctx->Next(); }).detach();
		})
		.Finally([](Ctx ctx){
			ctx->data->PrintLine(__LINE__);
			ctx->data->event.Post();
		});

		data->before = __LINE__;
		actx.Run();

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
		auto actx = asd::CreateActx(asd_Trace, tp, data);
		using Ctx = decltype(actx.CtxT());
		actx
		.While([](Ctx ctx) { return ++ctx->data->i <= 3; })
			.While([](Ctx ctx) { return ++ctx->data->j <= 3; })
				.While([](Ctx ctx) { return ++ctx->data->k <= 3; })
					.Then([](Ctx ctx){
						::printf("i:%d, j:%d, k:%d\n",
									ctx->data->i,
									ctx->data->j,
									ctx->data->k);
						std::thread([ctx](){ ctx->Next(); }).detach();
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
		.Run();

		data->event.Wait();
		data.reset();
		::printf("end\n");
	}
}

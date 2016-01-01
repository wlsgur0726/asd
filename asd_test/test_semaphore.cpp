#include "stdafx.h"
#include "asd/semaphore.h"
#include <thread>
#include <atomic>
#include <list>
#include <exception>


TEST(Semaphore, ConstructorTest1)
{
	asd::Semaphore sem;
	ASSERT_EQ(sem.GetCount(), 0);
	ASSERT_FALSE(sem.Wait(0));
}


TEST(Semaphore, ConstructorTest2)
{
	const int TestCount = 123;
	asd::Semaphore sem(TestCount);
	ASSERT_EQ(sem.GetCount(), TestCount);
	for (int i=0; i<TestCount; ++i) {
		ASSERT_TRUE(sem.Wait(0));
	}
	ASSERT_EQ(sem.GetCount(), 0);
}


TEST(Semaphore, PostTest) 
{
	const int TestCount = 123;
	asd::Semaphore sem;
	sem.Post(TestCount);
	ASSERT_EQ(sem.GetCount(), TestCount);
	for (int i=0; i<TestCount; ++i) {
		ASSERT_TRUE(sem.Wait(0));
	}
	ASSERT_EQ(sem.GetCount(), 0);
}


TEST(Semaphore, Example1)
{
	// 1ms에 핑퐁을 얼마나 주고받는가
	const int TestTimeMs = 100;
	asd::Semaphore semPing;
	asd::Semaphore semPong;
	uint64_t count = 0;

	volatile bool run = true;
	std::thread t1([&](){
		while (run) {
			if (semPing.Wait(100)) {
				semPong.Post();
			}
			else {
				EXPECT_FALSE(run);
			}
		}
	});
	std::thread t2([&](){
		while (run) {
			if (semPong.Wait(100)) {
				++count;
				semPing.Post();
			}
			else {
				EXPECT_FALSE(run);
			}
		}
	});

	// 측정 시작
	semPing.Post();
	auto start = std::chrono::high_resolution_clock::now();
	std::this_thread::sleep_for(std::chrono::milliseconds(TestTimeMs));

	// 측정 종료
	auto end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double, std::milli> elapsed = end - start;
	run = false;
	t1.join();
	t2.join();

	printf("%lf count per ms\n", count/elapsed.count());
}


TEST(Semaphore, Example2)
{
	// std::thread를 1ms에 몇개나 만들 수 있는가
	const int TestCount = 5;
	const int ThreadCount = 256;
	double sum_elapsed = 0;
	for (int i=0; i<TestCount; ++i) {
		asd::Semaphore created_event;
		asd::Semaphore close_event;

		std::thread threads[ThreadCount];

		// 시간측정 시작
		auto start = std::chrono::high_resolution_clock::now();
		for (auto& t : threads) {
			t = std::thread([&]() {
				created_event.Post();
				EXPECT_TRUE(close_event.Wait(1000));
			});
		}

		bool allSuccess = true;
		for (int i=0; i<ThreadCount; ++i)
			allSuccess = allSuccess && created_event.Wait(100);

		// 시간측정 종료
		auto end = std::chrono::high_resolution_clock::now();		
		std::chrono::duration<double, std::milli> elapsed = end - start;

		EXPECT_TRUE(allSuccess);
		EXPECT_EQ(created_event.GetCount(), 0);

		if (elapsed.count() > 0) {
			double result = ThreadCount / elapsed.count();
			printf("%3d : %lf count per ms\n", i+1, result);
			sum_elapsed += elapsed.count();
		}
		else if (elapsed.count() == 0) {
			printf("  very fast...\n");
		}
		EXPECT_TRUE(elapsed.count() >= 0);

		close_event.Post(ThreadCount);
		for (auto& t : threads)
			t.join();
		EXPECT_EQ(close_event.GetCount(), 0);
	}

	if (sum_elapsed > 0) {
		printf("average : %lf count per ms\n", (TestCount*ThreadCount)/sum_elapsed);
	}
	else if (sum_elapsed == 0) {
		printf("very fast!!!\n");
	}
	EXPECT_TRUE(sum_elapsed >= 0);
}


TEST(Semaphore, Example3)
{
	// 세마포어로 간단한 뮤텍스와 이벤트큐를 구현해본 테스트
	struct Mutex {
		asd::Semaphore m_sem = asd::Semaphore(1);
		void Lock() {
			bool timeout = true;
			for (int i=0; i<10 && timeout; ++i) {
				timeout = !m_sem.Wait(100);
				EXPECT_FALSE(timeout);
				ASSERT_TRUE(i<10);
			}
		}
		void Unlock() {
			m_sem.Post();
		}
	};

	// 한번에 포스팅할 이벤트 수
	const int PostCount = 10;

	// 전체 작업량
	const int JobCount = 100 * PostCount;

	// 이벤트 큐
	Mutex mtx;
	asd::Semaphore ev;
	std::list<int> jobQueue;

	// 완료되면 1로 셋팅
	int completeTable[JobCount] = {0};

	ASSERT_EQ(ev.GetCount(), 0);
	ASSERT_EQ(jobQueue.size(), 0);

	// 1. Worker Thread 생성
	volatile bool run = true;
	std::thread threads[4];
	for (auto& t : threads) {
		t = std::thread([&](){
			while (true) {
				ev.Wait(10);
				mtx.Lock();
				if (jobQueue.empty()) {
					mtx.Unlock();
					if (run)
						continue;
					else
						break;
				}
				int n = jobQueue.front();
				jobQueue.pop_front();
				mtx.Unlock();

				++completeTable[n];
			}
		});
	}

	// 2. 작업 포스팅
	for (int i=1; i<=JobCount; ++i) {
		mtx.Lock();
		jobQueue.push_back( i - 1 );
		mtx.Unlock();
		if ( i % PostCount == 0 )
			ev.Post(PostCount);
	}

	run = false;
	for (auto& t : threads)
		t.join();

	// 3. 정산
	EXPECT_EQ(ev.GetCount(), 0);
	EXPECT_EQ(jobQueue.size(), 0);
	for (auto b : completeTable) {
		EXPECT_EQ(b, 1);
	}
}

#include "stdafx.h"
#include "asd/redis.h"

namespace asdtest_redis
{
	TEST(Redis, Simple)
	{
		asd::RedisContext redis;
		ASSERT_TRUE(redis.Connect("127.0.0.1"));

		auto reply = redis.Command("SET test %d", 1234);
		ASSERT_EQ(reply.Error(), nullptr);
		auto t = asd::RedisReply::type(reply);

		reply = redis.Command("GET test");
		ASSERT_EQ(reply.Error(), nullptr);
		ASSERT_EQ(reply.integer(), 1234);

		redis.Disconnect();
	}
}

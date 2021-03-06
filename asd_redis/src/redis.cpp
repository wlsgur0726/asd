#include "asd/redis.h"
#include "hiredis/hiredis.h"
#include "string.h"

#if asd_Platform_Windows
#include <WinSock2.h>
#endif


namespace asd
{
	RedisReply::RedisReply()
	{
	}

	RedisReply::RedisReply(redisReply* reply)
	{
		reset(reply);
	}

	RedisReply::RedisReply(RedisReply&& share)
	{
		operator=(std::forward<RedisReply>(share));
	}


	RedisReply& RedisReply::operator=(RedisReply&& share)
	{
		BaseType::operator=(std::forward<RedisReply>(share));
		m_lastError = share.m_lastError;
		return *this;
	}


	void RedisReply::reset(redisReply* reply /*= nullptr*/)
	{
		BaseType::reset(reply, [](redisReply* reply)
		{
			if (reply)
				::freeReplyObject(reply);
		});

		m_lastError = nullptr;

		if (reply == nullptr) {
			m_lastError = "null";
			return;
		}

		if (reply->type == RedisReply::Type_Error) {
			if (reply->str==nullptr || reply->str[0]=='\0')
				m_lastError = "unknown error";
			else
				m_lastError = reply->str;
		}
	}


	RedisReply::operator redisReply*() const
	{
		return get();
	}


	const char* RedisReply::Error() const
	{
		return m_lastError;
	}


	RedisReply::Type RedisReply::type(redisReply* reply)
	{
#define asd_StaticAssert_TypeCheck(A, B) static_assert(A == B, "unexpected RedisReply::Type value")
		asd_StaticAssert_TypeCheck(RedisReply::Type_String,		REDIS_REPLY_STRING);
		asd_StaticAssert_TypeCheck(RedisReply::Type_Array,		REDIS_REPLY_ARRAY);
		asd_StaticAssert_TypeCheck(RedisReply::Type_Integer,	REDIS_REPLY_INTEGER);
		asd_StaticAssert_TypeCheck(RedisReply::Type_Nil,		REDIS_REPLY_NIL);
		asd_StaticAssert_TypeCheck(RedisReply::Type_Status,		REDIS_REPLY_STATUS);
		asd_StaticAssert_TypeCheck(RedisReply::Type_Error,		REDIS_REPLY_ERROR);

		return reply ? (RedisReply::Type)reply->type : RedisReply::Type_Error;
	}

	int64_t RedisReply::integer(redisReply* reply)
	{
		switch (type(reply)) {
			case RedisReply::Type_Integer:
				return reply->integer;
			case RedisReply::Type_String:
				return std::atoll(reply->str);
		}
		return 0;
	}

	int RedisReply::len(redisReply* reply)
	{
		return reply ? reply->len : 0;
	}

	char* RedisReply::str(redisReply* reply)
	{
		return reply ? reply->str : nullptr;
	}

	size_t RedisReply::elements(redisReply* reply)
	{
		return reply ? reply->elements : 0;
	}

	redisReply** RedisReply::element(redisReply* reply)
	{
		return reply ? reply->element : nullptr;
	}


	RedisContext::RedisContext()
	{
	}


	RedisContext::~RedisContext()
	{
		Disconnect();
	}


	bool RedisContext::Connect(const char* ip,
							   uint16_t port /*= 6379*/)
	{
		Disconnect();

		auto len = strlen(ip) + 1;
		m_ip = new char[len];
		memcpy(m_ip, ip, len);
		m_port = port;

		timeval timeout;
		timeout.tv_sec = 5;
		timeout.tv_usec = 0;
		m_ctx = ::redisConnectWithTimeout(ip, (int)port, timeout);
		return Error() == nullptr;
	}


	bool RedisContext::IsConnected() const
	{
		return m_ctx != nullptr;
	}


	RedisReply RedisContext::Command(const char* cmd,
									 ...)
	{
		va_list args;
		va_start(args, cmd);
		auto ret = CommandV(cmd, args);
		va_end(args);
		return ret;
	}

	RedisReply RedisContext::CommandV(const char* cmd,
									  const va_list& args)
	{
		RedisReply ret;

		ret.m_lastError = Error();
		if (ret.m_lastError != nullptr)
			return ret;

		ret.reset((redisReply*)::redisvCommand(m_ctx, cmd, args));
		if (ret == nullptr) {
			ret.m_lastError = Error();
			if (ret.m_lastError == nullptr)
				ret.m_lastError = "unknown error";
			return ret;
		}

		return ret;
	}


	const char* RedisContext::Error() const
	{
		if (m_ctx == nullptr)
			return "not connected";

		if (m_ctx->err != 0) {
			if (m_ctx->errstr==nullptr || m_ctx->errstr[0]=='\0')
				return "unknown error";
			return m_ctx->errstr;
		}

		return nullptr;
	}


	void RedisContext::Disconnect()
	{
		if (m_ctx == nullptr)
			return;

		auto ctx = m_ctx;
		m_ctx = nullptr;

		if (m_ip) {
			delete[] m_ip;
			m_ip = nullptr;
		}

		::redisFree(ctx);
	}


	redisContext* RedisContext::Handle() const
	{
		return m_ctx;
	}
}

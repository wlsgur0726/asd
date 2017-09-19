#pragma once
#include "asd/asdbase.h"
#include <string>
#include <memory>

struct redisReply;
struct redisContext;

namespace asd
{
	class RedisContext;

	class DllExport RedisReply final : protected std::shared_ptr<redisReply>
	{
	public:
		friend class asd::RedisContext;
		using BaseType = std::shared_ptr<redisReply>;

		enum Type : int
		{
			Type_String = 1,
			Type_Array = 2,
			Type_Integer = 3,
			Type_NIL = 4,
			Type_Status = 5,
			Type_Error = 6,
		};

		RedisReply();
		RedisReply(redisReply* reply);
		RedisReply(const RedisReply& share);

		void reset(redisReply* reply = nullptr);
		operator redisReply*() const;
		using BaseType::operator=;

		const char* Error() const;

		static Type type(redisReply* reply);
		static int64_t integer(redisReply* reply);
		static int len(redisReply* reply);
		static char* str(redisReply* reply);
		static size_t elements(redisReply* reply);
		static redisReply** element(redisReply* reply);

	private:
		const char* m_lastError = nullptr;
	};


	class DllExport RedisContext
	{
	public:
		RedisContext();
		RedisContext(const RedisContext&) = delete;
		virtual ~RedisContext();

		bool Connect(const char* ip,
					 uint16_t port = 6379);

		RedisReply Command(const char* cmd,
						   ...);

		RedisReply CommandV(const char* cmd,
							const va_list& args);

		const char* Error() const;

		void Disconnect();

		redisContext* Handle() const;


	private:
		redisContext* m_ctx = nullptr;
		std::string m_ip;
		uint16_t m_port = 6379;
	};
}
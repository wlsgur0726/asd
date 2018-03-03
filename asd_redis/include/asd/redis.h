#pragma once
#include "asd/asdbase.h"
#include <string>
#include <memory>

#if asd_Platform_Windows
#include "./version.h"
#endif

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
			Type_Nil = 4,
			Type_Status = 5,
			Type_Error = 6,
		};

		RedisReply();
		RedisReply(RedisReply&& share);

		using BaseType::operator=;
		RedisReply& operator=(RedisReply&& share);
		void reset(redisReply* reply = nullptr);
		operator redisReply*() const;

		const char* Error() const;

		static Type type(redisReply* reply);
		inline Type type() const { return type(get()); }

		static int64_t integer(redisReply* reply);
		inline int64_t integer() const { return integer(get()); }

		static int len(redisReply* reply);
		inline int len() const { return len(get()); }

		static char* str(redisReply* reply);
		inline char* str() const { return str(get()); }

		static size_t elements(redisReply* reply);
		inline size_t elements() const { return elements(get()); }

		static redisReply** element(redisReply* reply);
		inline redisReply** element() const { return element(get()); }


	private:
		RedisReply(redisReply* reply);
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
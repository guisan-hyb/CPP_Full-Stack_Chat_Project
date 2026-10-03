#pragma once

#include "Singleton.h"
#include <memory>
#include <sw/redis++/redis++.h>

class RedisMgr : public Singleton<RedisMgr>
{
	friend class Singleton<RedisMgr>;
public:
	~RedisMgr();

	bool Get(const std::string& key, std::string& val);
	bool Set(const std::string& key, const std::string& val);
	bool Del(const std::string& key);
	bool ExistsKey(const std::string& key);

	bool LPush(const std::string& key, const std::string& val);
	bool LPop(const std::string& key, std::string& val);
	bool RPush(const std::string& key, const std::string& val);
	bool RPop(const std::string& key, std::string& val);

	bool HSet(const std::string& key, const std::string& hkey, const std::string& value);
	bool HSet(const char* key, const char* hkey, const char* hvalue, std::size_t h_value_len);
	std::string HGet(const std::string& key, const std::string& hkey);
	bool HDel(const std::string& key, const std::string& field);

	void Close() {
		_redis.reset();
	}

private:
	RedisMgr();
	std::unique_ptr<sw::redis::Redis> _redis;
};
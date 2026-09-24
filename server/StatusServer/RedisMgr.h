#pragma once

#include "const.h"

class RedisMgr : public Singleton<RedisMgr> 
{
	friend class Singleton<RedisMgr>;
public:
	~RedisMgr();

	// String 类型操作
	bool Get(const std::string& key, std::string& value);
	bool Set(const std::string& key, const std::string& value);
	bool Del(const std::string& key);
	bool ExistsKey(const std::string& key);

	// List 类型操作
	bool LPush(const std::string& key, const std::string& value);
	bool LPop(const std::string& key, std::string& value);
	bool RPush(const std::string& key, const std::string& value);
	bool RPop(const std::string& key, std::string& value);

	// Hash 类型操作
	bool HSet(const std::string& key, const std::string& hkey, const std::string& value);
	bool HSet(const char* key, const char* hkey, const char* hvalue, size_t hvaluelen);
	std::string HGet(const std::string& key, const std::string& hkey);
	bool HDel(const std::string& key, const std::string& field);

	// 分布式锁与计数器 (补充实现)
	/*std::string acquireLock(const std::string& lockName, int lockTimeout, int acquireTimeout);
	bool releaseLock(const std::string& lockName, const std::string& identifier);

	void IncreaseCount(std::string server_name);
	void DecreaseCount(std::string server_name);
	void InitCount(std::string server_name);
	void DelCount(std::string server_name);*/

	void Close() {
		// redis++ 会自动在析构时关闭连接池，这里可以留空，或者主动 reset
		// _redis.reset();
	}

private:
	RedisMgr();
	std::unique_ptr<sw::redis::Redis> _redis;
};


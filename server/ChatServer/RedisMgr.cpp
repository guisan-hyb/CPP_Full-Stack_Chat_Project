#include "RedisMgr.h"
#include "ConfigMgr.h"
#include <iostream>
#include <chrono>

RedisMgr::RedisMgr() {
	auto& cfg = ConfigMgr::GetInst();
	auto& host = cfg["Redis"]["Host"];
	auto& port = cfg["Redis"]["Port"];
	auto& pwd = cfg["Redis"]["Passwd"];

	try {
		sw::redis::ConnectionOptions conn_opts;
		conn_opts.host = host;
		conn_opts.port = std::stoi(port);
		if (!pwd.empty()) {
			conn_opts.password = pwd;
		}

		conn_opts.connect_timeout = std::chrono::milliseconds(100);
		conn_opts.socket_timeout = std::chrono::milliseconds(100);

		sw::redis::ConnectionPoolOptions pool_opts;
		pool_opts.size = 5;
		pool_opts.wait_timeout = std::chrono::milliseconds(100);
		pool_opts.connection_lifetime = std::chrono::minutes(5);

		_redis = std::make_unique<sw::redis::Redis>(conn_opts, pool_opts);
		std::cout << "Redis connect success" << std::endl;
	}
	catch (const std::exception& e) {
		std::cerr << "Redis connect failed: " << e.what() << std::endl;
	}
}

RedisMgr::~RedisMgr() {

}



bool RedisMgr::Get(const std::string& key, std::string& val) {
	try {
		if (!_redis) {
			std::cerr << "Redis client is not initialized" << std::endl;
			return false;
		}

		auto res_val = _redis->get(key);
		if (res_val) {
			val = *res_val;
			return true;
		}
		
		return false;
	}
	catch (const std::exception& e) {
		std::cerr << "GET error: " << e.what() << std::endl;
		return false;
	}
}

bool RedisMgr::Set(const std::string& key, const std::string& value)
{
	try {
		_redis->set(key, value);
		return true;
	}
	catch (const std::exception& e) {
		std::cerr << "SET error: " << e.what() << std::endl;
		return false;
	}
}

bool RedisMgr::Del(const std::string& key)
{
	try {
		_redis->del(key);
		return true;
	}
	catch (const std::exception& e) {
		std::cerr << "DEL error: " << e.what() << std::endl;
		return false;
	}
}

bool RedisMgr::ExistsKey(const std::string& key)
{
	try {
		return _redis->exists(key) > 0;
	}
	catch (const std::exception& e) {
		std::cerr << "EXISTS error: " << e.what() << std::endl;
		return false;
	}
}




bool RedisMgr::LPush(const std::string& key, const std::string& value)
{
	try {
		_redis->lpush(key, value);
		return true;
	}
	catch (const std::exception&) {
		return false;
	}
}

bool RedisMgr::LPop(const std::string& key, std::string& value)
{
	try {
		auto val = _redis->lpop(key);
		if (val) {
			value = *val;
			return true;
		}
		return false;
	}
	catch (const std::exception&) {
		return false;
	}
}

bool RedisMgr::RPush(const std::string& key, const std::string& value)
{
	try {
		_redis->rpush(key, value);
		return true;
	}
	catch (const std::exception&) {
		return false;
	}
}

bool RedisMgr::RPop(const std::string& key, std::string& value)
{
	try {
		auto val = _redis->rpop(key);
		if (val) {
			value = *val;
			return true;
		}
		return false;
	}
	catch (const std::exception&) {
		return false;
	}
}






bool RedisMgr::HSet(const std::string& key, const std::string& hkey, const std::string& value)
{
	try {
		_redis->hset(key, hkey, value);
		return true;
	}
	catch (const std::exception&) {
		return false;
	}
}

// 支持 char* 和长度，用于存二进制数据
bool RedisMgr::HSet(const char* key, const char* hkey, const char* hvalue, size_t hvaluelen)
{
	try {
		_redis->hset(std::string(key), std::string(hkey),
			std::string(hvalue, hvaluelen));
		return true;
	}
	catch (const std::exception&) {
		return false;
	}
}

std::string RedisMgr::HGet(const std::string& key, const std::string& hkey)
{
	try {
		auto val = _redis->hget(key, hkey);
		if (val) {
			return *val;
		}
		return "";
	}
	catch (const std::exception&) {
		return "";
	}
}

bool RedisMgr::HDel(const std::string& key, const std::string& field)
{
	try {
		_redis->hdel(key, field);
		return true;
	}
	catch (const std::exception&) {
		return false;
	}
}


#include "RedisMgr.h"
#include "ConfigMgr.h"

RedisMgr::RedisMgr() {
	auto& gCfgMgr = ConfigMgr::GetInst();
	auto host = gCfgMgr["Redis"]["Host"];
	auto port = gCfgMgr["Redis"]["Port"];
	auto pwd = gCfgMgr["Redis"]["Passwd"];

	try {
		// 1. 配置连接选项
		sw::redis::ConnectionOptions conn_opts;
		conn_opts.host = host;
		conn_opts.port = std::stoi(port);
		if (!pwd.empty()) {
			conn_opts.password = pwd;
		}
		conn_opts.connect_timeout = std::chrono::milliseconds(100);// 连接超时
		conn_opts.socket_timeout = std::chrono::milliseconds(100);// 读写超时

		// 2. 配置连接池选项
		sw::redis::ConnectionPoolOptions pool_opts;
		pool_opts.size = 5;// 连接池的大小
		pool_opts.wait_timeout = std::chrono::milliseconds(100);// 获取连接等待时间
		pool_opts.connection_lifetime = std::chrono::minutes(5);// 连接最大存活时间(自动重连)

		// 3. 创建 Redis 客户端，自动启用连接池
		_redis = std::make_unique<sw::redis::Redis>(conn_opts, pool_opts);
		std::cout << "Redis connect success!" << std::endl;
	}
	catch (const std::exception& e) {
		std::cerr << "Redis connect failed: " << e.what() << std::endl;
	}
}

RedisMgr::~RedisMgr()
{
	// unique_ptr 会自动释放连接池，无需手动清理
}


// ==========================================
// String 类型操作 (最基础的 KV)
// ==========================================


bool RedisMgr::Get(const std::string& key, std::string& value)
{
	try {
		// redis++ 的 get 返回的是 OptionalString (类似 std::optional)
		// 因为 key 可能不存在。用 if(val) 自动判断是否存在
		auto val = _redis->get(key);
		if (val) {
			value = *val;
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
		// set 返回 void，如果没有抛异常就是成功
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
		// del 返回被删除的 key 的数量
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
		// exists 返回 long long，表示存在的 key 数量 (>0 即存在)
		return _redis->exists(key) > 0;
	}
	catch (const std::exception& e) {
		std::cerr << "EXISTS error: " << e.what() << std::endl;
		return false;
	}
}


// ==========================================
// List 类型操作 (常用于消息队列)
// ==========================================

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



// ==========================================
// Hash 类型操作 (常用于存储对象)
// ==========================================

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
		// redis++ 原生支持 string_view，可以安全处理二进制数据
		// 这里用string即可，用string_view会出错
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




#pragma once

#include "const.h"

class MySqlPool {
public:
	MySqlPool(const std::string& host, const std::string& port, const std::string& user,
		const std::string& pass, const std::string& schema, int poolSize)
		: _b_stop(false)
	{
		try {
			// X DevAPI 连接字符串格式: mysqlx://user:pass@host:port/schema
			std::string url = "mysqlx://" + user + ":" + pass + "@" + host + ":" + port + "/" + schema;

			// 直接利用 X DevAPI 的 Client 实现连接池
			// 设置 POOL_MAX_SIZE 为 poolSize，底层自动管理连接的借用和归还
			_client = std::make_unique<mysqlx::Client>(
				url,
				mysqlx::ClientOption::POOL_MAX_SIZE, poolSize
			);

		}
		catch (const mysqlx::Error& e) {
			std::cout << "mysql pool init failed: " << e.what() << std::endl;
			_client = nullptr;
		}
	}

	// 获取一个 Session (相当于 JDBC 中的 Connection)
	// 返回值是 RAII 对象，离开作用域自动析构，连接自动归还连接池！
	mysqlx::Session getConnection() {
		if (!_client || _b_stop) {
			throw std::runtime_error("MySQL client is not initialized or pool is stopped");
		}

		return _client->getSession();
	}

	void Close() {
		_b_stop = true;
		if (_client) {
			_client->close();
			_client = nullptr;
		}
	}

	~MySqlPool() {
		Close();
	}

private:
	std::unique_ptr<mysqlx::Client> _client;
	std::atomic<bool> _b_stop;
};




class MysqlDao {
public:
	MysqlDao();
	~MysqlDao();
	int RegUser(const std::string& name, const std::string& email, const std::string& pwd);
	bool CheckEmail(const std::string& name, const std::string& email);
	bool UpdatePwd(const std::string& name, const std::string& newpwd);

private:
	std::unique_ptr<MySqlPool> _pool;
};
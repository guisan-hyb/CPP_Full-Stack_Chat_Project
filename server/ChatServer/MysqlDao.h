#include "const.h"
#include "data.h"
#include <memory>
#include <mysqlx/xdevapi.h>
#include <atomic>
#include <iostream>

class MysqlPool {
public:
	MysqlPool(const std::string& host, const std::string& port, const std::string& user,
		const std::string& pass, const std::string& schema, int poolSize)
		: _b_stop(false)
	{
		try {
			std::string url = "mysqlx://" + user + ":" + pass + "@" + host + ":" + port + "/" + schema;
			_client = std::make_unique<mysqlx::Client>(
				url,
				mysqlx::ClientOption::POOL_MAX_SIZE, poolSize
			);
		}
		catch (const mysqlx::Error& e) {
			std::cerr << "mysql pool init failed: " << e.what() << std::endl;
			_client = nullptr;
		}
	}

	~MysqlPool() {
		Close();
	}

	mysqlx::Session getConnection() {
		if (!_client || _b_stop) {
			throw std::runtime_error("Mysql client is not initialized or pool is stopped");
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
	bool CheckPwd(const std::string& email, const std::string& pwd, UserInfo& userInfo);
	std::shared_ptr<UserInfo> GetUser(int uid);

private:
	std::unique_ptr<MysqlPool> _pool;
};
#include "MysqlDao.h"
#include "ConfigMgr.h"

MysqlDao::MysqlDao() {
	auto& cfg = ConfigMgr::GetInst();
	const auto& host = cfg["Mysql"]["Host"];
	const auto& port = cfg["Mysql"]["XPort"];
	const auto& pwd = cfg["Mysql"]["Passwd"];
	const auto& schema = cfg["Mysql"]["Schema"];
	const auto& user = cfg["Mysql"]["User"];
	_pool.reset(new MySqlPool(host, port, user, pwd, schema, 5));
}

MysqlDao::~MysqlDao() {
	_pool->Close();
}

int MysqlDao::RegUser(const std::string& name, const std::string& email, const std::string& pwd)
{
	try {
		// 获取 Session (相当于获取连接)
		// 注意：sess 是局部变量，函数结束自动析构，连接自动归还池子，不需要手动 returnConnection
		mysqlx::Session sess = _pool->getConnection();

		// X DevAPI 执行原生 SQL 并绑定参数
		// 调用存储过程 reg_user，传入 3 个参数，第 4 个是输出参数 @result
		sess.sql("CALL reg_user(?,?,?,@result)")
			.bind(name)
			.bind(email)
			.bind(pwd)
			.execute();

		// 读取会话变量 @result
		mysqlx::SqlResult res = sess.sql("SELECT @result AS result").execute();
		mysqlx::Row row = res.fetchOne();

		if (row) {
			// 判断是否为 NULL，如果不为空则获取 int 值
			if (row[0].isNull()) return -1;

			int result = row[0].get<int>();
			std::cout << "Result: " << result << std::endl;
			return result;
		}

		return -1;
	}
	catch (const mysqlx::Error& e) {
		std::cerr << "MySQL Error: " << e.what() << std::endl;
		return -1;
	}
	catch (std::exception& e) {
		std::cerr << "Standard Error: " << e.what() << std::endl;
		return -1;
	}
}


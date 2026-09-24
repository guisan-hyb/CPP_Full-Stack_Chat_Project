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

bool MysqlDao::CheckEmail(const std::string& name, const std::string& email) {
	try {
		// 获取Session
		mysqlx::Session sess = _pool->getConnection();

		// 执行查询，绑定参数
		mysqlx::SqlResult res = sess.sql("SELECT email FROM user WHERE name = ?")
								.bind(name)
								.execute();
		
		// 获取第一行 (因为用户名通常是唯一的，最多只有一条结果)
		mysqlx::Row row = res.fetchOne();

		if (row) { // 如果查到了数据
			// 安全地获取字符串，注意处理可能为 NULL 的情况
			std::string db_email = row[0].isNull() ? "" : row[0].get<std::string>();
			std::cout << "Check Email: " << db_email << std::endl;

			// 比对邮箱是否一致
			return email == db_email;
		}

		// 如果没查到该用户，直接返回 false
		return false;
	}
	catch (const mysqlx::Error& e) {
		std::cerr << "MySQL Error: " << e.what() << std::endl;
		return false;
	}
	catch (const std::exception& e) {
		std::cerr << "Standard Error: " << e.what() << std::endl;
		return false;
	}
}

bool MysqlDao::UpdatePwd(const std::string& name, const std::string& newpwd) {
	try {
		// 获取 Session
		mysqlx::Session sess = _pool->getConnection();

		// 执行更新操作，链式绑定参数 (按 SQL 中 ? 的顺序)
		mysqlx::SqlResult res = sess.sql("UPDATE user SET pwd = ? WHERE name = ?")
								.bind(newpwd)
								.bind(name)
								.execute();

		// 获取受影响的行数
		int updateCount = res.getAffectedItemsCount();
		std::cout << "Updated rows: " << updateCount << std::endl;

		return true;
	}
	catch (const mysqlx::Error& e) {
		std::cerr << "MySQL Error: " << e.what() << std::endl;
	}
	catch (const std::exception& e) {
		std::cerr << "Standard Error: " << e.what() << std::endl;
	}
}

bool MysqlDao::CheckPwd(const std::string& email, const std::string& pwd, UserInfo& userInfo) {
	try {
		// 获取 Session
		mysqlx::Session sess = _pool->getConnection();

		mysqlx::SqlResult res = sess.sql("SELECT uid, name, email, pwd FROM user WHERE email = ?")
								.bind(email)
								.execute();

		// fetchOne() 直接获取第一行，如果没有查到数据会返回一个空的 Row
		mysqlx::Row row = res.fetchOne();

		if (!row) return false; // 没查到该用户

		// 获取密码
		std::string origin_pwd = row[3].isNull() ? "" : row[3].get<std::string>();
		std::cout << "Password: " << origin_pwd << std::endl;

		// 密码比对
		if (pwd != origin_pwd) {
			return false;
		}

		// 密码正确则填充userInfo
		userInfo.uid = row[0].get<int>();
		userInfo.name = row[1].isNull() ? "" : row[1].get<std::string>();
		userInfo.email = row[2].isNull() ? "" : row[2].get<std::string>();
		userInfo.pwd = origin_pwd;

		return true;
	}
	catch (const mysqlx::Error& e) {
		std::cerr << "MySQL Error: " << e.what() << std::endl;
		return false;
	}
	catch (const std::exception& e) {
		std::cerr << "Standard Error: " << e.what() << std::endl;
		return false;
	}
}


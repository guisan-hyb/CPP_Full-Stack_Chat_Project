#include "MysqlDao.h"
#include "ConfigMgr.h"

MysqlDao::MysqlDao() {
	auto& cfg = ConfigMgr::GetInst();
	const auto& host = cfg["Mysql"]["Host"];
	const auto& port = cfg["Mysql"]["XPort"];
	const auto& pwd = cfg["Mysql"]["Passwd"];
	const auto& schema = cfg["Mysql"]["Schema"];
	const auto& user = cfg["Mysql"]["User"];

	_pool.reset(new MysqlPool(host, port, user, pwd, schema, 5));
}

MysqlDao::~MysqlDao() {
	_pool->Close();
}


int MysqlDao::RegUser(const std::string& name, const std::string& email, const std::string& pwd) {
	try {
		mysqlx::Session sess = _pool->getConnection();
		sess.sql("CALL reg_user(?,?,?,@result)")
			.bind("name")
			.bind("email")
			.bind("pwd")
			.execute();

		mysqlx::SqlResult res = sess.sql("SELECT @result AS result").execute();
		mysqlx::Row row = res.fetchOne();

		if (row) {
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
		mysqlx::Session sess = _pool->getConnection();
		auto res = sess.sql("SELECT email FROM user WHERE name = ?")
			.bind(email)
			.execute();

		mysqlx::Row row = res.fetchOne();
		if (row) {
			std::string db_email = row[0].isNull() ? "" : row[0].get<std::string>();
			std::cout << "Check Email: " << db_email << std::endl;

			return db_email == email;
		}

		return false;
	}
	catch (const mysqlx::Error& e) {
		std::cerr << "MySQL Error: " << e.what() << std::endl;
		return false;
	}
	catch (std::exception& e) {
		std::cerr << "Standard Error: " << e.what() << std::endl;
		return false;
	}
}

bool MysqlDao::UpdatePwd(const std::string& name, const std::string& newpwd) {
	try {
		mysqlx::Session sess = _pool->getConnection();
		mysqlx::SqlResult res = sess.sql("UPDATE user SET pwd = ? WHERE name = ?")
			.bind(newpwd)
			.bind(name)
			.execute();

		int updateCount = res.getAffectedItemsCount();
		std::cout << "Updated rows: " << updateCount << std::endl;

		return true;
	}
	catch (const mysqlx::Error& e) {
		std::cerr << "MySQL Error: " << e.what() << std::endl;
		return false;
	}
	catch (std::exception& e) {
		std::cerr << "Standard Error: " << e.what() << std::endl;
		return false;
	}
}

bool MysqlDao::CheckPwd(const std::string& email, const std::string& pwd, UserInfo& userInfo) {
	try {
		mysqlx::Session sess = _pool->getConnection();
		mysqlx::SqlResult res = sess.sql("SELECT uid, name, email, pwd FROM user WHERE email = ?")
			.bind(email)
			.execute();

		mysqlx::Row row = res.fetchOne();

		if (!row) return false;

		std::string origin_pwd = row[3].isNull() ? "" : row[3].get<std::string>();
		std::cout << "Password: " << origin_pwd << std::endl;

		if (pwd != origin_pwd) false;

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
	catch (std::exception& e) {
		std::cerr << "Standard Error: " << e.what() << std::endl;
		return false;
	}
}

std::shared_ptr<UserInfo> MysqlDao::GetUser(int uid)
{
	try {
		mysqlx::Session sess = _pool->getConnection();
		auto res = sess.sql("SELECT uid, name, email, pwd FROM user WHERE uid = ?")
			.bind(uid)
			.execute();

		auto row = res.fetchOne();
		std::shared_ptr<UserInfo> user_info = std::make_shared<UserInfo>();
		
		user_info->uid = uid;
		user_info->pwd = row[3].isNull() ? "" : row[3].get<std::string>();
		user_info->name = row[1].isNull() ? "" : row[1].get<std::string>();
		user_info->email = row[2].isNull() ? "" : row[2].get<std::string>();

		return user_info;
	}
	catch (const mysqlx::Error& e) {
		std::cerr << "MySQL Error: " << e.what() << std::endl;
		return nullptr;
	}
	catch (std::exception& e) {
		std::cerr << "Standard Error: " << e.what() << std::endl;
		return nullptr;
	}
}


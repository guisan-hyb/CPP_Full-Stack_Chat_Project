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
		auto res = sess.sql("SELECT * FROM user WHERE uid = ?")
			.bind(uid)
			.execute();

		auto row = res.fetchOne();
		// 没有这个用户时, fetchOne 返回的是空行, 直接返回空指针, 由调用方决定怎么回包
		if (!row) {
			return nullptr;
		}

		std::shared_ptr<UserInfo> user_info = std::make_shared<UserInfo>();
		
		user_info->uid = row[1].isNull() ? 0 : row[1].get<int>();
		user_info->name = row[2].isNull() ? "" : row[2].get<std::string>();
		user_info->email = row[3].isNull() ? "" : row[3].get<std::string>();
		user_info->pwd = row[4].isNull() ? "" : row[4].get<std::string>();
		user_info->nick = row[5].isNull() ? "" : row[5].get<std::string>();
		user_info->desc = row[6].isNull() ? "" : row[6].get<std::string>();
		user_info->sex = row[7].isNull() ? 0 : row[7].get<int>();
		user_info->icon = row[8].isNull() ? "" : row[8].get<std::string>();

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

std::shared_ptr<UserInfo> MysqlDao::GetUser(const std::string& name) {
	try {
		mysqlx::Session sess = _pool->getConnection();
		auto res = sess.sql("SELECT * FROM user WHERE name = ?")
			.bind(name)
			.execute();

		auto row = res.fetchOne();
		if (!row) {
			return nullptr;
		}

		std::shared_ptr<UserInfo> user_info = std::make_shared<UserInfo>();

		user_info->uid = row[1].isNull() ? 0 : row[1].get<int>();
		user_info->name = row[2].isNull() ? "" : row[2].get<std::string>();
		user_info->email = row[3].isNull() ? "" : row[3].get<std::string>();
		user_info->pwd = row[4].isNull() ? "" : row[4].get<std::string>();
		user_info->nick = row[5].isNull() ? "" : row[5].get<std::string>();
		user_info->desc = row[6].isNull() ? "" : row[6].get<std::string>();
		user_info->sex = row[7].isNull() ? 0 : row[7].get<int>();
		user_info->icon = row[8].isNull() ? "" : row[8].get<std::string>();

		return user_info;
	}
	catch (const mysqlx::Error& e) {
		std::cerr << "Mysql Error: " << e.what() << std::endl;
		return nullptr;
	}
	catch (std::exception& e) {
		std::cerr << "Standard Error: " << e.what() << std::endl;
		return nullptr;
	}
}

bool MysqlDao::AddFriendApply(const int& from, const int& to) {
	try {
		mysqlx::Session sess = _pool->getConnection();
		
		mysqlx::SqlResult res = sess.sql(
			"INSERT INTO friend_apply (from_uid, to_uid) VALUES (?, ?)"
			"ON DUPLICATE KEY UPDATE from_uid = from_uid, to_uid = to_uid"
		).bind(from, to).execute();

		return true;
	}
	catch (const mysqlx::Error& e) {
		std::cerr << "Mysql Error: " << e.what() << std::endl;
		return false;
	}
	catch (const std::exception& e) {
		std::cerr << "Standard exception: " << e.what() << std::endl;
		return false;
	}
}

bool MysqlDao::GetApplyList(int to_uid, std::vector<std::shared_ptr<ApplyInfo>>& apply_list, int begin, int limit) {
	try {
		mysqlx::Session sess = _pool->getConnection();
		mysqlx::SqlResult res = sess.sql(R"(
			SELECT friend_apply.from_uid, friend_apply.status, user.name, user.nick, user.sex
			FROM friend_apply	
			JOIN user ON friend_apply.from_uid = user.uid
			WHERE friend_apply.to_uid = ?
			AND friend_apply.id > ?
			ORDER BY friend_apply.id ASC
			LIMIT ?
		)")
			.bind(to_uid)
			.bind(begin)
			.bind(limit)
			.execute();

		for (mysqlx::Row& row : res) {
			auto uid = row[0].isNull() ? 0 : row[0].get<int>();
			auto status = row[1].isNull() ? 0 : row[1].get<int>();
			auto name = row[2].isNull() ? "" : row[2].get<std::string>();
			auto nick = row[3].isNull() ? "" : row[3].get<std::string>();
			int sex = row[4].isNull() ? 0 : row[4].get<int>();
			auto apply_one = std::make_shared<ApplyInfo>(uid, name, "", "", nick, sex, status);
			apply_list.push_back(apply_one);
		}

		return true;
	}
	catch (const mysqlx::Error& e) {
		std::cerr << "Mysql Error: " << e.what() << std::endl;
		return false;
	}
	catch (const std::exception& e) {
		std::cerr << "Standard exception: " << e.what() << std::endl;
		return false;
	}
}

bool MysqlDao::AuthAndAddFriend(const int& applicant_uid, const int& accepter_uid, const std::string& back_name) {
	mysqlx::Session sess = _pool->getConnection();
	try {
		// 开始事务
		sess.startTransaction();

		// 更新friend_apply中status
		mysqlx::SqlResult res1 = sess.sql(R"(
			UPDATE friend_apply
			SET status = 1
			WHERE from_uid = ?
			AND to_uid = ?
		)").bind(applicant_uid)
			.bind(accepter_uid)
			.execute();

		// 这条申请不存在
		if (res1.getAffectedItemsCount() == 0) {
			sess.rollback();
			std::cerr << "friend apply not found: from=" << applicant_uid << " to=" << accepter_uid << std::endl;
			return false;
		}

		// 插入认证方好友数据
		mysqlx::SqlResult res2 = sess.sql(R"(
			INSERT IGNORE INTO friend(self_id, friend_id, back)
			VALUES (?, ?, ?)
		)").bind(accepter_uid, applicant_uid, back_name).execute();

		// mysqlx 的 getAffectedItemsCount() 返回 size_t，不会小于 0；
		// 如果执行出错通常以异常形式抛出，这里仅做告警检查。
		if (res2.getWarningsCount() > 0) {
			// 可以选择记录日志，INSERT IGNORE 在主键/唯一键冲突时也会产生告警
			std::cerr << "AddFriend: first insert has warnings, count = "
				<< res2.getWarningsCount() << std::endl;
		}

		// 插入申请方好友数据
		mysqlx::SqlResult res3 = sess.sql(R"(
			INSERT IGNORE INTO friend(self_id, friend_id, back)
			VALUES (?, ?, ?)
		)").bind(applicant_uid, accepter_uid, std::string("")).execute();

		if (res3.getWarningsCount() > 0) {
			std::cerr << "AddFriend: second insert has warnings, count = "
				<< res3.getWarningsCount() << std::endl;
		}

		// 提交事务
		sess.commit();
		std::cout << "Auth And Add Friend success" << std::endl;

		return true;
	}
	catch (const mysqlx::Error& e) {
		//// 出现错误则回滚
		//try {
		//	sess.rollback();
		//}
		//catch (const std::exception& ex) {
		//	std::cerr << "rollback failed: " << ex.what() << std::endl;
		//}

		std::cerr << "Mysql Error: " << e.what() << std::endl;
		return false;
	}
	catch (const std::exception& e) {  // sess 析构时如果有未提交事务会自动 rollback
		//// 出现错误则回滚
		//try {
		//	sess.rollback();
		//}
		//catch (std::exception& ex) {
		//	std::cerr << "rollback failed: " << ex.what() << std::endl;
		//}

		std::cerr << "Standard exception: " << e.what() << std::endl;
		return false;
	}
}

bool MysqlDao::GetFriendList(int self_id, std::vector<std::shared_ptr<UserInfo>>& user_list) {
	try {
		mysqlx::Session sess = _pool->getConnection();
		mysqlx::SqlResult res = sess.sql(R"(
			SELECT * 
			FROM friend
			WHERE self_id = ?
		)").bind(self_id).execute();

		for (mysqlx::Row& row : res) {
			auto friend_id = row[2].isNull() ? 0 : row[2].get<int>();
			auto back = row[3].isNull() ? "" : row[3].get<std::string>();
			// 进一步查询 friend_id 对应的信息
			auto user_info = GetUser(friend_id);
			if (user_info == nullptr) {
				continue;
			}

			user_info->back = user_info->name;
			user_list.push_back(user_info);
		}

		return true;
	}
	catch (const mysqlx::Error& e) {
		std::cerr << "Mysql Error: " << e.what() << std::endl;
		return false;
	}
	catch (const std::exception& e) {
		std::cerr << "Standard exception: " << e.what() << std::endl;
		return false;
	}
}


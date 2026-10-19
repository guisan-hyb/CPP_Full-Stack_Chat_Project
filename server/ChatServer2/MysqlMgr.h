#pragma once

#include "Singleton.h"
#include "MysqlDao.h"

class MysqlMgr : public Singleton<MysqlMgr>
{
	friend class Singleton<MysqlMgr>;
public:
	~MysqlMgr();
	int RegUser(const std::string& name, const std::string& email, const std::string& pwd);
	bool CheckEmail(const std::string& name, const std::string& email);
	bool UpdatePwd(const std::string& name, const std::string& newpwd);
	bool CheckPwd(const std::string& email, const std::string& pwd, UserInfo& userInfo);
	std::shared_ptr<UserInfo> GetUser(int uid);
	std::shared_ptr<UserInfo> GetUser(const std::string& name);
	bool AddFriendApply(const int& from, const int& to);
	bool GetApplyList(int to_uid, std::vector<std::shared_ptr<ApplyInfo>>& apply_list, int begin, int limit = 10);

private:
	MysqlMgr();

private:
	MysqlDao _dao;
};


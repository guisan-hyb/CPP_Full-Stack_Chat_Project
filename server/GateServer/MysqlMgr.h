#pragma once

#include "const.h"
#include "MysqlDao.h"

class MysqlMgr : public Singleton<MysqlMgr>
{
	friend class Singleton<MysqlMgr>;
public:
	~MysqlMgr();
	int RegUser(const std::string& name, const std::string& email, const std::string& password_hash);
	bool CheckEmail(const std::string& name, const std::string& email);
	bool UpdatePasswordHash(const std::string& name, const std::string& password_hash);
	bool VerifyPassword(const std::string& email, const std::string& password, UserInfo& userInfo);

private:
	MysqlMgr();

private:
	MysqlDao _dao;
};

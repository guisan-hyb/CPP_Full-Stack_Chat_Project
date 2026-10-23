#include "MysqlMgr.h"

MysqlMgr::MysqlMgr() {

}

MysqlMgr::~MysqlMgr()
{

}

int MysqlMgr::RegUser(const std::string& name, const std::string& email, const std::string& password_hash) {
	return _dao.RegUser(name, email, password_hash);
}

bool MysqlMgr::CheckEmail(const std::string& name, const std::string& email)
{
	return _dao.CheckEmail(name, email);
}

bool MysqlMgr::UpdatePasswordHash(const std::string& name, const std::string& password_hash)
{
	return _dao.UpdatePasswordHash(name, password_hash);
}

bool MysqlMgr::VerifyPassword(const std::string& email, const std::string& password, UserInfo& userInfo)
{
	return _dao.VerifyPassword(email, password, userInfo);
}


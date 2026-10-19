#include "MysqlMgr.h"

MysqlMgr::MysqlMgr() {

}

MysqlMgr::~MysqlMgr()
{

}

int MysqlMgr::RegUser(const std::string& name, const std::string& email, const std::string& pwd) {
	return _dao.RegUser(name, email, pwd);
}

bool MysqlMgr::CheckEmail(const std::string& name, const std::string& email)
{
	return _dao.CheckEmail(name, email);
}

bool MysqlMgr::UpdatePwd(const std::string& name, const std::string& newpwd)
{
	return _dao.UpdatePwd(name, newpwd);
}

bool MysqlMgr::CheckPwd(const std::string& email, const std::string& pwd, UserInfo& userInfo)
{
	return _dao.CheckPwd(email, pwd, userInfo);
}

std::shared_ptr<UserInfo> MysqlMgr::GetUser(int uid) {
	return _dao.GetUser(uid);
}

std::shared_ptr<UserInfo> MysqlMgr::GetUser(const std::string& name)
{
	return _dao.GetUser(name);
}

bool MysqlMgr::AddFriendApply(const int& from, const int& to)
{
	return _dao.AddFriendApply(from, to);
}

bool MysqlMgr::GetApplyList(int to_uid, std::vector<std::shared_ptr<ApplyInfo>>& apply_list, int begin, int limit)
{
	return _dao.GetApplyList(to_uid, apply_list, begin, limit);
}


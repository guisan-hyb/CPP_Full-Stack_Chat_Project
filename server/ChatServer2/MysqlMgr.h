#pragma once

#include "Singleton.h"
#include "MysqlDao.h"

class MysqlMgr : public Singleton<MysqlMgr>
{
	friend class Singleton<MysqlMgr>;
public:
	~MysqlMgr();
	std::shared_ptr<UserInfo> GetUser(int uid);
	std::shared_ptr<UserInfo> GetUser(const std::string& name);
	bool AddFriendApply(const int& from, const int& to);
	bool AreFriends(int uid, int friend_id);
	bool GetApplyList(int to_uid, std::vector<std::shared_ptr<ApplyInfo>>& apply_list, int begin, int limit = 10);
	bool AuthAndAddFriend(const int& applicant_uid, const int& accepter_uid, const std::string& back_name);
	bool GetFriendList(int self_id, std::vector<std::shared_ptr<UserInfo>>& user_list);

private:
	MysqlMgr();

private:
	MysqlDao _dao;
};


#pragma once

#include "Singleton.h"
#include <queue>
#include <thread>
#include "CSession.h"
#include <unordered_map>
#include <mutex>
#include <functional>
#include <condition_variable>
#include <nlohmann/json.hpp>
#include "data.h"

using FuncCallBack = std::function<void(std::shared_ptr<CSession>, const short& msg_id, const std::string& msg_data)>;

using json = nlohmann::json;

class LogicSystem : public Singleton<LogicSystem>
{
	friend class Singleton<LogicSystem>;
public:
	~LogicSystem();
	void PostMsgToQue(std::shared_ptr<LogicNode> msg);

private:
	LogicSystem();
	void DealMsg();
	void RegisterCallBacks();
	bool GetBaseInfo(const std::string& base_key, int uid, std::shared_ptr<UserInfo>& user_info);
	void GetUserByUid(const std::string& uid_str, json& root);
	void GetUserByName(const std::string& name, json& root);
	bool isPureDigit(const std::string& s);
	bool GetFriendApplyInfo(int to_uid, std::vector<std::shared_ptr<ApplyInfo>>& user_list);
	bool GetFriendList(int self_id, std::vector<std::shared_ptr<UserInfo>>& user_list);

private:
	std::thread _work_thread;
	std::queue<std::shared_ptr<LogicNode>> _msg_que;
	std::mutex _mtx;
	std::condition_variable _cond;
	std::atomic<bool> _b_stop;

	std::unordered_map<short, FuncCallBack> _func_callbacks;
};

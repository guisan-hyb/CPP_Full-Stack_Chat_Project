#pragma once

#include "Singleton.h"
#include <queue>
#include <thread>
#include "CSession.h"
#include <unordered_map>
#include <mutex>
#include <functional>
#include <condition_variable>
#include "data.h"

using FuncCallBack = std::function<void(std::shared_ptr<CSession>, const short& msg_id, const std::string& msg_data)>;


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

private:
	std::thread _work_thread;
	std::queue<std::shared_ptr<LogicNode>> _msg_que;
	std::mutex _mtx;
	std::condition_variable _cond;
	std::atomic<bool> _b_stop;

	std::unordered_map<short, FuncCallBack> _func_callbacks;
	std::unordered_map<int, std::shared_ptr<UserInfo>> _users;
};
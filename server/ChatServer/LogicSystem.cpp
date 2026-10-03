#include "LogicSystem.h"
#include <iostream>
#include "const.h"
#include <nlohmann/json.hpp>
#include "StatusGrpcClient.h"
#include "MysqlMgr.h"

using json = nlohmann::json;

LogicSystem::LogicSystem()
	: _b_stop(false)
{
	RegisterCallBacks();
	_work_thread = std::thread(&LogicSystem::DealMsg, this);
}

LogicSystem::~LogicSystem() {
	_b_stop = true;
	_cond.notify_all();
	_work_thread.join();
}

void LogicSystem::PostMsgToQue(std::shared_ptr<LogicNode> msg)
{
	std::unique_lock<std::mutex> ulk(_mtx);
	_msg_que.push(msg);

	if (_msg_que.size() == 1) {
		ulk.unlock();
		_cond.notify_one();
	}
}

void LogicSystem::DealMsg()
{
	for (;;) {
		std::unique_lock<std::mutex> ulk(_mtx);
		_cond.wait(ulk, [this]() {
			return _b_stop || !_msg_que.empty();
		});

		if (_b_stop) {
			while (!_msg_que.empty()) {
				auto& msg = _msg_que.front();
				std::cout << "recv_msg id is: " << msg->_recvnode->GetMsgId() << std::endl;
				auto iter = _func_callbacks.find(msg->_recvnode->GetMsgId());
				if (iter == _func_callbacks.end()) {
					_msg_que.pop();
					continue;
				}

				iter->second(msg->_session, msg->_recvnode->GetMsgId(),
					std::string(msg->_recvnode->_data, msg->_recvnode->_total_len));

				_msg_que.pop();
			}
			break;
		}

		auto& msg = _msg_que.front();
		std::cout << "recv_msg id is: " << msg->_recvnode->GetMsgId() << std::endl;
		auto iter = _func_callbacks.find(msg->_recvnode->GetMsgId());
		if (iter == _func_callbacks.end()) {
			std::cout << "msg id [" << msg->_recvnode->GetMsgId() << "] handler not found" << std::endl;
			_msg_que.pop();
			continue;
		}
		iter->second(msg->_session, msg->_recvnode->GetMsgId(),
			std::string(msg->_recvnode->_data, msg->_recvnode->_total_len));
		_msg_que.pop();
	}
}

void LogicSystem::RegisterCallBacks()
{
	_func_callbacks[MSG_CHAT_LOGIN] = [this](std::shared_ptr<CSession> sess, const short& msg_id, const std::string& msg_data) {
		json src_root;
		try {
			src_root = json::parse(msg_data);
		}
		catch (const json::parse_error& e) {
			std::cerr << "LoginHandler parse json error: " << e.what() << std::endl;
			return;
		}

		auto uid = src_root["uid"].get<int>();
		auto token = src_root["token"].get<std::string>();
		std::cout << "user login uid is: " << uid << " user token is: " << token << std::endl;

		// 从状态服务器获取token匹配是否准确
		auto rsp = StatusGrpcClient::GetInst()->Login(uid, token);

		json root;
		Defer defer([&root, this, sess] {
			std::string jsonstr = root.dump();
			sess->Send(jsonstr, MSG_CHAT_LOGIN_RSP);
		});

		root["error"] = rsp.error();
		if (rsp.error() != ErrorCodes::Success) {
			return;
		}

		// 内存中查询用户信息
		auto iter = _users.find(uid);
		std::shared_ptr<UserInfo> user_info = nullptr;
		if (iter == _users.end()) {
			// 查数据库
			user_info = MysqlMgr::GetInst()->GetUser(uid);
			if (user_info == nullptr) {
				root["error"] = ErrorCodes::Uid_Invalid;
				return;
			}

			_users[uid] = user_info;
		}
		else {
			user_info = iter->second;
		}

		root["uid"] = uid;
		root["token"] = token;
		root["name"] = user_info->name;
	};
}


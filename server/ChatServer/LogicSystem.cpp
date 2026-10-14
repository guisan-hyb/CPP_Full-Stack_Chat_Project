#include "LogicSystem.h"
#include <iostream>
#include "const.h"
#include <nlohmann/json.hpp>
#include "StatusGrpcClient.h"
#include "MysqlMgr.h"
#include "RedisMgr.h"
#include "ConfigMgr.h"
#include "UserMgr.h"

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

				try {
					iter->second(msg->_session, msg->_recvnode->GetMsgId(),
						std::string(msg->_recvnode->_data, msg->_recvnode->_total_len));
				}
				catch (const json::exception& e) { // 语法错误/类型错误
					std::cerr << "json exception in handler [" << msg->_recvnode->GetMsgId() << "]: "
						<< e.what() << std::endl;
				}
				catch (const std::exception& e) { // 兜底
					std::cerr << "json exception in handler [" << msg->_recvnode->GetMsgId() << "]: "
						<< e.what() << std::endl;
				}

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

		try {
			iter->second(msg->_session, msg->_recvnode->GetMsgId(),
				std::string(msg->_recvnode->_data, msg->_recvnode->_total_len));
		}
		catch (const json::exception& e) { // 语法错误/类型错误
			std::cerr << "json exception in handler [" << msg->_recvnode->GetMsgId() << "]: "
				<< e.what() << std::endl;
		}
		catch (const std::exception& e) { // 兜底
			std::cerr << "json exception in handler [" << msg->_recvnode->GetMsgId() << "]: " 
				<< e.what() << std::endl;
		}
		
		_msg_que.pop();
	}
}

void LogicSystem::RegisterCallBacks()
{
	_func_callbacks[MSG_CHAT_LOGIN] = [this](std::shared_ptr<CSession> sess, const short& msg_id, const std::string& msg_data) {
		// 提到最前
		json root;
		Defer defer([&root, this, sess] {
			std::string jsonstr = root.dump();
			sess->Send(jsonstr, MSG_CHAT_LOGIN_RSP);
			});
		
		json src_root;
		try {
			src_root = json::parse(msg_data);
		}
		catch (const json::exception& e) {
			std::cerr << "LoginHandler parse json error: " << e.what() << std::endl;
			return;
		}

		// 类型校验: 值在不在、类型对不对,都显式判掉
		if (!src_root.is_object() ||
			!src_root.contains("uid") || !src_root["uid"].is_number_integer() ||
			!src_root.contains("token") || !src_root["token"].is_string()) {
			std::cerr << "login payload invalid: " << msg_data << std::endl;
			return;                                          // 回 {"error":1001}
		}

		int uid = src_root["uid"].get<int>();
		std::string token = src_root["token"].get<std::string>();
		if (uid <= 0 || token.empty()) { root["error"] = ErrorCodes::Uid_Invalid; return; }

		// 从redis 获取用户token是否正确 (把原来从状态服务器获取token匹配是否准确改了)
		std::string uid_str = std::to_string(uid);
		std::string token_key = std::string(USERTOKENPREFIX) + uid_str;
		std::string token_value = "";
		bool success = RedisMgr::GetInst()->Get(token_key, token_value);
		if (!success) {
			root["error"] = ErrorCodes::Uid_Invalid;
			return;
		}

		if (token != token_value) {
			root["error"] = ErrorCodes::Token_Invalid;
			return;
		}

		root["error"] = ErrorCodes::Success;

		// 这里同样也没必要从内存中查找用户信息了, 在redis+mysql中查询即可
		std::string base_key = std::string(USER_BASE_INFO) + uid_str;
		auto user_info = std::make_shared<UserInfo>();
		bool b_base = GetBaseInfo(base_key, uid, user_info);
		if (!b_base) {
			root["error"] = ErrorCodes::Uid_Invalid;
			return;
		}

		root["uid"] = uid;
		root["pwd"] = user_info->pwd;
		root["name"] = user_info->name;
		root["email"] = user_info->email;
		root["nick"] = user_info->nick;
		root["desc"] = user_info->desc;
		root["sex"] = user_info->sex;
		root["icon"] = user_info->icon;
		root["token"] = token;

		// 从数据库获取申请列表 (只获取一次就行, 申请列表会保存在客户端本地, 只同步一些待申请的即可)

		// 获取好友列表


		// 处理该新连接：
		auto server_name = ConfigMgr::GetInst()["SelfServer"]["Name"];
		// 将登陆数量增加
		auto rd_res = RedisMgr::GetInst()->HGet(std::string(LOGIN_COUNT), server_name);
		int conn_count = 0;// 默认连接数为0
		if (!rd_res.empty()) {
			conn_count = std::stoi(rd_res);
		}
		conn_count++;

		auto conn_str = std::to_string(conn_count);
		RedisMgr::GetInst()->HSet(std::string(LOGIN_COUNT), server_name, conn_str);


		// session绑定用户id
		sess->SetUserId(uid);

		// 为用户设置登录 ip server 的名字
		std::string ip_key = std::string(USERIPPREFIX) + uid_str;
		RedisMgr::GetInst()->Set(ip_key, server_name);

		// uid 和 session 绑定管理, 方便以后踢人操作
		UserMgr::GetInst()->SetUserSession(uid, sess);

		return;
	};
}


bool LogicSystem::GetBaseInfo(const std::string& base_key, int uid, std::shared_ptr<UserInfo>& user_info) {
	// 1. try redis cache first
	std::string info_str = "";
	if (RedisMgr::GetInst()->Get(base_key, info_str) && !info_str.empty()) {
		try {
			auto src_root = json::parse(info_str);
			auto cached = std::make_shared<UserInfo>();
			cached->uid = src_root.value("uid", uid);
			cached->name = src_root.value("name", std::string());
			cached->pwd = src_root.value("pwd", std::string());
			cached->email = src_root.value("email", std::string());
			cached->nick = src_root.value("nick", std::string());
			cached->desc = src_root.value("desc", std::string());
			cached->sex = src_root.value("sex", 0);
			cached->icon = src_root.value("icon", std::string());

			// empty cache entries were written by the old buggy code, ignore them and fall back to mysql
			if (!cached->name.empty() || !cached->email.empty()) {
				user_info = cached;
				std::cout << "user login uid is  " << user_info->uid << " name  is "
					<< user_info->name << " pwd is " << user_info->pwd << " email is " << user_info->email << std::endl;
				return true;
			}
			std::cerr << "empty user info in redis, fallback to mysql, uid: " << uid << std::endl;
		}
		catch (const json::exception& e) {
			std::cerr << "info_str parse error: " << e.what() << std::endl;
		}
	}

	// 2. not in redis (or the cached value is invalid), load from mysql
	auto user_info_from_mysql = MysqlMgr::GetInst()->GetUser(uid);
	if (user_info_from_mysql == nullptr) {
		return false;
	}

	// mysql returns an empty row when the uid does not exist, treat it as user not found
	if (user_info_from_mysql->name.empty() && user_info_from_mysql->email.empty()) {
		std::cerr << "user not found in mysql, uid: " << uid << std::endl;
		return false;
	}

	user_info = user_info_from_mysql;
	std::cout << "user login uid is  " << user_info->uid << " name  is "
		<< user_info->name << " pwd is " << user_info->pwd << " email is " << user_info->email << std::endl;

	// 3. write the mysql result back to the redis cache
	json redis_root;
	redis_root["uid"] = user_info->uid;
	redis_root["pwd"] = user_info->pwd;
	redis_root["name"] = user_info->name;
	redis_root["email"] = user_info->email;
	redis_root["nick"] = user_info->nick;
	redis_root["desc"] = user_info->desc;
	redis_root["sex"] = user_info->sex;
	redis_root["icon"] = user_info->icon;
	RedisMgr::GetInst()->Set(base_key, redis_root.dump());

	return true;
}


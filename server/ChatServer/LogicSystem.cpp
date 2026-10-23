#include "LogicSystem.h"
#include <iostream>
#include "const.h"
#include "StatusGrpcClient.h"
#include "MysqlMgr.h"
#include "RedisMgr.h"
#include "ConfigMgr.h"
#include "UserMgr.h"
#include "ChatGrpcClient.h"



LogicSystem::LogicSystem()
	: _b_stop(false)
{
	const auto ttl_config = ConfigMgr::GetInst()["Cache"]["UserProfileTtlSeconds"];
	if (!ttl_config.empty()) {
		try {
			const auto ttl = std::stoll(ttl_config);
			if (ttl > 0) {
				_user_profile_ttl = std::chrono::seconds(ttl);
			}
		}
		catch (const std::exception&) {
			std::cerr << "invalid Cache.UserProfileTtlSeconds, using 600 seconds" << std::endl;
		}
	}

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
	_func_callbacks[MSG_CHAT_LOGIN] = std::bind(&LogicSystem::HandleChatLogin, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
	_func_callbacks[ID_SEARCH_USER_REQ] = std::bind(&LogicSystem::HandleSearchUser, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
	_func_callbacks[ID_ADD_FRIEND_REQ] = std::bind(&LogicSystem::HandleAddFriend, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
	_func_callbacks[ID_AUTH_FRIEND_REQ] = std::bind(&LogicSystem::HandleAuthFriend, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
	_func_callbacks[ID_TEXT_CHAT_MSG_REQ] = std::bind(&LogicSystem::HandleTextChatMsg, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
}

void LogicSystem::HandleChatLogin(std::shared_ptr<CSession> sess, const short& msg_id, const std::string& msg_data)
{
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

		if (sess->GetUserId() > 0) {
			root["error"] = ErrorCodes::Token_Invalid;
			std::cerr << "chat login rejected: session already authenticated" << std::endl;
			return;
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

		// 这里同样也没必要从内存中查找用户信息了, 在redis+mysql中查询即可
		std::string base_key = std::string(USER_BASE_INFO) + uid_str;
		auto user_info = std::make_shared<UserInfo>();
		bool b_base = GetBaseInfo(base_key, uid, user_info);
		if (!b_base) {
			root["error"] = ErrorCodes::Uid_Invalid;
			return;
		}

		root["uid"] = uid;
		root["name"] = user_info->name;
		root["email"] = user_info->email;
		root["nick"] = user_info->nick;
		root["desc"] = user_info->desc;
		root["sex"] = user_info->sex;
		root["icon"] = user_info->icon;
		root["token"] = token;

		// 做任何逻辑的时候，先实现内存层面，再实现从缓存加载层面，最后实现从数据库加载层面
		//
		// 从数据库获取申请列表 (只获取一次就行, 申请列表会保存在客户端本地, 只同步一些待申请的即可)
		std::vector<std::shared_ptr<ApplyInfo>> apply_list;
		bool b_apply = GetFriendApplyInfo(uid, apply_list);
		if (b_apply) {
			json apply_array = json::array();
			for (auto& apply : apply_list) {
				json info;
				info["name"] = apply->_name;
				info["uid"] = apply->_uid;
				info["icon"] = apply->_icon;
				info["nick"] = apply->_nick;
				info["sex"] = apply->_sex;
				info["status"] = apply->_status;

				apply_array.push_back(info);
			}
			root["apply_list"] = apply_array;
		}

		// 获取好友列表
		std::vector<std::shared_ptr<UserInfo>> friend_list;
		bool b_friend_list = GetFriendList(uid, friend_list);
		json friend_array = json::array();
		for (auto& friend_ele : friend_list) {
			json info;
			info["name"] = friend_ele->name;
			info["uid"] = friend_ele->uid;
			info["icon"] = friend_ele->icon;
			info["nick"] = friend_ele->nick;
			info["sex"] = friend_ele->sex;
			info["desc"] = friend_ele->desc;
			info["back"] = friend_ele->back;

			friend_array.push_back(info);
		}
		root["friend_list"] = friend_array;


		// 处理该新连接：
		auto server_name = ConfigMgr::GetInst()["SelfServer"]["Name"];
		long long conn_count = 0;
		if (!RedisMgr::GetInst()->HIncrBy(std::string(LOGIN_COUNT), server_name, 1, conn_count)) {
			root["error"] = ErrorCodes::RPC_Failed;
			return;
		}
		std::cout << "server " << server_name << " login count is " << conn_count << std::endl;

		// 为用户设置登录 ip server 的名字
		std::string ip_key = std::string(USERIPPREFIX) + uid_str;
		if (!RedisMgr::GetInst()->Set(ip_key, server_name)) {
			long long rollback_count = 0;
			RedisMgr::GetInst()->HIncrBy(std::string(LOGIN_COUNT), server_name, -1, rollback_count);
			root["error"] = ErrorCodes::RPC_Failed;
			return;
		}

		// session绑定用户id
		sess->SetUserId(uid);

		// uid 和 session 绑定管理, 方便以后踢人操作
		UserMgr::GetInst()->SetUserSession(uid, sess);

		root["error"] = ErrorCodes::Success;
		return;
}

void LogicSystem::HandleSearchUser(std::shared_ptr<CSession> sess, const short& msg_id, const std::string& msg_data)
{
		json root;
		Defer defer([sess, &root, this] {
			std::string jsonstr = root.dump();
			sess->Send(jsonstr, ID_SEARCH_USER_RSP);
		});

		json src_root;
		try {
			src_root = json::parse(msg_data);
		}
		catch (const json::exception& e) {
			std::cerr << "search user parse json error: " << e.what() << std::endl;
			root["error"] = ErrorCodes::Error_Json;// 解析失败也要回包, 否则客户端会一直等下去
			return;
		}

		if (!src_root.is_object() ||
			!src_root.contains("uid") || !src_root["uid"].is_string()) {
			std::cerr << "search user payload error: " << msg_data << std::endl;
			root["error"] = ErrorCodes::Error_Json;
			return;
		}

		if (sess->GetUserId() <= 0) {
			root["error"] = ErrorCodes::Token_Invalid;
			return;
		}

		std::string uid_str = src_root["uid"].get<std::string>();// 客户端传来的可能是uid, 也可能是用户名称
		if (uid_str.empty()) {
			std::cerr << "search user: empty uid/name" << std::endl;
			root["error"] = ErrorCodes::Uid_Invalid;
			return;
		}

		bool b_digit = isPureDigit(uid_str);
		if (b_digit) {
			GetUserByUid(uid_str, root);
		}
		else {
			GetUserByName(uid_str, root);
		}
}

void LogicSystem::HandleAddFriend(std::shared_ptr<CSession> sess, const short& msg_id, const std::string& msg_data)
{
		json root;
		Defer defer([sess, &root, this]() {
			std::string jsonstr = root.dump();
			sess->Send(jsonstr, ID_ADD_FRIEND_RSP);
			});

		json src_root;
		try {
			src_root = json::parse(msg_data);
		}
		catch (const json::exception& e) {
			std::cerr << "Add Friend Json parse error: " << e.what() << std::endl;
			root["error"] = ErrorCodes::Error_Json;
			return;
		}

		if (!src_root.is_object() ||
			!src_root.contains("to_uid") || !src_root["to_uid"].is_number_integer()) {
			std::cerr << "Add Friend payload error: " << msg_data << std::endl;
			root["error"] = ErrorCodes::Error_Json;
			return;
		}

		int uid = sess->GetUserId();
		if (uid <= 0) {
			root["error"] = ErrorCodes::Token_Invalid;
			return;
		}

		int to_uid = src_root["to_uid"].get<int>();
		if (to_uid <= 0 || to_uid == uid) {
			root["error"] = ErrorCodes::Uid_Invalid;
			return;
		}

		std::string applyname = src_root.value("applyname", std::string());
		std::string backname = src_root.value("backname", std::string());
		std::cout << "user login uid is: " << uid << " applyname is: " << applyname << " backname is: " << backname
			<< " to_uid is: " << to_uid << std::endl;

		if (MysqlMgr::GetInst()->GetUser(to_uid) == nullptr) {
			root["error"] = ErrorCodes::Uid_Invalid;
			return;
		}

		if (MysqlMgr::GetInst()->AreFriends(uid, to_uid)) {
			root["error"] = ErrorCodes::User_Exist;
			return;
		}

		// 添加到数据库
		if (!MysqlMgr::GetInst()->AddFriendApply(uid, to_uid)) {
			root["error"] = ErrorCodes::Database_Failed;
			return;
		}

		root["error"] = ErrorCodes::Success;

		// 查询redis，查找to_uid对应的server_ip
		auto to_uid_str = std::to_string(to_uid);
		auto to_ip_key = std::string(USERIPPREFIX) + to_uid_str;
		std::string to_ip_value = "";
		bool b_to_ip = RedisMgr::GetInst()->Get(to_ip_key, to_ip_value);
		if (!b_to_ip) {
			return;
		}

		// 检查想加的好友所在的服务器是不是同一个服务器
		auto& cfg = ConfigMgr::GetInst();
		auto self_name = cfg["SelfServer"]["Name"];

		// 如果在同一个服务器，直接通知对方有申请消息
		if (to_ip_value == self_name) {
			auto session = UserMgr::GetInst()->GetSession(to_uid);
			if (session) {
				// 在内存中则直接发送通知对方
				json notify;
				notify["error"] = ErrorCodes::Success;
				notify["applyuid"] = uid;
				notify["name"] = applyname;
				notify["desc"] = "";
				std::string jsonstr = notify.dump();
				// 通知的是被申请人，不能发回给发起申请的 sess
				session->Send(jsonstr, ID_NOTIFY_ADD_FRIEND_REQ);
			}
			return;
		}

		// 不在同一个服务器
		std::string base_key = std::string(USER_BASE_INFO) + std::to_string(uid); // 先查一下自己的基本信息
		auto apply_info = std::make_shared<UserInfo>();
		bool b_info = GetBaseInfo(base_key, uid, apply_info);

		AddFriendReq add_req;
		add_req.set_apply_uid(uid);
		add_req.set_to_uid(to_uid);
		add_req.set_name(applyname);
		add_req.set_desc("");
		if (b_info) {
			add_req.set_icon(apply_info->icon);
			add_req.set_sex(apply_info->sex);
			add_req.set_nick(apply_info->nick);
		}

		// 发送通知
		ChatGrpcClient::GetInst()->NotifyAddFriend(to_ip_value, add_req);

}

void LogicSystem::HandleAuthFriend(std::shared_ptr<CSession> sess, const short& msg_id, const std::string& msg_data)
{
		json root;
		Defer defer([this, sess, &root] {
			std::string jsonstr = root.dump();
			sess->Send(jsonstr, ID_AUTH_FRIEND_RSP);
		});

		json src_root;
		try {
			src_root = json::parse(msg_data);
		}
		catch (const json::exception& e) {
			std::cerr << "Authen Friend Req Json parse error: " << e.what() << std::endl;
			root["error"] = ErrorCodes::Error_Json;
			return;
		}
		
		if (!src_root.is_object() ||
			!src_root.contains("to_uid") || !src_root["to_uid"].is_number_integer()) {
			std::cerr << "Authen friend payload error: " << msg_data << std::endl;
			root["error"] = ErrorCodes::Error_Json;
			return;
		}

		int uid = sess->GetUserId();
		if (uid <= 0) {
			root["error"] = ErrorCodes::Token_Invalid;
			return;
		}

		int to_uid = src_root["to_uid"].get<int>();
		if (to_uid <= 0 || to_uid == uid) {
			root["error"] = ErrorCodes::Uid_Invalid;
			return;
		}

		std::string back_name = src_root.value("back", std::string());
		std::cout << "from " << uid << "auth friend to " << to_uid << std::endl;

		auto user_info = std::make_shared<UserInfo>();

		std::string base_key = std::string(USER_BASE_INFO) + std::to_string(to_uid);
		bool b_info = GetBaseInfo(base_key, to_uid, user_info);
		if (b_info) {
			root["name"] = user_info->name;
			root["nick"] = user_info->nick;
			root["icon"] = user_info->icon;
			root["sex"] = user_info->sex;
			root["uid"] = to_uid;
		}
		else {
			root["error"] = ErrorCodes::Uid_Invalid;
			return;
		}

		// 更新数据库: 更新friend_apply中status状态 + 添加到friend
		bool b_auth_and_add = MysqlMgr::GetInst()->AuthAndAddFriend(to_uid, uid, back_name); // 当前协议：to_uid 是申请人 ，uid 是同意者 
		if (!b_auth_and_add) {
			root["error"] = ErrorCodes::Database_Failed;
			return;
		}

		root["error"] = ErrorCodes::Success;

		// 查询redis, 查找to_uid对应的server_ip
		auto to_str = std::to_string(to_uid);
		auto to_ip_key = std::string(USERIPPREFIX) + to_str;
		std::string to_ip_value = "";
		bool b_ip = RedisMgr::GetInst()->Get(to_ip_key, to_ip_value);
		if (!b_ip) {
			return;
		}

		auto& cfg = ConfigMgr::GetInst();
		auto self_name = cfg["SelfServer"]["Name"];

		// 对方也在该服务器
		if (self_name == to_ip_value) {
			auto peer_sess = UserMgr::GetInst()->GetSession(to_uid);
			if (peer_sess) {
				// 在内存中则直接发送通知对方
				json notify;
				notify["error"] = ErrorCodes::Success;
				notify["from_uid"] = uid;
				notify["to_uid"] = to_uid;
				std::string base_key = std::string(USER_BASE_INFO) + std::to_string(uid);
				auto user_info = std::make_shared<UserInfo>();
				bool b_info = GetBaseInfo(base_key, uid, user_info);
				if (b_info) {
					notify["name"] = user_info->name;
					notify["nick"] = user_info->nick;
					notify["icon"] = user_info->icon;
					notify["sex"] = user_info->sex;
				}
				else {
					notify["error"] = ErrorCodes::Uid_Invalid;
				}

				std::string return_str = notify.dump();
				peer_sess->Send(return_str, ID_NOTIFY_AUTH_FRIEND_REQ);
			}

			return;
		}

		// 不在同一服务器
		AuthFriendReq auth_req;
		auth_req.set_from_uid(uid);
		auth_req.set_to_uid(to_uid);
		// gRPC发送通知
		ChatGrpcClient::GetInst()->NotifyAuthFriend(to_ip_value, auth_req);
}

void LogicSystem::HandleTextChatMsg(std::shared_ptr<CSession> sess, const short& msg_id, const std::string& msg_data)
{
		json root;
		Defer defer([this, sess, &root] {
			std::string jsonstr = root.dump();
			sess->Send(jsonstr, ID_TEXT_CHAT_MSG_RSP);
		});

		json src_root;
		try {
			src_root = json::parse(msg_data);
		}
		catch (const json::exception& e) {
			std::cerr << "Text Chat Msg Json parse error: " << e.what() << std::endl;
			root["error"] = ErrorCodes::Error_Json;
			return;
		}

		if (!src_root.is_object() ||
			!src_root.contains("to_uid") || !src_root["to_uid"].is_number_integer() ||
			!src_root.contains("text_array") || !src_root["text_array"].is_array()) {
			std::cerr << "Text Chat payload error: invalid top-level fields" << std::endl;
			root["error"] = ErrorCodes::Error_Json;
			return;
		}

		int uid = sess->GetUserId();
		if (uid <= 0) {
			root["error"] = ErrorCodes::Token_Invalid;
			return;
		}

		if (src_root.contains("from_uid") &&
			(!src_root["from_uid"].is_number_integer() ||
			 src_root["from_uid"].get<int>() != uid)) {
			root["error"] = ErrorCodes::Token_Invalid;
			return;
		}

		int to_uid = src_root["to_uid"].get<int>();
		if (to_uid <= 0 || to_uid == uid) {
			root["error"] = ErrorCodes::Uid_Invalid;
			return;
		}

		if (!MysqlMgr::GetInst()->AreFriends(uid, to_uid)) {
			root["error"] = ErrorCodes::Friend_Required;
			return;
		}

		const auto& arrays = src_root["text_array"];
		for (const auto& txt_obj : arrays) {
			if (!txt_obj.is_object() ||
				!txt_obj.contains("content") || !txt_obj["content"].is_string() ||
				!txt_obj.contains("unique_id") || !txt_obj["unique_id"].is_string()) {
				std::cerr << "Text Chat payload error: invalid text item" << std::endl;
				root["error"] = ErrorCodes::Error_Json;
				return;
			}
		}

		root["from_uid"] = uid;
		root["to_uid"] = to_uid;
		root["text_array"] = arrays;

		// 查询redis
		auto to_ip_key = std::string(USERIPPREFIX) + std::to_string(to_uid);
		std::string to_ip_value = "";
		bool b_ip = RedisMgr::GetInst()->Get(to_ip_key, to_ip_value);
		if (!b_ip) {
			root["error"] = ErrorCodes::Uid_Invalid;
			return;
		}

		auto& self_name = ConfigMgr::GetInst()["SelfServer"]["Name"];
		// 同一服务器
		if (to_ip_value == self_name) {
			auto peer_sess = UserMgr::GetInst()->GetSession(to_uid);
			if (peer_sess) {
				root["error"] = ErrorCodes::Success;
				std::string return_str = root.dump();
				peer_sess->Send(return_str, ID_NOTIFY_TEXT_CHAT_MSG_REQ);
			}
			else {
				root["error"] = ErrorCodes::Uid_Invalid;
			}
			return;
		}

		// 不在同一个服务器
		TextChatMsgReq text_msg_req;
		text_msg_req.set_from_uid(uid);
		text_msg_req.set_to_uid(to_uid);
		for (const auto& txt_obj : arrays) {
			const auto& content = txt_obj["content"].get_ref<const std::string&>();
			const auto& unique_id = txt_obj["unique_id"].get_ref<const std::string&>();

			auto* text_msg = text_msg_req.add_text_msgs();
			text_msg->set_unique_id(unique_id);
			text_msg->set_msg_content(content);
		}

		// 发送通知
		auto rsp = ChatGrpcClient::GetInst()->NotifyTextChatMsg(to_ip_value, text_msg_req);
		root["error"] = rsp.error();
}

void LogicSystem::CacheUserProfile(const std::shared_ptr<UserInfo>& user_info) {
	if (!user_info || user_info->uid <= 0 || user_info->name.empty()) {
		return;
	}

	json profile;
	profile["uid"] = user_info->uid;
	profile["name"] = user_info->name;
	profile["email"] = user_info->email;
	profile["nick"] = user_info->nick;
	profile["desc"] = user_info->desc;
	profile["sex"] = user_info->sex;
	profile["icon"] = user_info->icon;

	const auto profile_key = std::string(USER_BASE_INFO) + std::to_string(user_info->uid);
	if (!RedisMgr::GetInst()->SetWithExpire(profile_key, profile.dump(), _user_profile_ttl)) {
		return;
	}

	const auto name_key = std::string(NAME_INFO) + user_info->name;
	RedisMgr::GetInst()->SetWithExpire(name_key, std::to_string(user_info->uid), _user_profile_ttl);
}

bool LogicSystem::GetBaseInfo(const std::string& base_key, int uid, std::shared_ptr<UserInfo>& user_info) {
	std::string info_str;
	if (RedisMgr::GetInst()->Get(base_key, info_str) && !info_str.empty()) {
		try {
			const auto src_root = json::parse(info_str);
			auto cached = std::make_shared<UserInfo>();
			cached->uid = src_root.value("uid", uid);
			cached->name = src_root.value("name", std::string());
			cached->email = src_root.value("email", std::string());
			cached->nick = src_root.value("nick", std::string());
			cached->desc = src_root.value("desc", std::string());
			cached->sex = src_root.value("sex", 0);
			cached->icon = src_root.value("icon", std::string());

			if (cached->uid == uid && (!cached->name.empty() || !cached->email.empty())) {
				user_info = cached;
				return true;
			}
		}
		catch (const json::exception& e) {
			std::cerr << "user profile cache parse error: " << e.what() << std::endl;
		}
	}

	auto db_user = MysqlMgr::GetInst()->GetUser(uid);
	if (!db_user || (db_user->name.empty() && db_user->email.empty())) {
		return false;
	}

	user_info = db_user;
	CacheUserProfile(user_info);
	return true;
}

void LogicSystem::GetUserByUid(const std::string& uid_str, json& root) {
	if (!isPureDigit(uid_str)) {
		root["error"] = ErrorCodes::Uid_Invalid;
		return;
	}

	const int uid = std::stoi(uid_str);
	std::shared_ptr<UserInfo> user_info;
	const auto base_key = std::string(USER_BASE_INFO) + uid_str;
	if (!GetBaseInfo(base_key, uid, user_info)) {
		root["error"] = ErrorCodes::Uid_Invalid;
		return;
	}

	root["error"] = ErrorCodes::Success;
	root["uid"] = user_info->uid;
	root["name"] = user_info->name;
	root["email"] = user_info->email;
	root["nick"] = user_info->nick;
	root["desc"] = user_info->desc;
	root["sex"] = user_info->sex;
	root["icon"] = user_info->icon;
}

void LogicSystem::GetUserByName(const std::string& name, json& root) {
	const auto name_key = std::string(NAME_INFO) + name;
	std::string cached_uid;
	if (RedisMgr::GetInst()->Get(name_key, cached_uid) && isPureDigit(cached_uid)) {
		const int uid = std::stoi(cached_uid);
		std::shared_ptr<UserInfo> user_info;
		if (GetBaseInfo(std::string(USER_BASE_INFO) + cached_uid, uid, user_info) &&
			user_info->name == name) {
			root["error"] = ErrorCodes::Success;
			root["uid"] = user_info->uid;
			root["name"] = user_info->name;
			root["email"] = user_info->email;
			root["nick"] = user_info->nick;
			root["desc"] = user_info->desc;
			root["sex"] = user_info->sex;
			root["icon"] = user_info->icon;
			return;
		}
	}

	auto user_info = MysqlMgr::GetInst()->GetUser(name);
	if (!user_info || (user_info->name.empty() && user_info->email.empty())) {
		root["error"] = ErrorCodes::Uid_Invalid;
		return;
	}

	CacheUserProfile(user_info);
	root["error"] = ErrorCodes::Success;
	root["uid"] = user_info->uid;
	root["name"] = user_info->name;
	root["email"] = user_info->email;
	root["nick"] = user_info->nick;
	root["desc"] = user_info->desc;
	root["sex"] = user_info->sex;
	root["icon"] = user_info->icon;
}


bool LogicSystem::isPureDigit(const std::string& s) {
	// 空串和超长数字都不当作 uid, 避免 std::stoi 抛异常(异常会让这次请求没有任何回包)
	if (s.empty() || s.size() > 9) return false;
	for (char ch : s) {
		if (!isdigit(static_cast<unsigned char>(ch))) return false;
	}
	return true;
}

bool LogicSystem::GetFriendApplyInfo(int to_uid, std::vector<std::shared_ptr<ApplyInfo>>& user_list)
{
	// 从mysql获取好友申请列表
	return MysqlMgr::GetInst()->GetApplyList(to_uid, user_list, 0, 10);
}

bool LogicSystem::GetFriendList(int self_id, std::vector<std::shared_ptr<UserInfo>>& user_list)
{
	// 从mysql获取好友列表
	return MysqlMgr::GetInst()->GetFriendList(self_id, user_list);
}


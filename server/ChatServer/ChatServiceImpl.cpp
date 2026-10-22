#include "ChatServiceImpl.h"
#include "UserMgr.h"
#include "CSession.h"
#include "MysqlMgr.h"
#include "RedisMgr.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

ChatServiceImpl::ChatServiceImpl()
{

}

Status ChatServiceImpl::NotifyAddFriend(ServerContext* context, const AddFriendReq* request, AddFriendRsp* response)
{
	Defer defer([request, response]() {
		response->set_error(ErrorCodes::Success);
		response->set_apply_uid(request->apply_uid());
		response->set_to_uid(request->to_uid());
	});

	// 查找用户是否在本服务器
	auto to_uid = request->to_uid();
	auto session = UserMgr::GetInst()->GetSession(to_uid);
	if (!session) {
		return Status::OK;
	}

	// 在内存中则直接发送通知对方
	json rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["applyuid"] = request->apply_uid();
	rtvalue["name"] = request->name();
	rtvalue["desc"] = request->desc();
	rtvalue["icon"] = request->icon();
	rtvalue["sex"] = request->sex();
	rtvalue["nick"] = request->nick();

	std::string jsonstr = rtvalue.dump();
	session->Send(jsonstr, ID_NOTIFY_ADD_FRIEND_REQ);

	return Status::OK;
}

Status ChatServiceImpl::NotifyAuthFriend(ServerContext* context, const AuthFriendReq* request, AuthFriendRsp* response)
{
	Defer defer([request, response]() {
		response->set_error(ErrorCodes::Success);
		response->set_from_uid(request->from_uid());
		response->set_to_uid(request->to_uid());
	});

	// 查找用户是否在本服务器
	auto to_uid = request->to_uid();
	auto from_uid = request->from_uid();
	auto sess = UserMgr::GetInst()->GetSession(to_uid);
	if (sess == nullptr) {
		return Status::OK;
	}

	// 在内存中直接通知对方
	json rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["from_uid"] = from_uid;
	rtvalue["to_uid"] = to_uid;

	std::string base_key = std::string(USER_BASE_INFO) + std::to_string(from_uid);
	auto user_info = std::make_shared<UserInfo>();
	bool b_info = GetBaseInfo(base_key, from_uid, user_info);
	if (b_info) {
		rtvalue["name"] = user_info->name;
		rtvalue["nick"] = user_info->nick;
		rtvalue["icon"] = user_info->icon;
		rtvalue["sex"] = user_info->sex;
	}
	else {
		rtvalue["error"] = ErrorCodes::Uid_Invalid;
	}

	std::string return_value = rtvalue.dump();
	sess->Send(return_value, ID_NOTIFY_AUTH_FRIEND_REQ);
	return Status::OK;
}

Status ChatServiceImpl::NotifyTextChatMsg(ServerContext* context, const TextChatMsgReq* request, TextChatMsgRsp* response)
{
	// 查找是否在服务器
	auto to_uid = request->to_uid();
	auto sess = UserMgr::GetInst()->GetSession(to_uid);
	response->set_error(ErrorCodes::Success);

	// 不在内存则直接返回
	if (sess == nullptr) {
		response->set_error(ErrorCodes::Uid_Invalid);
		return Status::OK;
	}

	// 在内存中则直接通知对方
	json rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["from_uid"] = request->from_uid();
	rtvalue["to_uid"] = request->to_uid();

	// 将聊天数据组织为数组
	json text_array = json::array();
	for (auto& msg : request->text_msgs()) {
		json element;
		element["content"] = msg.msg_content();
		element["unique_id"] = msg.unique_id();
		text_array.push_back(element);
	}
	rtvalue["text_array"] = text_array;

	std::string return_str = rtvalue.dump();
	sess->Send(return_str, ID_NOTIFY_TEXT_CHAT_MSG_REQ);

	return Status::OK;
}

bool ChatServiceImpl::GetBaseInfo(const std::string& base_key, int uid, std::shared_ptr<UserInfo>& user_info) {
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

void ChatServiceImpl::RegisterServer(std::shared_ptr<CServer> p_server)
{

}


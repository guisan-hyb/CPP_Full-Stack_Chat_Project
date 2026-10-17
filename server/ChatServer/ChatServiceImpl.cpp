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
}

Status ChatServiceImpl::NotifyAuthFriend(ServerContext* context, const AuthFriendReq* request, AuthFriendRsp* response)
{
	return Status();
}

Status ChatServiceImpl::NotifyTextChatMsg(ServerContext* context, const TextChatMsgReq* request, TextChatMsgRsp* response)
{
	return Status();
}

bool ChatServiceImpl::GetBaseInfo(const std::string& base_key, int uid, std::shared_ptr<UserInfo>& user_info) {
	return false;
}

void ChatServiceImpl::RegisterServer(std::shared_ptr<CServer> p_server)
{

}


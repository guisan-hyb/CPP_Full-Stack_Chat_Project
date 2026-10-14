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
	return Status();
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


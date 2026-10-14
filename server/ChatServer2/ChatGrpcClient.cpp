#include "ChatGrpcClient.h"
#include "ConfigMgr.h"
#include "RedisMgr.h"
#include "MysqlMgr.h"
#include "UserMgr.h"
#include "CSession.h"


ChatGrpcClient::ChatGrpcClient() {
	auto& cfg = ConfigMgr::GetInst();
	auto server_list = cfg["PeerServer"]["Servers"];

	std::stringstream ss(server_list);
	std::vector<std::string> server_name;
	std::string str;

	while (std::getline(ss, str, ',')) {
		server_name.push_back(str);
	}

	for (auto& server : server_name) {
		if (cfg[server]["Name"].empty()) continue;

		_pools[cfg[server]["Name"]] = std::make_unique<ChatConnPool>(5, cfg[server]["Host"], cfg[server]["Port"]);
	}
}

AddFriendRsp ChatGrpcClient::NotifyAddFriend(const std::string& server_ip, const AddFriendReq& req)
{
	return AddFriendRsp();
}

AuthFriendRsp ChatGrpcClient::NotifyAuthFriend(const std::string& server_ip, const AuthFriendReq& req)
{
	return AuthFriendRsp();
}

TextChatMsgRsp ChatGrpcClient::NotifyTextChatMsg(const std::string& server_ip, const TextChatMsgReq& req, const json& rtvalue)
{
	return TextChatMsgRsp();
}

bool ChatGrpcClient::GetBaseInfo(const std::string& base_key, int uid, std::shared_ptr<UserInfo>& user_info)
{
	return false;
}


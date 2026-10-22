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
	AddFriendRsp rsp;

	rsp.set_apply_uid(req.apply_uid());
	rsp.set_to_uid(req.to_uid());
	rsp.set_error(ErrorCodes::Success);// 设为默认值，后续出错直接覆盖

	// 查找发送目标的连接池
	auto find_iter = _pools.find(server_ip);
	if (find_iter == _pools.end()) {
		rsp.set_error(ErrorCodes::RPC_Failed);
		return rsp;
	}

	auto& pool = find_iter->second;
	ClientContext context;
	auto stub = pool->GetConnection();
	Status status = stub->NotifyAddFriend(&context, req, &rsp);

	Defer defer_process_returnSource([&stub, &pool, this]() {
		pool->ReturnConnection(std::move(stub));
	});

	if (!status.ok()) {
		rsp.set_error(ErrorCodes::RPC_Failed);
		return rsp;
	}

	return rsp;
}

AuthFriendRsp ChatGrpcClient::NotifyAuthFriend(const std::string& server_ip, const AuthFriendReq& req)
{
	AuthFriendRsp rsp;
	rsp.set_from_uid(req.from_uid());
	rsp.set_to_uid(req.to_uid());
	rsp.set_error(ErrorCodes::Success);

	auto find_iter = _pools.find(server_ip);
	if (find_iter == _pools.end()) {
		return rsp;
	}

	auto& pool = find_iter->second;
	ClientContext context;
	auto stub = pool->GetConnection();
	Status status = stub->NotifyAuthFriend(&context, req, &rsp);
	
	Defer defer_process_returnSource([&stub, &pool, this]() {
		pool->ReturnConnection(std::move(stub));
	});

	if (!status.ok()) {
		rsp.set_error(ErrorCodes::RPC_Failed);
	}

	return rsp;
}

TextChatMsgRsp ChatGrpcClient::NotifyTextChatMsg(const std::string& server_ip, const TextChatMsgReq& req)
{
	TextChatMsgRsp rsp;
	rsp.set_error(ErrorCodes::Success);
	rsp.set_from_uid(req.from_uid());
	rsp.set_to_uid(req.to_uid());
	for (const auto& text_data : req.text_msgs()) {
		TextChatData* new_msg = rsp.add_text_msgs();
		new_msg->set_unique_id(text_data.unique_id());
		new_msg->set_msg_content(text_data.msg_content());
	}

	auto iter = _pools.find(server_ip);
	if (iter == _pools.end()) {
		rsp.set_error(ErrorCodes::RPC_Failed);
		return rsp;
	}

	auto& pool = iter->second;
	ClientContext context;
	auto stub = pool->GetConnection();
	Status status = stub->NotifyTextChatMsg(&context, req, &rsp);

	Defer defer_process_returnSource([&stub, &pool, this]() {
		pool->ReturnConnection(std::move(stub));
	});

	if (!status.ok()) {
		rsp.set_error(ErrorCodes::RPC_Failed);
	}

	return rsp;
}

bool ChatGrpcClient::GetBaseInfo(const std::string& base_key, int uid, std::shared_ptr<UserInfo>& user_info)
{
	return false;
}


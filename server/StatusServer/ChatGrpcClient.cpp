#include "ChatGrpcClient.h"
#include "ConfigMgr.h"
#include "MysqlMgr.h"
#include "RedisMgr.h"


AddFriendRsp ChatGrpcClient::NotifyAddFriend(const AddFriendReq& req)
{
	auto to_uid = req.to_uid();
	std::string uit_str = std::to_string(to_uid);

	AddFriendRsp rsp;
	return rsp;
}

ChatGrpcClient::ChatGrpcClient() {
	auto& cfg = ConfigMgr::GetInst();
	auto server_list = cfg["chatservers"]["Name"];

	std::string word;
	std::stringstream ss(server_list);
	std::vector<std::string> servers;

	while (std::getline(ss, word, ',')) {
		servers.push_back(word);
	}

	for (auto& server : servers) {
		if (cfg[server]["Name"].empty()) {
			continue;
		}

		_pools[cfg[server]["Name"]] = std::make_unique<ChatConnPool>(5, cfg[server]["Host"], cfg[server]["Port"]);
	}
}


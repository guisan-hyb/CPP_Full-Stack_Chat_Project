#include "StatusServiceImpl.h"
#include <boost/uuid/uuid.hpp>            // uuid 类的定义
#include <boost/uuid/uuid_generators.hpp> // random_generator 的定义
#include <boost/uuid/uuid_io.hpp>        // 如果需要将 uuid 转成字符串输出，需要这个
#include "ConfigMgr.h"
#include "RedisMgr.h"

// 辅助函数：生成唯一token
std::string generate_unique_string() {
	boost::uuids::uuid uuid = boost::uuids::random_generator()(); // 创建UUID对象
	std::string unique_string = boost::uuids::to_string(uuid);// 将UUID转换为字符串
	return unique_string;
}

StatusServiceImpl::StatusServiceImpl()
{
	auto& cfg = ConfigMgr::GetInst();
	auto server_list = cfg["chatservers"]["Name"];
	std::vector<std::string> words;

	std::stringstream ss(server_list);
	std::string word;
	while (std::getline(ss, word, ',')) {
		words.push_back(word);
	}

	for (auto& word : words) {
		if (cfg[word]["Name"].empty()) continue;

		ChatServer server;
		server.host = cfg[word]["Host"];
		server.port = cfg[word]["Port"];
		server.name = cfg[word]["Name"];
		_servers[server.name] = server;
	}
}

Status StatusServiceImpl::GetChatServer(ServerContext* context, const GetChatServerReq* request, GetChatServerRsp* response)
{
	const auto& server = getChatServer();
	response->set_host(server.host);
	response->set_port(server.port);
	response->set_error(ErrorCodes::Success);
	response->set_token(generate_unique_string());
	insertToken(request->uid(), response->token());
	return Status::OK;
}

Status StatusServiceImpl::Login(ServerContext* context, const LoginReq* request, LoginRsp* response)
{
	auto uid = request->uid();
	auto token = request->token();

	std::string uid_str = std::to_string(uid);
	std::string token_key = std::string(USERTOKENPREFIX) + uid_str;
	std::string token_val = "";

	bool success = RedisMgr::GetInst()->Get(token_key, token_val);
	if (!success) {
		response->set_error(ErrorCodes::Uid_Invalid);
		return Status::OK;
	}

	if (token != token_val) {
		response->set_error(ErrorCodes::Token_Invalid);
		return Status::OK;
	}

	response->set_error(ErrorCodes::Success);
	response->set_uid(uid);
	response->set_token(token);

	return Status::OK;
}

void StatusServiceImpl::insertToken(int uid, const std::string& token)
{
	std::string uid_str = std::to_string(uid);
	std::string token_key = std::string(USERTOKENPREFIX) + uid_str;
	RedisMgr::GetInst()->Set(token_key, token);
}

ChatServer StatusServiceImpl::getChatServer()
{
	std::lock_guard<std::mutex> lk(_server_mtx);
	auto minServer = _servers.begin()->second;
	auto count_str = RedisMgr::GetInst()->HGet(LOGIN_COUNT, minServer.name);
	if (count_str.empty()) {
		// 不存在默认设置成最大
		minServer.conn_count = INT_MAX;
	}
	else {
		minServer.conn_count = std::stoi(count_str);
	}

	for (auto& server : _servers) {
		if (server.second.name == minServer.name) continue;

		auto cur_count_str = RedisMgr::GetInst()->HGet(LOGIN_COUNT, server.second.name);
		if (cur_count_str.empty()) {
			server.second.conn_count = INT_MAX;
		}
		else {
			server.second.conn_count = std::stoi(cur_count_str);
		}

		if (server.second.conn_count < minServer.conn_count) {
			minServer = server.second;
		}
	}

	return minServer;
}



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
		server.conn_count = 0;
		_servers[server.name] = server;
	}
}

Status StatusServiceImpl::GetChatServer(ServerContext* context, const GetChatServerReq* request, GetChatServerRsp* response)
{
	std::cout << "status server has received: " << std::endl;
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

	std::lock_guard<std::mutex> lk(_token_mtx);
	auto iter = _tokens.find(uid);
	if (iter == _tokens.end()) {
		response->set_error(ErrorCodes::Uid_Invalid);
		return Status::OK;
	}

	if (iter->second != token) {
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
	std::lock_guard<std::mutex> lk(_token_mtx);
	_tokens[uid] = token;
}

ChatServer StatusServiceImpl::getChatServer()
{
	std::lock_guard<std::mutex> lk(_server_mtx);
	auto minServer = _servers.begin()->second;
	
	for (auto& server : _servers) {
		if (server.second.conn_count < minServer.conn_count) {
			minServer = server.second;
		}
	}

	return minServer;
}



#pragma once

#include "const.h"
#include "Singleton.h"
#include <message.grpc.pb.h>
#include <message.pb.h>
#include <grpcpp/grpcpp.h>
#include "data.h"
#include <nlohmann/json.hpp>
#include <queue>


using json = nlohmann::json;

using grpc::Channel;
using grpc::Status;
using grpc::ClientContext;

using message::AddFriendReq;
using message::AddFriendRsp;

using message::AuthFriendReq;
using message::AuthFriendRsp;

using message::GetChatServerRsp;
using message::LoginReq;
using message::LoginRsp;
using message::ChatService;

using message::TextChatMsgReq;
using message::TextChatMsgRsp;
using message::TextChatData;


class ChatConnPool {
public:
	ChatConnPool(int pool_size, std::string host, std::string port)
		: _pool_size(pool_size), _host(host), _port(port), _b_stop(false) {
		for (int i = 0; i < pool_size; i++) {
			std::shared_ptr<Channel> channel = grpc::CreateChannel(host + ":" + port, grpc::InsecureChannelCredentials());
			_connections.push(ChatService::NewStub(channel));
		}
	}

	~ChatConnPool() {
		std::lock_guard<std::mutex> lk(_mtx);
		Close();
		while (!_connections.empty()) {
			_connections.pop();
		}
	}

	void Close() {
		_b_stop.store(true);
		_cond.notify_all();
	}

	std::unique_ptr<ChatService::Stub> GetConnection() {
		std::unique_lock<std::mutex> ulk(_mtx);
		_cond.wait(ulk, [this]() {
			return _b_stop || !_connections.empty();
		});

		if (_b_stop) {
			return nullptr;
		}

		auto context = std::move(_connections.front());
		_connections.pop();

		return context;
	}

	void ReturnConnection(std::unique_ptr<ChatService::Stub> context) {
		std::lock_guard<std::mutex> lk(_mtx);
		if (_b_stop) return;

		_connections.push(std::move(context));
		_cond.notify_one();
	}

private:
	std::atomic<bool> _b_stop;
	int _pool_size;
	std::string _host;
	std::string _port;
	std::queue<std::unique_ptr<ChatService::Stub>> _connections;
	std::mutex _mtx;
	std::condition_variable _cond;
};



class ChatGrpcClient : public Singleton<ChatGrpcClient>
{
	friend class Singleton<ChatGrpcClient>;
public:
	~ChatGrpcClient() = default;
	AddFriendRsp NotifyAddFriend(const std::string& server_ip, const AddFriendReq& req);
	AuthFriendRsp NotifyAuthFriend(const std::string& server_ip, const AuthFriendReq& req);
	TextChatMsgRsp NotifyTextChatMsg(const std::string& server_ip, const TextChatMsgReq& req, const json& rtvalue);
	bool GetBaseInfo(const std::string& base_key, int uid, std::shared_ptr<UserInfo>& user_info);

private:
	ChatGrpcClient();
	std::unordered_map<std::string, std::unique_ptr<ChatConnPool>> _pools;
};


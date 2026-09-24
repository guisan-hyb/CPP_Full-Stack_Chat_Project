#pragma once

#include <grpcpp/grpcpp.h>
#include "message.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using message::GetChatServerReq;
using message::GetChatServerRsp;
using message::LoginReq;
using message::LoginRsp;
using message::StatusService;


class ChatServer {
public:
	ChatServer(): host(""), port(""), name(""), conn_count(0) {}
	ChatServer(const ChatServer& cs): host(cs.host), port(cs.port), name(cs.name), conn_count(cs.conn_count) {}
	ChatServer& operator=(const ChatServer& cs) {
		if (this == &cs) {
			return *this;
		}

		host = cs.host;
		port = cs.port;
		name = cs.name;
		conn_count = cs.conn_count;
		return *this;
	}

	std::string host;
	std::string port;
	std::string name;
	int conn_count;
};


class StatusServiceImpl final : public StatusService::Service
{
public:
	StatusServiceImpl();
	Status GetChatServer(ServerContext* context, const GetChatServerReq* request, GetChatServerRsp* response) override;
	Status Login(ServerContext* context, const LoginReq* request, LoginRsp* response) override;

private:
	void insertToken(int uid, const std::string& token);
	ChatServer getChatServer();

private:
	std::unordered_map<std::string, ChatServer> _servers; // 也可以用堆实现
	std::mutex _server_mtx;
};


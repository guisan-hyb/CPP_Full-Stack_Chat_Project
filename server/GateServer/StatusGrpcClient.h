#pragma once

#include "const.h"
#include "Singleton.h"
#include <grpcpp/grpcpp.h>
#include <message.grpc.pb.h>

using grpc::Channel;
using grpc::Status;
using grpc::ClientContext;

using message::GetChatServerReq;
using message::GetChatServerRsp;
using message::LoginReq;
using message::LoginRsp;
using message::StatusService;

class StatusConnectionPool {
public:
	StatusConnectionPool(std::size_t pool_size, std::string host, std::string port);
	~StatusConnectionPool();

	std::unique_ptr<StatusService::Stub> GetConnection();
	void ReturnConnection(std::unique_ptr<StatusService::Stub> context);
	void Close();

private:
	std::atomic<bool> _b_stop;
	std::size_t _pool_size;
	std::string _host;
	std::string _port;
	std::queue<std::unique_ptr<StatusService::Stub>> _connections;
	std::mutex _mtx;
	std::condition_variable _cond;
};



class StatusGrpcClient : public Singleton<StatusGrpcClient>
{
	friend class Singleton<StatusGrpcClient>;
public:
	~StatusGrpcClient();
	GetChatServerRsp GetChatServer(int uid);
	LoginRsp Login(int uid, std::string token);

private:
	StatusGrpcClient();
	std::unique_ptr<StatusConnectionPool> _pool;
};


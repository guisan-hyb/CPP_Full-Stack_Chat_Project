#pragma once

#include <grpcpp/grpcpp.h>
#include "const.h"
#include "Singleton.h"
#include <message.grpc.pb.h>

using grpc::Channel;// 客户端与服务器之间的网络连接抽象
using grpc::Status;// RPC 调用结束后，服务器会返回这个状态对象
using grpc::ClientContext;// 客户端发起调用时携带额外的元数据（Metadata），比如超时时间（set_deadline）、认证 Token（JWT）等。客户端和服务端都可以读写它。

using message::GetVerifyReq;
using message::GetVerifyRsp;
using message::VerifyService;


class RPC_connect_pool {
public:
	RPC_connect_pool(std::size_t pool_size, std::string host, std::string port);
	~RPC_connect_pool();

	void Close();
	std::unique_ptr<VerifyService::Stub> GetConnection();
	void ReturnConnection(std::unique_ptr<VerifyService::Stub> context);

private:
	std::atomic<bool> _b_stop;// 标记是否回收
	std::size_t _pool_size;
	std::string _host;
	std::string _port;

	std::queue<std::unique_ptr<VerifyService::Stub>> _connections;
	std::mutex _mtx;
	std::condition_variable _cond;
};


class VerifyGrpcClient : public Singleton<VerifyGrpcClient>
{
	friend class Singleton<VerifyGrpcClient>;
public:
	// 把email发给对方并获得回包
	GetVerifyRsp GetVerifyCode(std::string email);

private:
	VerifyGrpcClient();

private:
	std::unique_ptr<RPC_connect_pool> _pool;
};


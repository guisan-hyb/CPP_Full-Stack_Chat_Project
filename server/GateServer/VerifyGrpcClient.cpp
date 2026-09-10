#include "VerifyGrpcClient.h"
#include "ConfigMgr.h"

GetVerifyRsp VerifyGrpcClient::GetVerifyCode(std::string email) {
	ClientContext context;
	GetVerifyReq request;
	GetVerifyRsp reply;

	request.set_email(email);
	auto stub = _pool->GetConnection();
	// 防止空指针解引用崩溃
	if (!stub) {
		reply.set_error(ErrorCodes::RPC_Failed);
		return reply;
	}

	Status status = stub->GetVerifyCode(&context, request, &reply);
	if (status.ok()) {
		_pool->ReturnConnection(std::move(stub));
		return reply;
	}
	else {
		_pool->ReturnConnection(std::move(stub));
		reply.set_error(ErrorCodes::RPC_Failed);
		return reply;
	}
}


VerifyGrpcClient::VerifyGrpcClient() {
	auto& gCfgMgr = ConfigMgr::GetInst();
	std::string host = gCfgMgr["VerifyServer"]["Host"];
	std::string port = gCfgMgr["VerifyServer"]["Port"];
	_pool.reset(new RPC_connect_pool(5, host, port));
}




RPC_connect_pool::RPC_connect_pool(std::size_t pool_size, std::string host, std::string port)
	: _pool_size(pool_size), _host(host), _port(port), _b_stop(false)
{
	for (std::size_t i = 0; i < _pool_size; i++) {
		std::shared_ptr<Channel> channel = grpc::CreateChannel(host + ":" + port,
			grpc::InsecureChannelCredentials());
		_connections.push(VerifyService::NewStub(channel));
	}
}

RPC_connect_pool::~RPC_connect_pool()
{
	std::lock_guard<std::mutex> lk(_mtx);
	Close();
	while (!_connections.empty()) {
		_connections.pop();
	}
}

void RPC_connect_pool::Close() {
	_b_stop.store(true);
	_cond.notify_all();
}

std::unique_ptr<VerifyService::Stub> RPC_connect_pool::GetConnection()
{
	std::unique_lock<std::mutex> ulk(_mtx);
	_cond.wait(ulk, [this]() {
		if (_b_stop) return true;
		return !_connections.empty();
	});

	if (_b_stop) return nullptr;

	auto context = std::move(_connections.front());
	_connections.pop();
	return context;
}

void RPC_connect_pool::ReturnConnection(std::unique_ptr<VerifyService::Stub> context)
{
	std::lock_guard<std::mutex> lk(_mtx);
	if (_b_stop) return;

	_connections.push(std::move(context));
	_cond.notify_one();
}


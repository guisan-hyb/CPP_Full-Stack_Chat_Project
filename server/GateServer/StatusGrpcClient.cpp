#include "StatusGrpcClient.h"
#include "ConfigMgr.h"

StatusConnectionPool::StatusConnectionPool(std::size_t pool_size, std::string host, std::string port)
	: _pool_size(pool_size), _host(host), _port(port)
{
	for (std::size_t i = 0; i < pool_size; i++) {
		std::shared_ptr<Channel> channel = grpc::CreateChannel(host + ":" + port, grpc::InsecureChannelCredentials());
		_connections.push(StatusService::NewStub(channel));
	}
}

StatusConnectionPool::~StatusConnectionPool()
{
	std::lock_guard<std::mutex> lk(_mtx);
	Close();
	while (!_connections.empty()) {
		_connections.pop();
	}
}

std::unique_ptr<StatusService::Stub> StatusConnectionPool::GetConnection() {
	std::unique_lock<std::mutex> ulk(_mtx);
	_cond.wait(ulk, [this]() {
		return _b_stop || !_connections.empty();
	});

	if (_b_stop) return nullptr;

	auto context = std::move(_connections.front());
	_connections.pop();
	return context;
}

void StatusConnectionPool::ReturnConnection(std::unique_ptr<StatusService::Stub> context) {
	std::lock_guard<std::mutex> lk(_mtx);
	if (_b_stop) return;

	_connections.push(std::move(context));
	_cond.notify_one();
}

void StatusConnectionPool::Close() {
	_b_stop = true;
	_cond.notify_all();
}




StatusGrpcClient::~StatusGrpcClient()
{

}

GetChatServerRsp StatusGrpcClient::GetChatServer(int uid)
{
	ClientContext context;
	GetChatServerReq request;
	GetChatServerRsp response;

	request.set_uid(uid);
	auto stub = _pool->GetConnection();
	Status status = stub->GetChatServer(&context, request, &response);

	if (status.ok()) {
		_pool->ReturnConnection(std::move(stub));
		return response;
	}
	else {
		_pool->ReturnConnection(std::move(stub));
		response.set_error(ErrorCodes::RPC_Failed);
		return response;
	}
}

LoginRsp StatusGrpcClient::Login(int uid, std::string token)
{
	ClientContext context;
	LoginReq request;
	LoginRsp response;

	request.set_uid(uid);
	request.set_token(token);
	auto stub = _pool->GetConnection();
	Status status = stub->Login(&context, request, &response);

	if (status.ok()) {
		_pool->ReturnConnection(std::move(stub));
		return response;
	}
	else {
		_pool->ReturnConnection(std::move(stub));
		response.set_error(ErrorCodes::RPC_Failed);
		return response;
	}
}

StatusGrpcClient::StatusGrpcClient() {
	auto& gCfgMgr = ConfigMgr::GetInst();
	std::string host = gCfgMgr["StatusServer"]["Host"];
	std::string port = gCfgMgr["StatusServer"]["Port"];
	_pool.reset(new StatusConnectionPool(5, host, port));
}


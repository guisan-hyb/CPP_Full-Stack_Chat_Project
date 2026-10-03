#include "StatusGrpcClient.h"
#include "ConfigMgr.h"
#include "const.h"

StatusGrpcClient::StatusGrpcClient() {
	auto& cfg = ConfigMgr::GetInst();
	auto& host = cfg["StatusServer"]["Host"];
	auto& port = cfg["StatusServer"]["Port"];
	_pool.reset(new StatusConnectionPool(5, host, port));
}

StatusGrpcClient::~StatusGrpcClient() {

}

GetChatServerRsp StatusGrpcClient::GetChatServer(int uid) {
	ClientContext context;
	GetChatServerReq request;
	GetChatServerRsp response;

	request.set_uid(uid);
	auto stub = _pool->GetConnection();
	Status status = stub->GetChatServer(&context, request, &response);

	Defer defer([&stub, this]() {
		_pool->ReturnConnection(std::move(stub));
	});

	if (status.ok()) {
		return response;
	}
	else {
		response.set_error(ErrorCodes::RPC_Failed);
		return response;
	}
}


LoginRsp StatusGrpcClient::Login(int uid, const std::string& token) {
	ClientContext context;
	LoginReq request;
	LoginRsp response;

	request.set_uid(uid);
	request.set_token(token);

	auto stub = _pool->GetConnection();
	Status status = stub->Login(&context, request, &response);

	Defer defer([&stub, this]() {
		_pool->ReturnConnection(std::move(stub));
	});

	if (status.ok()) {
		return response;
	}
	else {
		response.set_error(ErrorCodes::RPC_Failed);
		return response;
	}
}


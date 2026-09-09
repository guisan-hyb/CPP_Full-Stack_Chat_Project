#include "VerifyGrpcClient.h"


GetVerifyRsp VerifyGrpcClient::GetVerifyCode(std::string email) {
	ClientContext context;
	GetVerifyReq request;
	GetVerifyRsp reply;

	request.set_email(email);
	Status status = _stub->GetVerifyCode(&context, request, &reply);
	if (status.ok()) {
		return reply;
	}
	else {
		reply.set_error(ErrorCodes::RPC_Failed);
		return reply;
	}
}


VerifyGrpcClient::VerifyGrpcClient() {
	std::shared_ptr<Channel> channel = grpc::CreateChannel("0.0.0.0:50051",
		grpc::InsecureChannelCredentials());
	_stub = VerifyService::NewStub(channel);
}


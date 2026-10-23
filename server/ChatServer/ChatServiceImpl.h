#pragma once

#include <chrono>
#include <grpcpp/grpcpp.h>
#include <message.grpc.pb.h>
#include <message.pb.h>
#include <mutex>
#include "data.h"
#include "CServer.h"
#include <memory>


using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using message::AddFriendReq;
using message::AddFriendRsp;

using message::AuthFriendReq;
using message::AuthFriendRsp;

using message::ChatService;
using message::TextChatData;
using message::TextChatMsgReq;
using message::TextChatMsgRsp;

using message::KickUserReq;
using message::KickUserRsp;


class ChatServiceImpl final : public ChatService::Service
{
public:
	ChatServiceImpl();
    Status NotifyAddFriend(ServerContext* context, const AddFriendReq* request, AddFriendRsp* response) override;
    Status NotifyAuthFriend(ServerContext* context, const AuthFriendReq* request, AuthFriendRsp* response) override;
    Status NotifyTextChatMsg(ServerContext* context, const TextChatMsgReq* request, TextChatMsgRsp* response) override;
    bool GetBaseInfo(const std::string& base_key, int uid, std::shared_ptr<UserInfo>& user_info);
    void RegisterServer(std::shared_ptr<CServer> p_server);
    
private:
	void CacheUserProfile(const std::shared_ptr<UserInfo>& user_info);
	std::shared_ptr<CServer> _p_server;
	std::chrono::seconds _user_profile_ttl{600};
};

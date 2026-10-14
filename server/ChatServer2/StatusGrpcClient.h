#pragma once

#include "Singleton.h"
#include <grpcpp/grpcpp.h>
#include <message.grpc.pb.h>
#include <atomic>
#include <mutex>
#include <queue>
#include <condition_variable>

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
	StatusConnectionPool(std::size_t pool_size, const std::string& host, const std::string& port)
		: _pool_size(pool_size), _host(host), _port(port)
	{
		for (std::size_t i = 0; i < _pool_size; i++) {
			std::shared_ptr<Channel> channel = grpc::CreateChannel(host + ":" + port, grpc::InsecureChannelCredentials());
			_connections.push(StatusService::NewStub(channel));
		}
	}

	~StatusConnectionPool() {
		std::lock_guard<std::mutex> lk(_mtx);
		Close();
		while (!_connections.empty()) {
			_connections.pop();
		}
	}

	void Close() {
		_b_stop = true;
		_cond.notify_all();
	}

	std::unique_ptr<StatusService::Stub> GetConnection() {
		std::unique_lock<std::mutex> ulk(_mtx);
		_cond.wait(ulk, [this]() {
			return _b_stop || !_connections.empty();
		});

		if (_b_stop) return nullptr;

		auto context = std::move(_connections.front());
		_connections.pop();
		return context;
	}

	void ReturnConnection(std::unique_ptr<StatusService::Stub> context) {
		std::lock_guard<std::mutex> lk(_mtx);
		if (_b_stop) return;

		_connections.push(std::move(context));
		_cond.notify_one();
	}

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
	LoginRsp Login(int uid, const std::string& token);

private:
	StatusGrpcClient();
	std::unique_ptr<StatusConnectionPool> _pool;
};


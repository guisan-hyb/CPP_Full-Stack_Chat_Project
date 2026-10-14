#include "ConfigMgr.h"
#include "CServer.h"
#include "AsioIOServicePool.h"
#include <iostream>
#include "RedisMgr.h"
#include "const.h"
#include "ChatServiceImpl.h"

int main() {
	auto& cfg = ConfigMgr::GetInst();
	auto server_name = cfg["SelfServer"]["Name"];

	try {
		auto pool = AsioIOServicePool::GetInst();
		// 将登录数设置为 0 
		RedisMgr::GetInst()->HSet(std::string(LOGIN_COUNT), server_name, "0");

		// 定义一个GrpcServer
		std::string server_address(cfg["SelfServer"]["Host"] + ":" + cfg["SelfServer"]["RPCPort"]);
		ChatServiceImpl service;
		ServerBuilder builder;
		// 监听端口和添加服务
		builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
		builder.RegisterService(&service);
		// 构建并启动gRPC服务器
		std::unique_ptr<Server> server(builder.BuildAndStart());
		std::cout << "RPC Server listening on: " << server_address << std::endl;

		// 单独一个线程处理grpc服务
		std::thread grpc_server_thread([&server]() {
			server->Wait();
			});

		net::io_context ioc;
		net::signal_set signals(ioc, SIGINT, SIGTERM);
		signals.async_wait([&ioc, pool, &server](auto, auto) {
			ioc.stop();
			pool->Stop();
			server->Shutdown();
			});

		auto port_str = cfg["SelfServer"]["Port"];
		CServer s(ioc, std::stoi(port_str));

		ioc.run();

		RedisMgr::GetInst()->HDel(std::string(LOGIN_COUNT), server_name);
		RedisMgr::GetInst()->Close();
		grpc_server_thread.join();
	}
	catch (std::exception& e) {
		std::cerr << "Exception: " << e.what() << std::endl;
		RedisMgr::GetInst()->HDel(std::string(LOGIN_COUNT), server_name);
		RedisMgr::GetInst()->Close();
	}

	return 0;
}


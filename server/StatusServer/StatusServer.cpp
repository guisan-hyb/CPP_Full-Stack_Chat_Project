#include "const.h"
#include "RedisMgr.h"
#include "MysqlMgr.h"
#include "ConfigMgr.h"
#include "StatusServiceImpl.h"

void RunServer() {
	auto& cfg = ConfigMgr::GetInst();

	std::string server_address(cfg["StatusServer"]["Host"] + ":" + cfg["StatusServer"]["Port"]);
	StatusServiceImpl service;

	grpc::ServerBuilder builder;
	// 监听端口和添加服务
	builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
	builder.RegisterService(&service);

	// 构建并启动gRPC服务器
	std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
	std::cout << "Server listening on" << server_address << std::endl;

	net::io_context ioc;
	net::signal_set signals(ioc, SIGINT, SIGTERM);

	signals.async_wait([&server, &ioc](boost::system::error_code ec, int signal_number) {
		if (!ec) {
			std::cout << "Shutting down server..." << std::endl;
			server->Shutdown();
			ioc.stop();
		}
	});

	// 在单独的线程中运行io_context
	std::thread([&ioc]() {ioc.run(); }).detach();

	// 等待服务器关闭
	server->Wait();
}

int main() {
	try {
		RunServer();
		RedisMgr::GetInst()->Close();
	}
	catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		RedisMgr::GetInst()->Close();
		return EXIT_FAILURE;
	}

	return 0;
}


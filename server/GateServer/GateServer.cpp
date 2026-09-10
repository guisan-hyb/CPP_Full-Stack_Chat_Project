#include "CServer.h"
#include "ConfigMgr.h"

int main() {
	auto& gCfgMgr = ConfigMgr::GetInst();
	std::string gate_port_str = gCfgMgr["GateServer"]["Port"];
	unsigned short gate_port = std::stoi(gate_port_str);
	try {
		unsigned short port = gate_port;
		net::io_context ioc{ 1 };
		net::signal_set signals(ioc, SIGINT, SIGTERM);
		signals.async_wait([&ioc](boost::system::error_code error, int signal_number) {
			if (error) return;
			ioc.stop();
		});

		std::make_shared<CServer>(ioc, port)->Start();
		std::cout << "Gate Server listen on port: " << port << std::endl;
		ioc.run();
	}
	catch (std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return 0;
}


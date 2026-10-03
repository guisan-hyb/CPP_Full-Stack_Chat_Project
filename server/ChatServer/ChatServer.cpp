#include "ConfigMgr.h"
#include "CServer.h"
#include "AsioIOServicePool.h"

int main() {
	auto& cfg_mgr = ConfigMgr::GetInst();
	auto pool = AsioIOServicePool::GetInst();

	net::io_context ioc;
	net::signal_set signals(ioc, SIGINT, SIGTERM);
	signals.async_wait([&ioc, pool](auto, auto) {
		ioc.stop();
		pool->Stop();
	});

	auto port_str = cfg_mgr["chatserver1"]["Port"];
	CServer server(ioc, std::stoi(port_str));
	ioc.run();

	return 0;
}


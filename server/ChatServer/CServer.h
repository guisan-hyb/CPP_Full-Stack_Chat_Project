#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace net = boost::asio;

class CSession;

class CServer {
public:
	CServer(net::io_context& ioc, short port_num);
	~CServer();
	void ClearSession(const std::string& uuid);

private:
	void StartAccept();

	net::ip::tcp::acceptor _acceptor;
	short _port;
	std::unordered_map<std::string, std::shared_ptr<CSession>> _sessions;
	std::mutex _mtx;
};
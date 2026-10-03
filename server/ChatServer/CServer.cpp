#include "CServer.h"
#include <iostream>
#include "CSession.h"
#include "AsioIOServicePool.h"

CServer::CServer(net::io_context& ioc, short port_num)
	: _acceptor(ioc, { net::ip::tcp::v4(),static_cast<net::ip::port_type>(port_num) }), _port(port_num)
{
	std::cout << "Server start success, on port: " << port_num << std::endl;
	StartAccept();
}

CServer::~CServer()
{
	std::cout << "Server destruct listen on port : " << _port << std::endl;
}

void CServer::ClearSession(const std::string& uuid)
{
	std::lock_guard<std::mutex> lk(_mtx);
	_sessions.erase(uuid);
}

void CServer::StartAccept()
{
	auto& io_service = AsioIOServicePool::GetInst()->GetIOService();
	auto new_session = std::make_shared<CSession>(io_service, this);
	_acceptor.async_accept(new_session->GetSocket(), [this, new_session](boost::system::error_code ec) {
		if (!ec) {
			new_session->Start();
			std::lock_guard<std::mutex> lk(_mtx);
			_sessions[new_session->GetUuid()] = new_session;
		}
		else {
			std::cerr<< "session accept failed, error is " << ec.what() << std::endl;
		}

		StartAccept();
	});
}


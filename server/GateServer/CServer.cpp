#include "CServer.h"
#include "HttpConnection.h"
#include "AsioIOServicePool.h"

CServer::CServer(net::io_context& ioc, unsigned short& port)
	: _ioc(ioc), _acceptor(ioc, tcp::endpoint(tcp::v4(), port)) {

}

void CServer::Start()
{
	auto self = shared_from_this();
	auto& io_context = AsioIOServicePool::GetInst()->GetIOService();
	std::shared_ptr<HttpConnection> new_connection = std::make_shared<HttpConnection>(io_context);
	_acceptor.async_accept(new_connection->GetSocket(), [self, new_connection](beast::error_code ec) {
		try {
			// 出错则放弃这条连接，继续监听其他链接
			if (ec) {
				self->Start();
				return;
			}

			new_connection->Start();// 启动链接

			// 继续监听
			self->Start();
		}
		catch (std::exception& e) {

		}
	});
}

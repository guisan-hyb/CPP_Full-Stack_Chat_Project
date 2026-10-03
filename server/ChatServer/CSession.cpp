#include "CSession.h"
#include "CServer.h"
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include "const.h"
#include <iostream>
#include "LogicSystem.h"

CSession::CSession(net::io_context& ioc, CServer* server)
	: _socket(ioc), _server(server), _b_stop(false)
{
	boost::uuids::uuid a_uuid = boost::uuids::random_generator()();
	_uuid = boost::uuids::to_string(a_uuid);

	_recv_head_node = std::make_shared<RecvNode>(HEAD_TOTAL_LEN, 0);
}

CSession::~CSession()
{
	Close();
}

net::ip::tcp::socket& CSession::GetSocket() {
	return _socket;
}

std::string CSession::GetUuid()
{
	return _uuid;
}

void CSession::Start() {
	_recv_head_node->Clear();
	net::async_read(_socket, net::buffer(_recv_head_node->_data, HEAD_TOTAL_LEN),
		std::bind(&CSession::HandleReadHead, this, std::placeholders::_1, std::placeholders::_2,
			shared_from_this()));
}

void CSession::Send(std::string& msg, short msg_id)
{
	std::lock_guard<std::mutex> lk(_send_mtx);
	int send_que_size = _send_que.size();
	if (send_que_size > MAX_SENDQUE) {
		std::cout << "session: " << _uuid << " send que failed, size is: " << MAX_SENDQUE << std::endl;
		return;
	}

	_send_que.push(std::make_shared<SendNode>(msg.data(), msg.size(), msg_id));

	if (send_que_size > 0) return;

	DoWrite();
}

void CSession::Close() {
	_b_stop = true;
	_socket.close();
}

void CSession::DoWrite()
{
	auto self = shared_from_this();
	auto& msgnode = _send_que.front();

	net::async_write(_socket, net::buffer(msgnode->_data, msgnode->_total_len),
		[self, this](boost::system::error_code ec, std::size_t) {
			try
			{
				if (!ec) {
					std::lock_guard<std::mutex> lk(_send_mtx);
					_send_que.pop();

					if (!_send_que.empty()) {
						DoWrite();
					}
				}
				else {
					std::cout << "handle write failed, error code is: " << ec.message() << std::endl;
					_server->ClearSession(_uuid);
					Close();
				}
			}
			catch (const std::exception& e)
			{
				std::cerr << "Exception code: " << e.what() << std::endl;
			}
		});
}

void CSession::HandleReadHead(boost::system::error_code ec, std::size_t bytes_transferred, std::shared_ptr<CSession> _self_shared)
{
	try
	{
		if (!ec) {
			if (bytes_transferred < HEAD_TOTAL_LEN) {
				Close();
				_server->ClearSession(_uuid);
				std::cerr << "read head lenth error" << std::endl;
				return;
			}

			short msg_id = 0;
			memcpy(&msg_id, _recv_head_node->_data, HEAD_ID_LEN);
			msg_id = net::detail::socket_ops::network_to_host_short(msg_id);
			if (msg_id > MAX_LENGTH) {
				std::cout << "invalid msg_id is: " << msg_id << std::endl;
				_server->ClearSession(_uuid);
				return;
			}

			short msg_len = 0;
			memcpy(&msg_len, _recv_head_node->_data + HEAD_ID_LEN, HEAD_DATA_LEN);
			msg_len = net::detail::socket_ops::network_to_host_short(msg_len);
			if (msg_len > MAX_LENGTH) {
				std::cout << "invalid msg_len is: " << msg_len << std::endl;
				_server->ClearSession(_uuid);
				return;
			}

			_recv_msg_node = std::make_shared<RecvNode>(msg_len, msg_id);
			net::async_read(_socket, net::buffer(_recv_msg_node->_data, _recv_msg_node->_total_len),
				std::bind(&CSession::HandleReadMsg, this, std::placeholders::_1, std::placeholders::_2,
					shared_from_this()));
		}
		else {
			std::cerr << "handle read head failed, error: " << ec.message() << std::endl;
			Close();
			_server->ClearSession(_uuid);
		}
	}
	catch (const std::exception& e)
	{
		std::cerr << "Exception code: " << e.what() << std::endl;
	}
}

void CSession::HandleReadMsg(boost::system::error_code ec, std::size_t bytes_transferred, std::shared_ptr<CSession> _self_shared)
{
	try
	{
		if (!ec) {
			if (bytes_transferred < _recv_msg_node->_total_len) {
				Close();
				_server->ClearSession(_uuid);
				std::cerr << "read msg lenth error" << std::endl;
				return;
			}

			_recv_msg_node->_data[_recv_msg_node->_total_len] = '\0';

			// 投递到逻辑队列
			LogicSystem::GetInst()->PostMsgToQue(std::make_shared<LogicNode>(_self_shared, _recv_msg_node));

			_recv_head_node->Clear();
			net::async_read(_socket, net::buffer(_recv_head_node->_data, _recv_head_node->_total_len),
				std::bind(&CSession::HandleReadHead, this, std::placeholders::_1, std::placeholders::_2,
					shared_from_this()));
		}
		else {
			std::cerr << "handle read head failed, error: " << ec.message() << std::endl;
			Close();
			_server->ClearSession(_uuid);
		}
	}
	catch (const std::exception& e)
	{
		std::cerr << "Exception code: " << e.what() << std::endl;
	}
}


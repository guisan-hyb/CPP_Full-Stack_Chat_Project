#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <queue>
#include <mutex>
#include "MsgNode.h"

namespace net = boost::asio;

class CServer;

class CSession : public std::enable_shared_from_this<CSession>
{
public:
	CSession(net::io_context& ioc, CServer* server);
	~CSession();
	net::ip::tcp::socket& GetSocket();
	std::string GetUuid();
	void Start();
	void Send(std::string& msg, short msg_id);
	void Close();

private:
	void DoWrite();
	void HandleReadHead(boost::system::error_code, std::size_t bytes_transferred, std::shared_ptr<CSession> _self_shared);
	void HandleReadMsg(boost::system::error_code, std::size_t bytes_transferred, std::shared_ptr<CSession> _self_shared);

private:
	net::ip::tcp::socket _socket;
	CServer* _server;
	std::string _uuid;

	std::shared_ptr<RecvNode> _recv_head_node;
	std::shared_ptr<RecvNode> _recv_msg_node;
	std::queue<std::shared_ptr<SendNode>> _send_que;

	std::atomic<bool> _b_stop;
	std::mutex _send_mtx;
};




class LogicNode {
	friend class LogicSystem;
public:
	LogicNode(std::shared_ptr<CSession> session, std::shared_ptr<RecvNode> recvnode)
		: _session(session), _recvnode(recvnode)
	{ }

private:
	std::shared_ptr<CSession> _session;
	std::shared_ptr<RecvNode> _recvnode;
};
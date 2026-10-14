#include "CServer.h"
#include <iostream>
#include "CSession.h"
#include "AsioIOServicePool.h"
#include "UserMgr.h"
#include "ConfigMgr.h"
#include "RedisMgr.h"

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

void CServer::ClearSession(const std::string& session_id)
{
	std::lock_guard<std::mutex> lk(_mtx);

	auto it = _sessions.find(session_id);
	if (it == _sessions.end()) return;

	int uid = it->second->GetUserId();
	std::string server_name = ConfigMgr::GetInst()["SelfServer"]["Name"];

	// 排除自己, 本机是否还有该uid的其他会话(同机多开同一账号,或者同一连接重登过)
	bool has_other_local = false;
	if (uid > 0) {
		for (auto& kv : _sessions) {
			if (kv.first == session_id) continue;
			if (kv.second->GetUserId() == uid) {
				has_other_local = true;
				break;
			}
		}
	}

	_sessions.erase(it);
	if (uid <= 0) return; // 0 只存在于"连接已建立、但登录还没成功"这段区间里

	UserMgr::GetInst()->RmvUserSession(uid);

	// Logincount -1
	auto rd_res = RedisMgr::GetInst()->HGet(std::string(LOGIN_COUNT), server_name);
	int conn_count = rd_res.empty() ? 0 : std::stoi(rd_res);
	if (conn_count) conn_count--;
	RedisMgr::GetInst()->HSet(std::string(LOGIN_COUNT), server_name, std::to_string(conn_count));

	// uip_: 该字段只有在 "这条会话真的登陆过"+"归属还是我这台"+"本机没有该uid的其他会话" 时才删除
	if (has_other_local) return;
	std::string cur_owner;
	if (!RedisMgr::GetInst()->Get(std::string(USERIPPREFIX) + std::to_string(uid), cur_owner)) return;
	if (cur_owner != server_name) return;
	RedisMgr::GetInst()->Del(std::string(USERIPPREFIX) + std::to_string(uid));
}

void CServer::StartAccept()
{
	auto& io_service = AsioIOServicePool::GetInst()->GetIOService();
	auto new_session = std::make_shared<CSession>(io_service, this);
	_acceptor.async_accept(new_session->GetSocket(), [this, new_session](boost::system::error_code ec) {
		if (!ec) {
			new_session->Start();
			std::lock_guard<std::mutex> lk(_mtx);
			_sessions[new_session->GetSessionId()] = new_session;
		}
		else {
			std::cerr<< "session accept failed, error is " << ec.what() << std::endl;
		}

		StartAccept();
	});
}


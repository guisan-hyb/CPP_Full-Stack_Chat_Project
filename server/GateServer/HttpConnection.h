#pragma once

#include "const.h"

class HttpConnection : public std::enable_shared_from_this<HttpConnection>
{
	friend class LogicSystem;// 将单例逻辑类设置为友元便于处理，也可以再封装一个接口，都可以
public:
	HttpConnection(net::io_context& ioc);
	void Start();
	tcp::socket& GetSocket();

private:
	void CheckDeadline(); // 超时检测
	void WriteResponse(); // 应答
	void HandleReq(); // 处理请求：解析请求头，解析包体内容
	void PreParseGetParam();// 解析url

private:
	tcp::socket _socket;
	beast::flat_buffer _buffer{ 8192 };// 缓冲区
	http::request<http::dynamic_body> _request;// 解析请求
	http::response<http::dynamic_body> _response;// 回应客户端
	//定时器，判断是否超时
	net::steady_timer _deadline{
		_socket.get_executor(),std::chrono::seconds(60)
	};

	std::string _get_url;
	std::unordered_map<std::string, std::string> _get_params;
};




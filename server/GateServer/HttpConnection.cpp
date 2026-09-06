#include "HttpConnection.h"
#include "LogicSystem.h"

std::string UrlEncode(const std::string& str);// url编码操作
std::string UrlDecode(const std::string& str);// url解码操作


HttpConnection::HttpConnection(tcp::socket socket)
	: _socket(std::move(socket))
{

}

void HttpConnection::Start() {
	auto self = shared_from_this();
	http::async_read(_socket, _buffer, _request, [self](beast::error_code ec, std::size_t bytes_transferred) {
		try {
			if (ec) {
				std::cout << "http read err is: " << ec.what() << std::endl;
				return;
			}

			boost::ignore_unused(bytes_transferred);// http服务器无需做粘包处理，把已经发送的字节数忽略掉
			self->HandleReq();// 处理请求
			self->CheckDeadline();// 启动超时检测
		}
		catch (std::exception& e) {
			std::cout << "exception is: " << e.what() << std::endl;
		}
	});
}

void HttpConnection::CheckDeadline()
{
	auto self = shared_from_this();
	_deadline.async_wait([self](beast::error_code ec) {
		if (!ec) {
			self->_socket.close(ec);
		}
	});
}

void HttpConnection::WriteResponse()
{
	auto self = shared_from_this();
	_response.content_length(_response.body().size());
	http::async_write(_socket, _response, [self](beast::error_code ec, std::size_t bytes_transferred) {
		self->_socket.shutdown(tcp::socket::shutdown_send, ec);// 把发送端关闭
		self->_deadline.cancel();// 定时器取消
	});
}

void HttpConnection::HandleReq()
{
	_response.version(_request.version());// 设置版本
	_response.keep_alive(false);// 设置为短链接

	if (_request.method() == http::verb::get) {
		PreParseGetParam();// 解析url
		// 单例逻辑类处理Get请求
		bool success = LogicSystem::GetInst()->HandleGet(_get_url, shared_from_this());
		if (!success) {
			_response.result(http::status::not_found);
			_response.set(http::field::content_type, "text/plain");
			beast::ostream(_response.body()) << "url not found\r\n";
			WriteResponse();// 回包
			return;
		}

		_response.result(http::status::ok);
		_response.set(http::field::server, "GateServer");
		WriteResponse();// 回包
		return;
	}
	else if (_request.method() == http::verb::post) {
		// 处理Post请求，无需解析url，处理请求体即可
		bool success = LogicSystem::GetInst()->HandlePost(_request.target(), shared_from_this());
		if (!success) {
			_response.result(http::status::not_found);
			_response.set(http::field::content_type, "text/plain");
			beast::ostream(_response.body()) << "url not found\r\n";
			WriteResponse();// 回包
			return;
		}

		_response.result(http::status::ok);
		_response.set(http::field::server, "GateServer");
		WriteResponse();// 回包
		return;
	}
}

void HttpConnection::PreParseGetParam()
{
	// 提取 url, eg: get_test?key1=value&key2=value2
	auto url = _request.target();
	// 查找 查询字符串 的开始位置(即'?'的位置)
	auto query_pos = url.find('?');
	if (query_pos == std::string::npos) {
		_get_url = url;
		return;
	}

	_get_url = url.substr(0, query_pos);
	std::string query_string = url.substr(query_pos + 1);
	std::string key, value;
	std::size_t pos = 0;

	while ((pos = query_string.find('&')) != std::string::npos) {
		auto pair = query_string.substr(0, pos);
		std::size_t eq_pos = pair.find('=');
		if (eq_pos != std::string::npos) {
			key = UrlDecode(pair.substr(0, eq_pos));
			value = UrlDecode(pair.substr(eq_pos + 1));
			_get_params[key] = value;
		}
		query_string.erase(0, pos + 1);
	}
	// 处理最后一个参数对(如果没有 & 分隔符)
	if (!query_string.empty()) {
		std::size_t eq_pos = query_string.find('=');
		if (eq_pos != std::string::npos) {
			key = UrlDecode(query_string.substr(0, eq_pos));
			value = UrlDecode(query_string.substr(eq_pos + 1));
			_get_params[key] = value;
		}
	}
}

unsigned char ToHex(unsigned char x)
{
	return  x > 9 ? x + 55 : x + 48;
}

unsigned char FromHex(unsigned char x)// 16进制转十进制
{
	unsigned char y;
	if (x >= 'A' && x <= 'Z') y = x - 'A' + 10;
	else if (x >= 'a' && x <= 'z') y = x - 'a' + 10;
	else if (x >= '0' && x <= '9') y = x - '0';
	else assert(0);
	return y;
}

std::string UrlEncode(const std::string& str)// url编码操作
{
	std::string strTemp = "";
	size_t length = str.length();
	for (size_t i = 0; i < length; i++)
	{
		//判断是否仅有数字和字母构成
		if (isalnum((unsigned char)str[i]) ||
			(str[i] == '-') ||
			(str[i] == '_') ||
			(str[i] == '.') ||
			(str[i] == '~'))
			strTemp += str[i];
		else if (str[i] == ' ') //为空字符
			strTemp += "+";
		else
		{
			//其他字符需要提前加%并且高四位和低四位分别转为16进制
			strTemp += '%';
			strTemp += ToHex((unsigned char)str[i] >> 4);
			strTemp += ToHex((unsigned char)str[i] & 0x0F);
		}
	}
	return strTemp;
}

std::string UrlDecode(const std::string& str)// url解码操作
{
	std::string strTemp = "";
	size_t length = str.length();
	for (size_t i = 0; i < length; i++)
	{
		//还原+为空
		if (str[i] == '+') strTemp += ' ';
		//遇到%将后面的两个字符从16进制转为char再拼接
		else if (str[i] == '%')
		{
			assert(i + 2 < length);
			unsigned char high = FromHex((unsigned char)str[++i]);
			unsigned char low = FromHex((unsigned char)str[++i]);
			strTemp += high * 16 + low;
		}
		else strTemp += str[i];
	}
	return strTemp;
}


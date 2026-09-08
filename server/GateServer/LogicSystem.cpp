#include "LogicSystem.h"
#include "HttpConnection.h"

LogicSystem::LogicSystem() {
	RegGet("/get_test", [](std::shared_ptr<HttpConnection> connection) {
		beast::ostream(connection->_response.body()) << "receive get_test req";
		int i = 0;
		for (auto& ele : connection->_get_params) {
			i++;
			beast::ostream(connection->_response.body()) << "param " << i << " key is: " << ele.first;
			beast::ostream(connection->_response.body()) << "param " << i << " value is: " << ele.second << std::endl;
		}
	});

	RegPost("/get_verifycode", [](std::shared_ptr<HttpConnection> connection) {
		auto body_str = beast::buffers_to_string(connection->_request.body().data());
		std::cout << "receive body is: " << body_str << std::endl;
		connection->_response.set(http::field::content_type, "text/json");

		json root;// 要赋给_response 的
		json src_root;// 源，来自_request

		try {
			src_root = json::parse(body_str);
		}
		catch (const json::parse_error& e) {
			std::cout << "Failed to parse JSON data!" << std::endl;
			root["error"] = ErrorCodes::Error_Json;
			std::string jsonstr = root.dump();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}

		// email 字段可能不存在,用 value() 给一个默认值,避免抛异常(类似 JsonCpp 的 asString 行为)
		auto email = src_root.value("email", std::string(""));
		std::cout << "email is: " << email << std::endl;

		root["error"] = 0;
		root["email"] = src_root["email"];
		std::string jsonstr = root.dump();
		beast::ostream(connection->_response.body()) << jsonstr;
		return true;
	});
}

LogicSystem::~LogicSystem()
{
}

bool LogicSystem::HandleGet(std::string path, std::shared_ptr<HttpConnection> connection)
{
	if (_get_handlers.find(path) == _get_handlers.end()) {
		return false;
	}

	_get_handlers[path](connection);
	return true;
}

void LogicSystem::RegGet(std::string path, HttpHandler handler)
{
	_get_handlers[path] = handler;
}

void LogicSystem::RegPost(std::string path, HttpHandler handler)
{
	_post_handlers[path] = handler;
}

bool LogicSystem::HandlePost(std::string path, std::shared_ptr<HttpConnection> connection)
{
	if (_post_handlers.find(path) == _post_handlers.end()) {
		return false;
	}

	_post_handlers[path](connection);
	return true;
}



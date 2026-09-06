#pragma once

#include "const.h"

class HttpConnection;
using HttpHandler = std::function<void(std::shared_ptr<HttpConnection>)>;

class LogicSystem : public Singleton<LogicSystem>
{
	friend class Singleton<LogicSystem>;
public:
	~LogicSystem();
	bool HandleGet(std::string path, std::shared_ptr<HttpConnection> connection);// 处理Get请求
	void RegGet(std::string path, HttpHandler handler);// 注册Get请求

	void RegPost(std::string path, HttpHandler handler);// 注册Post请求
	bool HandlePost(std::string path, std::shared_ptr<HttpConnection> connection);// 处理Post请求

private:
	LogicSystem();
	
private:
	std::unordered_map<std::string, HttpHandler> _post_handlers;// post处理请求的集合
	std::unordered_map<std::string, HttpHandler> _get_handlers;// get处理请求的集合
};


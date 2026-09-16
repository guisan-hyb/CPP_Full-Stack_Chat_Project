#include "LogicSystem.h"
#include "HttpConnection.h"
#include "VerifyGrpcClient.h"
#include "RedisMgr.h"
#include "MysqlMgr.h"

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

		// 使用gRPC
		GetVerifyRsp rsp = VerifyGrpcClient::GetInst()->GetVerifyCode(email);

		root["error"] = rsp.error();
		root["email"] = src_root["email"];
		std::string jsonstr = root.dump();
		beast::ostream(connection->_response.body()) << jsonstr;
		return true;
	});


	RegPost("/user_register", [](std::shared_ptr<HttpConnection> connection) {
		auto body_str = beast::buffers_to_string(connection->_request.body().data());
		std::cout << "receive body is: " << body_str << std::endl;
		connection->_response.set(http::field::content_type, "text/json");

		json root;
		json src_root;

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

		//多重验证，包括前面的验证码服务，并没有进行后端的验证服务我用接口测试工具来发的时候发一个错误邮箱地址的他也会显示成功，因为没有做后端验证
		auto email = src_root.value("email", std::string(""));
		auto name = src_root.value("user", std::string(""));
		auto pwd = src_root.value("passwd", std::string(""));
		auto confirm = src_root.value("confirm", std::string(""));

		if (pwd != confirm) {
			std::cout << "password err " << std::endl;
			root["error"] = ErrorCodes::Passwd_Error;
			std::string jsonstr = root.dump();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}

		// 先查找redis中email对应的验证码
		std::string verify_code;
		std::string email_key = std::string(CODEPREFIX) + email;
		bool b_get_verify = RedisMgr::GetInst()->Get(email_key, verify_code);
		if (!b_get_verify) { // 验证码是否过期
			std::cout << "get verify code expired" << std::endl;
			root["error"] = ErrorCodes::Verify_Expired;
			std::string jsonstr = root.dump();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}

		if (verify_code != src_root.value("verifycode", std::string(""))) { // 验证码是否合理
			std::cout << "verify code error" << std::endl;
			root["error"] = ErrorCodes::Verify_Code_Error;
			std::string jsonstr = root.dump();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}

		// 查找用户是否已经存在
		// 这里是redi查找，没必要，用mysql查找
		/*bool b_user_exist = RedisMgr::GetInst()->ExistsKey(root["user"].get<std::string>());
		if (b_user_exist) {
			std::cout << "user exist" << std::endl;
			root["error"] = ErrorCodes::User_Exist;
			std::string jsonstr = root.dump();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}*/

		// 查找数据库判断用户是否存在
		int uid = MysqlMgr::GetInst()->RegUser(name, email, pwd);
		if (uid == 0 || uid == -1) {
			std::cout << "user or email exist" << std::endl;
			root["error"] = ErrorCodes::User_Exist;
			std::string jsonstr = root.dump();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}


		root["error"] = 0;
		root["email"] = src_root["email"];
		root["user"] = src_root["user"];
		root["passwd"] = src_root["passwd"];
		root["verifycode"] = src_root["verifycode"];
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



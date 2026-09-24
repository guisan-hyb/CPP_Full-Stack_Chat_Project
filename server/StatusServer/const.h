#pragma once

#include <boost/beast/http.hpp>
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <memory>
#include <iostream>
#include "Singleton.h"
#include <functional>
#include <unordered_map>
#include <string>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>

#include <vector>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>


#include <sw/redis++/redis++.h>
#include <chrono>

#include <mysqlx/xdevapi.h>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;
using json = nlohmann::json;


enum ErrorCodes {
	Success = 0,
	Error_Json = 1001, // json解析错误
	RPC_Failed = 1002, // RPC请求错误
	Verify_Expired = 1003, // 验证码过期
	Verify_Code_Error = 1004, // 验证码错误
	User_Exist = 1005, // 用户已经存在
	Passwd_Error = 1006, // 密码错误
	Email_Not_Match = 1007, // 邮箱不匹配
	Passwd_Up_Failed = 1008, // 更新密码失败
	Passwd_Invalid = 1009, // 密码无效
	Token_Invalid = 1010, // Token无效
	Uid_Invalid = 1011, // uid无效
};


#define CODEPREFIX "code_"

#define USERIPPREFIX  "uip_"
#define USERTOKENPREFIX  "utoken_"
#define IPCOUNTPREFIX  "ipcount_"
#define USER_BASE_INFO "ubaseinfo_"
#define LOGIN_COUNT  "logincount"
#define LOCK_COUNT "lockcount"


#pragma once

#include <functional>

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



class Defer {
public:
	Defer(std::function<void()> f) : _func(f) {}
	~Defer() {
		_func();
	}

private:
	std::function<void()> _func;
};


#define MAX_LENGTH  1024*2
//头部总长度
#define HEAD_TOTAL_LEN 4
//头部id长度
#define HEAD_ID_LEN 2
//头部数据长度
#define HEAD_DATA_LEN 2
#define MAX_RECVQUE  10000
#define MAX_SENDQUE 1000


enum MSG_IDS {
	MSG_CHAT_LOGIN = 1005, //用户登陆
	MSG_CHAT_LOGIN_RSP = 1006, //用户登陆回包
};


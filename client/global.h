#ifndef GLOBAL_H
#define GLOBAL_H

#include <QWidget>
#include <functional>
#include <QStyle>
#include <QRegularExpression>
#include <memory>
#include <iostream>
#include <mutex>
#include <QByteArray>
#include <QNetworkReply>
#include <QJsonObject>
#include <QDir>
#include <QSettings>

/**
 * @brief repolish 用来刷新qss (快捷键/** + Enter)
 */
extern std::function<void(QWidget*)> repolish;

// 简单的动态加密
extern std::function<QString(QString)> xorString;

/**
 * @brief The ReqId enum 具体功能ID
 */
enum ReqId{
    ID_GET_VERIFY_CODE = 1001, // 获取验证码
    ID_REG_USER = 1002, // 注册用户
    ID_RESET_PWD = 1003, // 重置密码
    ID_LOGIN_USER = 1004, // 用户登录
    ID_CHAT_LOGIN = 1005, //登录聊天服务器
    ID_CHAT_LOGIN_RSP = 1006, //登录聊天服务器回包
};

/**
 * @brief The Modules enum 模块ID
 */
enum Modules{
    MOD_REGISTER = 0,
    MOD_RESET = 1,
};

/**
 * @brief The ErrorCodes enum 错误类型
 */
enum ErrorCodes{
    SUCCESS = 0,
    ERR_JSON = 1, // json解析失败
    ERR_NETWORK = 2, // 网络错误
};

/**
 * @brief The TipErr enum 前端注册错误类型
 */
enum TipErr{
    TIP_SUCCESS = 0,
    TIP_EMAIL_ERR = 1,
    TIP_PWD_ERR = 2,
    TIP_CONFIRM_ERR = 3,
    TIP_PWD_CONFIRM = 4,
    TIP_VERIFY_ERR = 5,
    TIP_USER_ERR = 6
};

/**
 * @brief The ClickLbState enum 注册时 隐藏和显示密码 状态
 */
enum ClickLbState{
    Normal = 0,
    Selected = 1
};

/**
 * @brief gate_url_prefix 网关url前缀
 */
extern QString gate_url_prefix;

#endif // GLOBAL_H

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

/**
 * @brief The ReqId enum 具体功能ID
 */
enum ReqId{
    ID_GET_VERIFY_CODE = 1001, // 获取验证码
    ID_REG_USER = 1002, // 注册用户
};

/**
 * @brief The Modules enum 模块ID
 */
enum Modules{
    MOD_REGISTER = 0,

};

/**
 * @brief The ErrorCodes enum 错误类型
 */
enum ErrorCodes{
    SUCCESS = 0,
    ERR_JSON = 1, // json解析失败
    ERR_NETWORK = 2, // 网络错误
};

extern QString gate_url_prefix;

#endif // GLOBAL_H

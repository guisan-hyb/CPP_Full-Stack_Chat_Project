#ifndef GLOBAL_H
#define GLOBAL_H

#include <QWidget>
#include <functional>
#include <QStyle>
#include <QRegularExpression>

/**
 * @brief repolish 用来刷新qss (快捷键/** + Enter)
 */
extern std::function<void(QWidget*)> repolish;

#endif // GLOBAL_H

#include "global.h"

QString gate_url_prefix = "";

std::function<void(QWidget*)> repolish = [](QWidget* w){
    //把原来的样式去掉，再刷一下
    w->style()->unpolish(w);
    w->style()->polish(w);
};


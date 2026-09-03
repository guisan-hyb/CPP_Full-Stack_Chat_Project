#include "global.h"

std::function<void(QWidget*)> repolish = [](QWidget* w){
    //把原来的样式去掉，再刷一下
    w->style()->unpolish(w);
    w->style()->polish(w);
};


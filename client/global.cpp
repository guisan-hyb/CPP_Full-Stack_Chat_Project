#include "global.h"

QString gate_url_prefix = "";

std::function<void(QWidget*)> repolish = [](QWidget* w){
    //把原来的样式去掉，再刷一下
    w->style()->unpolish(w);
    w->style()->polish(w);
};


std::function<QString(QString)> xorString = [](QString input){
    QString result = input;
    int length = result.length();
    length %= 255;
    for(int i = 0;i<input.length();i++){
        result[i] = QChar(static_cast<ushort>(input[i].unicode() ^ static_cast<ushort>(length)));
    }
    return result;
};


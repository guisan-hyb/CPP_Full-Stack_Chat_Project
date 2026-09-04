QT += widgets network

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    global.cpp \
    httpmgr.cpp \
    logindialog.cpp \
    main.cpp \
    mainwindow.cpp \
    registerdialog.cpp

HEADERS += \
    global.h \
    httpmgr.h \
    logindialog.h \
    mainwindow.h \
    registerdialog.h \
    singleton.h

FORMS += \
    logindialog.ui \
    mainwindow.ui \
    registerdialog.ui

RC_ICONS = icon.ico # icon
DESTDIR = ./bin # 指定编译后生成的可执行文件（例如 .exe 文件）的输出目录

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    rc.qrc

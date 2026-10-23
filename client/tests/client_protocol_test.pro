QT += core
CONFIG += console c++17
CONFIG -= app_bundle

TEMPLATE = app
TARGET = client_protocol_test

INCLUDEPATH += ..

SOURCES += \
    client_protocol_test.cpp \
    ../chatprotocol.cpp

HEADERS += \
    ../chatprotocol.h \
    ../textutils.h

TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.cpp
INCLUDEPATH += "/usr/include/boost"
LIBS += -lboost_system -lboost_thread -lpthread

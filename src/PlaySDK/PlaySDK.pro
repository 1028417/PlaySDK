#-------------------------------------------------
#
# Project created by QtCreator 2019-03-30T00:23:26
#
#-------------------------------------------------

QT       -= core

TARGET = xPlaySDK
TEMPLATE = lib

#CONFIG += c++11
QMAKE_CXXFLAGS += -std=c++1y #gnu++1y

SOURCES += \
    Player.cpp \
    decoder.cpp \
    audiodecoder.cpp \
    avpacketqueue.cpp \
    SDLEngine.cpp \
    SLESEngine.cpp

HEADERS +=\
    ../../inc/Player.h \
    decoder.h \
    audiodecoder.h \
    avpacketqueue.h \
    inc.h \
    SDLEngine.h \
    SLESEngine.h

DEFINES += __PlaySDKPrj

INCLUDEPATH += \
    ../../../Common2.1/inc/util \
    ./ffmpeg/include

android {
LIBS    += -lOpenSLES #-landroid -llog

LIBS    += -L$$PWD/../../lib/armeabi-v7a/ffmpeg -lavcodec -lavformat -lavutil -lswresample

LIBS    += -L$$PWD/../../../XMusic/lib/armeabi-v7a -lxutil

DESTDIR = $$PWD/../../../XMusic/lib/armeabi-v7a
} else {
INCLUDEPATH += ./SDL2/include

LIBS    += $$PWD/../../bin/SDL2.dll

LIBS    += -L$$PWD/ffmpeg/lib \
    -lavcodec.dll -lavformat.dll -lavutil.dll -lswresample.dll

LIBS    += -L$$PWD/../../../Common2.1/bin -lxutil

DESTDIR = $$PWD/../../bin
}

MOC_DIR = $$PWD/../../build/
RCC_DIR = $$PWD/../../build/
UI_DIR = $$PWD/../../build/
OBJECTS_DIR = $$PWD/../../build/

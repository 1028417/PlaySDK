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
    LIBS    += -L$$PWD/../../lib/armeabi-v7a/ffmpeg -L$$PWD/../../../XMusic/lib/armeabi-v7a \
        -lOpenSLES \ #-landroid -llog
        -lavcodec -lavformat -lavutil -lswresample

    DESTDIR = $$PWD/../../../XMusic/lib/armeabi-v7a
} else {
    INCLUDEPATH += ./SDL2/include

    LIBS    +=  -L$$PWD/../../../Common2.1/bin -L$$PWD/../../bin

    macx {
        LIBS    += "-F$$PWD/SDL2/" \
            -framework SDL2 -lavcodec.58 -lavformat.58 -lavutil.56 -lswresample.3
    } else {
        LIBS    += -lSDL2 -lavcodec-58 -lavformat-58 -lavutil-56 -lswresample-3
    }

    DESTDIR = $$PWD/../../bin
}

LIBS    += -lxutil

MOC_DIR = $$PWD/../../build/
RCC_DIR = $$PWD/../../build/
UI_DIR = $$PWD/../../build/
OBJECTS_DIR = $$PWD/../../build/

#-------------------------------------------------
#
# Project created by QtCreator 2019-03-30T00:23:26
#
#-------------------------------------------------

QT       -= core

TARGET = xPlaySDK
TEMPLATE = lib

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
    ../../../Common2.1/inc \
    ./ffmpeg/include

android {
    LIBS    += -lOpenSLES -L../../lib/armeabi-v7a/ffmpeg \
        -lavcodec -lavformat -lavutil -lswresample

    LIBS    += -L../../../XMusic/lib/armeabi-v7a -lxutil

    DESTDIR = ../../../XMusic/lib/armeabi-v7a
} else {
    INCLUDEPATH += ./SDL2/include

    macx {
        LIBS    +=  ../../bin/macx/SDL2.framework/Versions/A/SDL2

        LIBS    +=  -L../../bin/macx -lavcodec.58 -lavformat.58 -lavutil.56 -lswresample.3

        LIBS    +=  -L../../../Common2.1/bin/macx -lxutil.1

        DESTDIR = ../../bin/macx

        target.path = ../../../XMusic/bin/macx
        INSTALLS += target
    } else: ios {
        DESTDIR = ../../../build/ioslib
    } else {
        LIBS    +=  -L../../bin -lSDL2 -lavcodec-58 -lavformat-58 -lavutil-56 -lswresample-3

        LIBS    +=  -L../../../Common2.1/bin -lxutil

        DESTDIR = ../../bin

        target.path = ../../../XMusic/bin
        INSTALLS += target
    }
}

INSTALLS += target

MOC_DIR = ../../../build/xPlaySDK
RCC_DIR = ../../../build/xPlaySDK
UI_DIR = ../../../build/xPlaySDK
OBJECTS_DIR = ../../../build/xPlaySDK

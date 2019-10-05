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
    LIBS    += -lOpenSLES

    LIBS    += -L../../libs/armeabi-v7a/ffmpeg -lavcodec -lavformat -lavutil -lswresample

    LIBS    += -L../../../XMusic/libs/armeabi-v7a -lxutil

    build_dir = ../../../build/xPlaySDK/android
    DESTDIR = ../../../libs/armeabi-v7a
} else {
    INCLUDEPATH += ./SDL2/include

    macx {
        LIBS    +=  ../../bin/mac/SDL2.framework/Versions/A/SDL2

        LIBS    += -L../../bin/mac -lavcodec.58 -lavformat.58 -lavutil.56 -lswresample.3

        #LIBS    += -lavcodec -lavformat -lavutil -lswresample \
        #            -framework CoreFoundation   -framework AudioToolbox  -lz -lbz2 -liconv -framework CoreMedia -framework VideoToolbox -framework AVFoundation -framework CoreVideo -framework Security

        LIBS    += -L../../../Common2.1/bin/mac -lxutil

        build_dir = ../../../build/xPlaySDK/mac
        DESTDIR = ../../bin/mac

        target.path = ../../../XMusic/bin/mac
        INSTALLS += target
    } else: ios {
        build_dir = ../../../build/xPlaySDK/ios
        DESTDIR = ../../../build/ioslib
    } else {
        LIBS    += -L../../bin -lSDL2 -lavcodec-58 -lavformat-58 -lavutil-56 -lswresample-3

        LIBS    += -L../../../Common2.1/bin -lxutil

        build_dir = ../../../build/xPlaySDK/win
        DESTDIR = ../../bin

        target.path = ../../../XMusic/bin
        INSTALLS += target
    }
}

MOC_DIR = $$build_dir
RCC_DIR = $$build_dir
UI_DIR = $$build_dir
OBJECTS_DIR = $$build_dir

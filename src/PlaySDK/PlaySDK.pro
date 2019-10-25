#-------------------------------------------------
#
# Project created by QtCreator 2019-03-30T00:23:26
#
#-------------------------------------------------

QT       -= core

TARGET = xPlaySDK
TEMPLATE = lib

QMAKE_CXXFLAGS += -std=c++11 #c++1y #gnu++1y

SOURCES += cpp/Player.cpp \
    cpp/decoder.cpp \
    cpp/audiodecoder.cpp \
    cpp/avpacketqueue.cpp \
    cpp/SDLEngine.cpp \
    cpp/SLESEngine.cpp

HEADERS += ../../inc/Player.h  cpp/inc.h

INCLUDEPATH += ../../../Common2.1/inc  ../../3rd/ffmpeg/include

DEFINES += __PlaySDKPrj

android {
    LIBS += -L../../../XMusic/libs/armeabi-v7a  -lxutil \
            -lOpenSLES \
            -L../../libs/armeabi-v7a/ffmpeg  -lavcodec  -lavformat  -lavutil  -lswresample \

    platform = android
    DESTDIR = ../../../XMusic/libs/armeabi-v7a
} else {
    macx {
        LIBS += -L../../../Common2.1/bin/mac  -lxutil \
                ../../bin/mac/SDL2.framework/Versions/A/SDL2 \
                -L../../bin/mac  -lavcodec.58  -lavformat.58  -lavutil.56  -lswresample.3
        #LIBS += -lavcodec  -lavformat  -lavutil  -lswresample -lz  -lbz2  -liconv \
        #        -framework CoreFoundation  -framework AudioToolbox  -framework CoreMedia \
        #        -framework VideoToolbox  -framework AVFoundation  -framework CoreVideo  -framework Security

       platform = mac
        DESTDIR = ../../bin/mac

        target.path = ../../../XMusic/bin/mac
        INSTALLS += target
    } else: ios {
        platform = ios
        DESTDIR = ../../../build/ioslib
    } else {
        LIBS += -L../../../Common2.1/bin  -lxutil \
                -L../../bin  -lSDL2  -lavcodec-58  -lavformat-58  -lavutil-56  -lswresample-3

        platform = win
        DESTDIR = ../../bin

        target.path = ../../../XMusic/bin
        INSTALLS += target
    }
}

build_dir = ../../../build/xPlaySDK/$$platform

MOC_DIR = $$build_dir
RCC_DIR = $$build_dir
UI_DIR = $$build_dir
OBJECTS_DIR = $$build_dir

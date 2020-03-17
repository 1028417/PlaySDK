#-------------------------------------------------
#
# Project created by QtCreator 2019-03-30T00:23:26
#
#-------------------------------------------------

QT       -= gui

TARGET = xPlaySDK
TEMPLATE = lib

QMAKE_CXXFLAGS += -std=c++11 #c++1y #gnu++1y

DEFINES += QT_DEPRECATED_WARNINGS __PlaySDKPrj

SOURCES += cpp/Player.cpp \
    cpp/decoder.cpp \
    cpp/audiodecoder.cpp \
    cpp/avpacketqueue.cpp \
    cpp/SDLEngine.cpp \
    cpp/SLESEngine.cpp

HEADERS += ../../inc/Player.h  cpp/inc.h

CommonDir = ../../../Common2.1
INCLUDEPATH += $$CommonDir/inc  ../../3rd/ffmpeg/include

mac {
XMusicDir = ../../../XMusic
} else {
XMusicDir = ..\..\..\XMusic
}

BinDir = ../../bin

android {
    LIBS += -L$$CommonDir/libs/armeabi-v7a  -lxutil \
            -lOpenSLES \
            -L../../libs/armeabi-v7a/ffmpeg  -lavcodec  -lavformat  -lavutil  -lswresample \

    platform = android
    DESTDIR = ..\..\libs\armeabi-v7a
    QMAKE_POST_LINK += copy /Y $$DESTDIR\libxPlaySDK.so $$XMusicDir\libs\armeabi-v7a
} else: macx {
        LIBS += -L$$CommonDir/bin/mac  -lxutil \
                        $$BinDir/mac/SDL2.framework/Versions/A/SDL2 \
                        -L$$BinDir/mac  -lavcodec.58  -lavformat.58  -lavutil.56  -lswresample.3
	#LIBS += -lavcodec  -lavformat  -lavutil  -lswresample -lz  -lbz2  -liconv \
	#        -framework CoreFoundation  -framework AudioToolbox  -framework CoreMedia \
	#        -framework VideoToolbox  -framework AVFoundation  -framework CoreVideo  -framework Security

	platform = mac
        DESTDIR = $$BinDir/mac

        QMAKE_POST_LINK += cp -f $$DESTDIR/libxPlaySDK*.dylib $$XMusicDir/bin/mac/
} else: ios {
	platform = ios
	DESTDIR = ../../../build/ioslib
} else {
        LIBS += -L$$CommonDir/bin  -lxutil \
                        -L$$BinDir  -lSDL2  -lavcodec-58  -lavformat-58  -lavutil-56  -lswresample-3

	platform = win
        DESTDIR = ..\..\bin

        QMAKE_POST_LINK += copy /Y $$DESTDIR\xPlaySdk.dll $$XMusicDir\bin && \
            copy /Y $$DESTDIR\libxPlaySDK.a $$XMusicDir\bin
}

#CONFIG += debug_and_release
CONFIG(debug, debug|release) {
BuildDir = xPlaySDKd
} else {
BuildDir = xPlaySDK
}
BuildDir = ../../../build/$$BuildDir/$$platform

MOC_DIR = $$BuildDir
RCC_DIR = $$BuildDir
UI_DIR = $$BuildDir
OBJECTS_DIR = $$BuildDir

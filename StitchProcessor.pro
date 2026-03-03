QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17
QMAKE_CXXFLAGS += /utf-8

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    AppConfig.cpp \
    DecodeThread.cpp \
    OriginVideoWid.cpp \
    ProcessThread.cpp \
    Utils.cpp \
    main.cpp \
    Widget.cpp

HEADERS += \
    AppConfig.h \
    DecodeThread.h \
    OriginVideoWid.h \
    ProcessThread.h \
    Utils.h \
    Widget.h

FORMS += \
    OriginVideoWid.ui \
    Widget.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target


# OpenCV4.90
win32:CONFIG(release, debug|release): LIBS += -L$$PWD/3rdparty/OpenCV4.10/x64/vc16/lib/ -lopencv_world4100
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/3rdparty/OpenCV4.10/x64/vc16/lib/ -lopencv_world4100d

INCLUDEPATH += $$PWD/3rdparty/OpenCV4.10/include
DEPENDPATH += $$PWD/3rdparty/OpenCV4.10/include

# FFmpeg8.0
LIBS += -L$$PWD/3rdparty/ffmpeg-8.0-full_build-shared/lib/ -lavcodec -lavdevice -lavfilter -lavformat -lavutil -lswresample -lswscale

INCLUDEPATH += $$PWD/3rdparty/ffmpeg-8.0-full_build-shared/include
DEPENDPATH += $$PWD/3rdparty/ffmpeg-8.0-full_build-shared/include

# cuda11.6
LIBS += -L$$PWD/3rdparty/cuda12.6/lib/x64 -lcuda -lcudart
INCLUDEPATH += $$PWD/3rdparty/cuda12.6/include
DEPENDPATH += $$PWD/3rdparty/cuda12.6/include

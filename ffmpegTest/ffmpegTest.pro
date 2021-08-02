QT       += core gui multimedia widgets opengl openglextensions

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    audioplay.cpp \
    audiothread.cpp \
    decode.cpp \
    decodethread.cpp \
    demux.cpp \
    demuxthread.cpp \
    main.cpp \
    resample.cpp \
    slider.cpp \
    videothread.cpp \
    videowidget.cpp \
    widget.cpp

HEADERS += \
    IVideoCall.h \
    audioplay.h \
    audiothread.h \
    decode.h \
    decodethread.h \
    demux.h \
    demuxthread.h \
    resample.h \
    slider.h \
    videothread.h \
    videowidget.h \
    widget.h

FORMS += \
    widget.ui
INCLUDEPATH+=$$PWD/../include
LIBS+= -L$$PWD/../lib -lavcodec -lavdevice -lavfilter -lavformat -lavutil  -lswscale -lswresample
DESTDIR=$$PWD/../bin
#INCLUDEPATH+=/usr/local/ffmpeg/include
#LIBS+= -L/usr/local/ffmpeg/lib -lavcodec -lavdevice -lavfilter -lavformat -lavutil  -lswscale -lswresample
# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

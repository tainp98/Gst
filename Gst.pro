TEMPLATE = app
CONFIG += console c++17
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
        main.cpp \
#    findtype.cpp \
#    dataprobe.cpp \
#    appsrc.cpp
#    changecaps.cpp \
    changepipe.cpp \
#    multithreadpad.cpp
    StreamingManager.cpp \
    RecordTee.cpp \
    AddRemoveRecord.cpp \
    SplitMuxSink.cpp \
    DirTool.cpp \
    Monitoring.cpp \
    ChangeSource.cpp
#    dynamicrecord.cpp

#============= gstreamer-1.0
INCLUDEPATH += /usr/local/include/gstreamer-1.0
INCLUDEPATH += /usr/lib/x86_64-linux-gnu/gstreamer-1.0/include
INCLUDEPATH += /usr/include/glib-2.0
INCLUDEPATH += /usr/lib/x86_64-linux-gnu/glib-2.0/include
LIBS += -lgstapp-1.0 -lgstbase-1.0 -lgstreamer-1.0 -lgobject-2.0 -lglib-2.0 -lgstpbutils-1.0 -lgstvideo-1.0

HEADERS += \
#    multithreadpad.h
    StreamingManager.h \
    RecordTee.h \
    AddRemoveRecord.h \
    SplitMuxSink.h \
    DirTool.h \
    ChangeSource.h

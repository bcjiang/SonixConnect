QT += core gui

TARGET = qt
TEMPLATE = app

HEADERS += \
    TexoView.h \
    TexoDemo.h \
    stdafx.h \
    TexoViewImg.h

SOURCES += \
    TexoView.cpp \
    TexoDemo.cpp \
    StdAfx.cpp \
    main.cpp \
    TexoViewImg.cpp

FORMS += \
    texo.ui

RESOURCES += \
    texo.qrc \
    texo.qrc

OTHER_FILES += \
    res/u.ico \
    res/stop.png \
    res/run.png \
    res/init.png



INCLUDEPATH += ../../inc/
INCLUDEPATH += ../../../

LIBS += -L"../../lib/" -ltexo
LIBS += -L"../../../bin/" -ltexo



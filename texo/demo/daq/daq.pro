QT += core gui

TARGET = qt
TEMPLATE = app

INCLUDEPATH += ../../inc/
INCLUDEPATH += ../../../daq/inc/
INCLUDEPATH += ../../../amplio/inc/
INCLUDEPATH += ../../../

LIBS += -L"../../lib/" -ltexo
LIBS += -L"../../../bin/" -ltexo
LIBS += -L"../../../daq/lib/" -ldaq
LIBS += -L"../../../bin/" -ldaq
LIBS += -L"../../../amplio/lib/" -lamplio
LIBS += -L"../../../bin/" -lamplio

HEADERS += \
    ViewBImg.h \
    TexoViewImg.h \
    TexoDemo.h \
    stdafx.h \
    ViewRFImg.h

SOURCES += \
    ViewRFImg.cpp \
    ViewBImg.cpp \
    TexoViewImg.cpp \
    TexoDemo.cpp \
    StdAfx.cpp \
    main.cpp

FORMS += \
    texo.ui

RESOURCES += \
    texo.qrc



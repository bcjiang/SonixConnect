QT += core gui

TARGET = qt
TEMPLATE = app

HEADERS += \
    UlteriusDemo.h \
    stdafx.h

SOURCES += \
    UlteriusDemo.cpp \
    StdAfx.cpp \
    main.cpp

FORMS += \
    ulterius.ui

RESOURCES += \
    ulterius.qrc \
    ulterius.qrc

OTHER_FILES += \
    res/u.ico \
    res/refresh.png \
    res/random.png \
    res/inject.png \
    res/freeze.png \
    res/connect.png



INCLUDEPATH += ../../inc/
INCLUDEPATH += ../../../

LIBS += -L"../../lib/" -lulterius_old
LIBS += -L"../../../bin/" -lulterius_old

TEMPLATE = app
CONFIG += console
CONFIG -= qt

SOURCES += \
    StdAfx.cpp \
    main.cpp

HEADERS += \
    StdAfx.h

INCLUDEPATH += ../../inc/
INCLUDEPATH += ../../../

LIBS += -L"../../lib" -ltexo
LIBS += -L"../../../bin" -ltexo

QT       += core gui widgets
CONFIG   += c++17
TARGET    = bugworld
TEMPLATE  = app

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    gridwidget.cpp \
    simulator.cpp \
    parser.cpp

HEADERS += \
    mainwindow.h \
    gridwidget.h \
    simulator.h \
    parser.h

LIBS += -lpthread
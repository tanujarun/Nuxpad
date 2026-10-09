TEMPLATE = app
TARGET = notepad
QT += core gui widgets printsupport svg network
CONFIG += c++20 release warn_on
CONFIG -= debug_and_release

DEFINES += QT_NO_CAST_FROM_BYTEARRAY NOTEPAD_VERSION=\\\"2.0\\\"

HEADERS += \
    src/appsettings.h \
    src/editor.h \
    src/findbar.h \
    src/mainwindow.h \
    src/pagesetup.h \
    src/printing.h \
    src/session.h \
    src/settingspage.h \
    src/statusbar.h \
    src/style.h \
    src/tabbar.h \
    src/textfile.h \
    src/theme.h \
    src/widgets.h

SOURCES += \
    src/appsettings.cpp \
    src/editor.cpp \
    src/findbar.cpp \
    src/main.cpp \
    src/mainwindow.cpp \
    src/pagesetup.cpp \
    src/printing.cpp \
    src/session.cpp \
    src/settingspage.cpp \
    src/statusbar.cpp \
    src/style.cpp \
    src/tabbar.cpp \
    src/textfile.cpp \
    src/theme.cpp \
    src/widgets.cpp

RESOURCES += res/notepad.qrc

OBJECTS_DIR = obj
MOC_DIR = moc
RCC_DIR = rcc

# Test build only (qmake CONFIG+=devhooks): scripted input and window grabs.
devhooks {
    QT += testlib
    DEFINES += NOTEPAD_DEVHOOKS
    SOURCES += src/devscript.cpp
}

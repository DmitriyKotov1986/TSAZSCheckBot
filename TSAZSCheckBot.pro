QT = core network sql

CONFIG += c++20 cmdline
CONFIG += static

VERSION = 0.1

HEADERS += \
        buttondata.h \
        chat.h \
        filedownloader.h \
        question.h \
        questionnaire.h \
        tconfig.h \
        core.h \
        user.h \
        users.h

SOURCES += \
        buttondata.cpp \
        chat.cpp \
        filedownloader.cpp \
        main.cpp \
        question.cpp \
        questionnaire.cpp \
        tconfig.cpp \
        core.cpp \
        user.cpp \
        users.cpp

TRANSLATIONS += \
    TSAZSCheckBot_ru_RU.ts

CONFIG += lrelease
CONFIG += embed_translations

PKGCONFIG += openssl

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

include($$PWD/../../Common/Common/Common.pri)
include($$PWD/../../Telegram/Telegram/Telegram.pri)
include($$PWD/../../QtCSV/QtCSV/QtCSV.pri)

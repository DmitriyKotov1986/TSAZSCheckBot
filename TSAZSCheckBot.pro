QT = core network sql

CONFIG += c++20 cmdline
CONFIG += static

HEADERS += \
        buttondata.h \
        question.h \
        questionnaire.h \
        tconfig.h \
        core.h \
        users.h

SOURCES += \
        buttondata.cpp \
        main.cpp \
        question.cpp \
        questionnaire.cpp \
        tconfig.cpp \
        core.cpp \
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

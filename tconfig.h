#pragma once

//QT
#include <QString>

#include "Common/common.h"

class TConfig final
{
public:
    static TConfig* config(const QString& configFileName = "");
    static void deleteConfig();

private:
    TConfig() = delete;
    Q_DISABLE_COPY_MOVE(TConfig)

    explicit TConfig(const QString& configFileName);
    ~TConfig();

public:
    bool save();

    //[DATABASE]
    const Common::DBConnectionInfo& dbConnectionInfo() const { return _dbConnectionInfo; };

    //[SYSTEM]
    bool sys_DebugMode() const { return _sys_DebugMode; }

    //[BOT]
    const QString& bot_token() const { return _bot_token; }
    qint32 bot_updateId() const { return _bot_updateId; }
    void set_bot_UpdateId(qint32 updateId);

    //errors
    QString errorString();
    bool isError() const { return !_errorString.isEmpty(); }

private:
    const QString _configFileName;

    QString _errorString;

    //[DATABASE]
    Common::DBConnectionInfo _dbConnectionInfo;

    //[SYSTEM]
    bool _sys_DebugMode = false;

    //[BOT]
    QString _bot_token;
    qint32 _bot_updateId = 0;

};

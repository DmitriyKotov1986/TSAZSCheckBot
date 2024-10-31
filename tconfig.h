#pragma once

//QT
#include <QString>
#include <QObject>

//My
#include "Common/common.h"
#include "Common/tdbconfig.h"

class TConfig final
    : public QObject
{
    Q_OBJECT

public:
    static TConfig* config(const QString& configFileName = "");
    static void deleteConfig();

    static void makeConfig(const QString& configFileName);

public:
    //[DATABASE]
    const Common::DBConnectionInfo& dbConnectionInfo() const { return _dbConnectionInfo; };

    //[SYSTEM]
    bool sys_DebugMode() const { return _sys_DebugMode; }

    //[BOT]
    const QString& bot_token() const { return _bot_token; }

    qint32 bot_updateId();
    void set_bot_UpdateId(qint32 updateId);

    //errors
    QString errorString();
    bool isError() const { return !_errorString.isEmpty(); }

signals:
    void errorOccurred(Common::EXIT_CODE errorCode, const QString& errorString);

private slots:
    void errorOccurredDBConfig(Common::EXIT_CODE errorCode, const QString& errorString);

private:
    TConfig() = delete;
    Q_DISABLE_COPY_MOVE(TConfig)

    explicit TConfig(const QString& configFileName);
    ~TConfig();

    void loadFromDB();

private:
    const QString _configFileName;

    QString _errorString;

    Common::TDBConfig* _dbConfig = nullptr;

    //[DATABASE]
    Common::DBConnectionInfo _dbConnectionInfo;

    //[SYSTEM]
    bool _sys_DebugMode = false;

    //[BOT]
    QString _bot_token;

};

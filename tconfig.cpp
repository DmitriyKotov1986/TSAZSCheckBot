//QT
#include <QSettings>
#include <QFileInfo>
#include <QDebug>
#include <QFileInfo>

#include "tconfig.h"

using namespace Common;

//static
static TConfig* configPtr = nullptr;

TConfig* TConfig::config(const QString& configFileName)
{
    if (configPtr == nullptr)
    {
        Q_ASSERT(!configFileName.isEmpty());
        configPtr = new TConfig(configFileName);
    }

    return configPtr;
};

void TConfig::deleteConfig()
{
    delete configPtr;

    configPtr = nullptr;
}

//public
TConfig::TConfig(const QString& configFileName) :
    _configFileName(configFileName)
{
    if (_configFileName.isEmpty()) {
        _errorString = "Configuration file name cannot be empty";

        return;
    }
    if (!QFileInfo::exists(_configFileName)) {
        _errorString = QString("Configuration file not exist. File name: %1").arg(_configFileName);

        return;
    }

    qInfo() << QString("Reading configuration from %1").arg(_configFileName);

    QSettings ini(_configFileName, QSettings::IniFormat);

    QStringList groups = ini.childGroups();
    if (!groups.contains("DATABASE"))
    {
        _errorString = "Configuration file not contains [DATABASE] group";

        return;
    }

    //Database
    ini.beginGroup("DATABASE");
    _dbConnectionInfo.db_Driver = ini.value("Driver", "QODBC").toString();
    if (_dbConnectionInfo.db_Driver.isEmpty())
    {
        _errorString = "Key value [DATABASE]/Driver cannot be empty";

        return;
    }
    _dbConnectionInfo.db_DBName = ini.value("DataBase", "DB").toString();
    if (_dbConnectionInfo.db_DBName.isEmpty())
    {
        _errorString = "Key value [DATABASE]/DB cannot be empty";

        return;
    }
    _dbConnectionInfo.db_UserName = ini.value("UID", "").toString();
    _dbConnectionInfo.db_Password = ini.value("PWD", "").toString();
    _dbConnectionInfo.db_ConnectOptions = ini.value("ConnectionOptions", "").toString();
    _dbConnectionInfo.db_Port = ini.value("Port", "").toUInt();
    _dbConnectionInfo.db_Host = ini.value("Host", "localhost").toString();
    ini.endGroup();

    //System
    ini.beginGroup("SYSTEM");

    _sys_DebugMode = ini.value("DebugMode", "0").toBool();

    ini.endGroup();

    //Bot
    ini.beginGroup("BOT");

    _bot_token = ini.value("Token", "").toString();
    if (_bot_token.isEmpty())
    {
        _errorString = "Key value [BOT]/Token cannot be empty";

        return;
    }

    _bot_updateId = ini.value("UpdateID", "").toULongLong();

    ini.endGroup();
}

TConfig::~TConfig()
{
    if (!isError())
    {
        save();
    }
}

bool TConfig::save()
{
    QSettings ini(_configFileName, QSettings::IniFormat);

    if (!ini.isWritable()) {
        _errorString = "Can not write configuration file " +  _configFileName;

        return false;
    }

    ini.clear();

    //Database
    ini.beginGroup("DATABASE");

    ini.remove("");

    ini.setValue("Driver", _dbConnectionInfo.db_Driver);
    ini.setValue("DataBase", _dbConnectionInfo.db_DBName);
    ini.setValue("UID", _dbConnectionInfo.db_UserName);
    ini.setValue("PWD", _dbConnectionInfo.db_Password);
    ini.setValue("ConnectionOprions", _dbConnectionInfo.db_ConnectOptions);
    ini.setValue("Port", _dbConnectionInfo.db_Port);
    ini.setValue("Host", _dbConnectionInfo.db_Host);

    ini.endGroup();

    //System
    ini.beginGroup("SYSTEM");

    ini.remove("");

    ini.setValue("DebugMode", _sys_DebugMode);

    ini.endGroup();


    //Bot
    ini.beginGroup("BOT");

    ini.remove("");

    ini.setValue("Token", _bot_token);
    ini.setValue("UpdateID", _bot_updateId);

    ini.endGroup();

    //сбрасываем буфер
    ini.sync();

    if (_sys_DebugMode)
    {
        qDebug() << QString("Save configuration to %1").arg(_configFileName);
    }

    return true;
}

void TConfig::set_bot_UpdateId(qint32 updateId)
{
    Q_ASSERT(_bot_updateId <= updateId);

    _bot_updateId = updateId;

    save();
}

QString TConfig::errorString()
{
    auto res = _errorString;
    _errorString.clear();

    return res;
}


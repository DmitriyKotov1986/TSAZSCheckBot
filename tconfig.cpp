//QT
#include <QSettings>
#include <QFileInfo>
#include <QDebug>
#include <QFileInfo>

#include "tconfig.h"

using namespace Common;

//static
static TConfig* configPtr = nullptr;
Q_GLOBAL_STATIC_WITH_ARGS(const QString, CONFIG_DB_NAME, ("Config"))
Q_GLOBAL_STATIC_WITH_ARGS(const QString, UPDATEID_PARAM_NAME, ("UpdateID"))

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

void TConfig::makeConfig(const QString& configFileName)
{
    if (configFileName.isEmpty())
    {
        qWarning() << "Configuration file name cannot be empty";

        return;
    }

    QSettings ini(configFileName, QSettings::IniFormat);

    if (!ini.isWritable())
    {
        qWarning() << QString("Can not write configuration file: %1").arg(configFileName);

        return;
    }

    ini.clear();

    //Database
    ini.beginGroup("DATABASE");

    ini.remove("");

    ini.setValue("Driver", "QMYSQL");
    ini.setValue("DataBase", "TSAZSCheckBotDB");
    ini.setValue("UID", "user");
    ini.setValue("PWD", "password");
    ini.setValue("ConnectionOptions", "");
    ini.setValue("Port", "3306");
    ini.setValue("Host", "localhost");

    ini.endGroup();

    //System
    ini.beginGroup("SYSTEM");

    ini.remove("");

    ini.setValue("DebugMode", true);

    ini.endGroup();


    //Bot
    ini.beginGroup("BOT");

    ini.remove("");

    ini.setValue("Token", "API token");

    ini.endGroup();

    //сбрасываем буфер
    ini.sync();

    qInfo() << QString("Save configuration to %1").arg(configFileName);
}

//public
TConfig::TConfig(const QString& configFileName)
    : _configFileName(configFileName)
{
    if (_configFileName.isEmpty())
    {
        _errorString = "Configuration file name cannot be empty";

        return;
    }
    if (!QFileInfo::exists(_configFileName))
    {
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
    _dbConnectionInfo.driver = ini.value("Driver", "QODBC").toString();
    if (_dbConnectionInfo.driver.isEmpty())
    {
        _errorString = "Key value [DATABASE]/Driver cannot be empty";

        return;
    }
    _dbConnectionInfo.dbName = ini.value("DataBase", "DB").toString();
    if (_dbConnectionInfo.dbName.isEmpty())
    {
        _errorString = "Key value [DATABASE]/DB cannot be empty";

        return;
    }
    _dbConnectionInfo.userName = ini.value("UID", "").toString();
    _dbConnectionInfo.password = ini.value("PWD", "").toString();
    _dbConnectionInfo.connectOptions = ini.value("ConnectionOptions", "").toString();
    _dbConnectionInfo.port = ini.value("Port", "").toUInt();
    _dbConnectionInfo.host = ini.value("Host", "localhost").toString();
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

    ini.endGroup();
}

TConfig::~TConfig()
{
    delete _dbConfig;
}

qint32 TConfig::bot_updateId()
{
    loadFromDB();

    auto valueStr = _dbConfig->getValue(*UPDATEID_PARAM_NAME);
    bool ok = false;
    qint32 result = valueStr.toInt(&ok);

    if (!ok)
    {
        qWarning() << QString("Key value [ConfigDB]/UpdateID must be number. Value: %1").arg(valueStr);

        set_bot_UpdateId(result);
    }

    return result;
}

void TConfig::set_bot_UpdateId(qint32 updateId)
{
    loadFromDB();

    Q_ASSERT(_dbConfig->getValue(*UPDATEID_PARAM_NAME).toInt() <= updateId);

    _dbConfig->setValue(*UPDATEID_PARAM_NAME, QString::number(updateId));
}

QString TConfig::errorString()
{
    auto res = _errorString;
    _errorString.clear();

    return res;
}

void TConfig::errorOccurredDBConfig(Common::EXIT_CODE errorCode, const QString &errorString)
{
    _errorString = errorString;

    emit errorOccurred(errorCode, errorString);
}

void TConfig::loadFromDB()
{
    if (_dbConfig)
    {
        return;
    }

    _dbConfig = new TDBConfig(_dbConnectionInfo, *CONFIG_DB_NAME);

    QObject::connect(_dbConfig, SIGNAL(errorOccurred(Common::EXIT_CODE, const QString&)), SLOT(errorOccurredDBConfig(Common::EXIT_CODE, const QString&)));

    if (_dbConfig->isError())
    {
            _errorString = _dbConfig->errorString();
    }
}


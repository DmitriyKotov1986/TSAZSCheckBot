//Qt
#include <QCoreApplication>
#include <QLocale>
#include <QTranslator>
#include <QTimer>
#include <QCommandLineParser>
#include <QFileInfo>

//My
#include <Common/common.h>
#include <Common/tdbloger.h>

#include "tconfig.h"
#include "core.h"

using namespace Common;

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, ""); //настраиваем локаль
    qInstallMessageHandler(messageOutput);

    QCoreApplication a(argc, argv);
    QCoreApplication::setApplicationName("TSAZSCheckBot");
    QCoreApplication::setOrganizationName("OOO 'SA'");
    QCoreApplication::setApplicationVersion(QString("Version:0.1 Build: %1 %2").arg(__DATE__).arg(__TIME__));

    QTranslator translator;
    const QString baseName = "TSAZSCheckBot_ru_RU.qm";
    if (translator.load(baseName))
    {
        a.installTranslator(&translator);
    }

    //Создаем парсер параметров командной строки
    QCommandLineParser parser;
    parser.setApplicationDescription("The Telegram TS AZS check bot");
    parser.addHelpOption();
    parser.addVersionOption();

    //добавляем опцию Config
    QCommandLineOption config(QStringList() << "c" << "config", "Config file name", "FileName", QString("%1/%2.ini").arg(a.applicationDirPath()).arg(a.applicationName()));
    parser.addOption(config);

    //добавляем опцию MakeConfig
    QCommandLineOption makeConfig(QStringList() << "m" << "makeconfig", "Create new config file");
    parser.addOption(makeConfig);

    //Парсим опции командной строки
    parser.process(a);

    if (parser.isSet(makeConfig))
    {
        TConfig::makeConfig(QFileInfo(parser.value(config)).absoluteFilePath());

        return EXIT_CODE::OK;
    }

    TConfig* cnf = nullptr;
    TDBLoger* loger = nullptr;
    Core* core = nullptr;
    try
    {
        cnf = TConfig::config(parser.value(config));
        if (cnf->isError())
        {
            throw StartException(EXIT_CODE::LOAD_CONFIG_ERR, QString("Error load configuration: %1").arg(cnf->errorString()));
        }

        //настраиваем подключение БД логирования
        loger = Common::TDBLoger::DBLoger(cnf->dbConnectionInfo(), "TSAZSCheckBotLog", cnf->sys_DebugMode());

        //создаем и запускаем сервис
        core = new Core();
        if (core->isError())
        {
            throw StartException(EXIT_CODE::SERVICE_INIT_ERR, QString("Core initialization error: %1").arg(core->errorString()));
        }

        loger->start();
        if (loger->isError())
        {
            throw StartException(EXIT_CODE::START_LOGGER_ERR, QString("Loger initialization error. Error: %1").arg(loger->errorString()));
        }
    }

    catch (const StartException& err)
    {
        delete core;
        TDBLoger::deleteDBLoger();
        TConfig::deleteConfig();

        qCritical() << err.what();

        return err.exitCode();
    }

    //Таймер запуска
    QTimer startTimer;
    startTimer.setSingleShot(true);

    QObject::connect(&startTimer, SIGNAL(timeout()), core, SLOT(start()));

    startTimer.start(0);

    const auto res = a.exec();

    core->stop();

    delete core;
    TDBLoger::deleteDBLoger();
    TConfig::deleteConfig();

    return res;
}



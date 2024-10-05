//Qt
#include <QCoreApplication>
#include <QLocale>
#include <QTranslator>
#include <QTimer>

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
    if ((argc > 1) && (!std::strcmp(argv[1], std::string("--version").c_str())))
    {
        QTextStream outStream(stdout);
        outStream << QString("%1 %2\n").arg(QCoreApplication::applicationName()).arg(QCoreApplication::applicationVersion());

        return EXIT_CODE::OK;
    }

    const QString applicationDirName = QFileInfo(argv[0]).absolutePath();
    const QString configFileName = QString("%1/%2.ini").arg(applicationDirName).arg(QCoreApplication::applicationName());

    TConfig* cnf = nullptr;
    TDBLoger* loger = nullptr;
    Core* core = nullptr;
    try
    {
        cnf = TConfig::config(configFileName);
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
    QTimer StartTimer;
    StartTimer.setSingleShot(true);

    QObject::connect(&StartTimer, SIGNAL(timeout()), core, SLOT(start()));

    StartTimer.start(0);

    const auto res = a.exec();

    core->stop();

    delete core;
    TDBLoger::deleteDBLoger();
    TConfig::deleteConfig();

    return res;
}



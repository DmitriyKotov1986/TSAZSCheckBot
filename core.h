///////////////////////////////////////////////////////////////////////////////
/// Класс ядра. Управляет созданием и удалением объект, обработкой и
///     сохранением логов и начальной загрузкой
///
/// (с) Dmitriy Kotov, 2024
///////////////////////////////////////////////////////////////////////////////
#pragma once

//QT
#include <QObject>
#include <QTimer>

//My
#include <Telegram/TelegramBotAPI.h>
#include <Common/tdbloger.h>

#include "tconfig.h"
#include "users.h"
#include "questionnaire.h"
#include "filedownloader.h"

////////////////////////////////////////////////////////////////////////////////
/// Класс ядра
///
class Core
    : public QObject
{
    Q_OBJECT

public:
    explicit Core(QObject *parent = nullptr);

    /*!
        Деструктор
    */
    ~Core();

    QString errorString();
    bool isError() const { return !_errorString.isEmpty(); }

public slots:
    void start();
    void stop();

private slots:
    void errorOccuredLoger(Common::EXIT_CODE errorCode, const QString &errorString);
    void errorOccuredUsers(Common::EXIT_CODE errorCode, const QString &errorString);
    void errorOccuredQuestionnaire(Common::EXIT_CODE errorCode, const QString &errorString);

    void errorOccuredBot(Telegram::Error error);
    void networkErrorOccuredBot(Telegram::Error error);
    void messageReceivedBot(qint32 update_id, Telegram::Message message);
    void callbackQueryReceivedBot(qint32 message_id, Telegram::CallbackQuery callback_query);
    void updateDataFromServer();

    void downloadCompliteDownloader(qint32 chatId, qint32 userId, const QByteArray& data);
    void downloadErrorDownloader(qint32 chatId, qint32 userId);
    void sendLogMsgDownloader(Common::TDBLoger::MSG_CODE category, const QString& msg);

private:
    enum class UserEditAction: quint8
    {
        UNBLOCK = 0,
        BLOCK = 1
    };

private:
    static quint64 UUIDtoInt(const QUuid& uuid);

private:
    Q_DISABLE_COPY_MOVE(Core)

    void initUser(qint32 chatId, const Telegram::Message& message);
    void removeUser(qint32 chatId, qint32 userId);

    void rebootUsers(const QString& userMessage);

    void startQuestionnaire(qint32 chatId, qint32 userId);
    void finishQuestionnaire(qint32 chatId, qint32 userId);
    void resultQuestionnaire(qint32 chatId, qint32 userId, const QDateTime& start, const QDateTime& end);
    void loadQuestionnaire(qint32 chatId, qint32 userId);
    void saveQuestionnaire(qint32 chatId, qint32 userId);
    void nextQuestions(qint32 chatId, qint32 userId, qint32 questionId);
    void saveAnswer(qint32 chatId, qint32 userId, qint32 questionId, const QVariant& answer);

    void startUsersEdit(qint32 chatId, qint32 userId);
    void usersSelectList(qint32 chatId, qint32 userId, UserEditAction action);
    void userConfirm(qint32 chatId, qint32 userId, qint32 userWorkId);
    void userBlock(qint32 chatId, qint32 userId, qint32 userWorkId);

    void selectDate(qint32 chatId, qint32 userId);

    void cancel(qint32 chatId, qint32 userId);

    void setUserState(qint32 chatId, qint32 userId, ::User::EUserState state);

    void startButton(qint32 chatId, ::User::EUserRole role);
    void startUserButton(qint32 chatId);
    void startAdminButton(qint32 chatId);
    void startQuestionnaireButton(qint32 chatId);
    void clearButton(qint32 chatId);
    void cancelButton(qint32 chatId);

private:
    TConfig* _cnf = nullptr;            ///< Глобальная конфигурация
    Common::TDBLoger* _loger = nullptr; ///< Глобальны логер

    QString _errorString;

    bool _isStarted = false;

    Telegram::Bot* _bot;

    QTimer* _updateTimer = nullptr;

    Users* _users = nullptr; 
    Questionnaire* _questionnaire = nullptr;

    FileDownloader* _fileDownloader = nullptr;

}; // class Core

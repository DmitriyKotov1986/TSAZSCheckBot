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
    void errorOccurredLoger(Common::EXIT_CODE errorCode, const QString &errorString);
    void errorOccurredUsers(Common::EXIT_CODE errorCode, const QString &errorString);
    void errorOccurredQuestionnaire(Common::EXIT_CODE errorCode, const QString &errorString);
    void errorOccurredConfig(Common::EXIT_CODE errorCode, const QString &errorString);

    void errorOccurredBot(Telegram::Error error);
    void networkerrorOccurredBot(Telegram::Error error);
    void messageReceivedBot(qint32 update_id, Telegram::Message message);
    void callbackQueryReceivedBot(qint32 message_id, Telegram::CallbackQuery callback_query);
    void updateDataFromServer();

    void downloadCompliteDownloader(qint64 chatId, qint64 userId, const QByteArray& data);
    void downloadErrorDownloader(qint64 chatId, qint64 userId);
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

    void sendMessage(const Telegram::FunctionArguments::SendMessage& arguments);

    void initUser(qint64 chatId, const Telegram::Message& message);
    void removeUser(qint64 chatId, qint64 userId);

    void rebootUsers(const QString& userMessage);

    void startQuestionnaire(qint64 chatId, qint64 userId);
    void finishQuestionnaire(qint64 chatId, qint64 userId);
    void resultQuestionnaire(qint64 chatId, qint64 userId, const QDateTime& start, const QDateTime& end);
    void loadQuestionnaire(qint64 chatId, qint64 userId);
    void saveQuestionnaire(qint64 chatId, qint64 userId);
    void nextQuestions(qint64 chatId, qint64 userId, qint32 questionId);
    void saveAnswer(qint64 chatId, qint64 userId, qint32 questionId, const QVariant& answer);

    void startUsersEdit(qint64 chatId, qint64 userId);
    void usersSelectList(qint64 chatId, qint64 userId, UserEditAction action);
    void userConfirm(qint64 chatId, qint64 userId, qint64 userWorkId);
    void userBlock(qint64 chatId, qint64 userId, qint64 userWorkId);

    void selectDate(qint64 chatId, qint64 userId);

    void cancel(qint64 chatId, qint64 userId);

    void setUserState(qint64 chatId, qint64 userId, ::User::EUserState state);

    void startButton(qint64 chatId, ::User::EUserRole role);
    void startUserButton(qint64 chatId);
    void startAdminButton(qint64 chatId);

    static Telegram::ReplyKeyboardMarkup startFinishButton();
    void startQuestionnaireButton(qint64 chatId);

    void clearButton(qint64 chatId);
    void cancelButton(qint64 chatId);

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

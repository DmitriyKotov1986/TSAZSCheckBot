//QT
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QSqlError>
#include <QCoreApplication>

//My
#include <Common/common.h>

#include "buttondata.h"

#include "core.h"

using namespace Common;
using namespace Telegram;

static const QString CORE_CONNECTION_TO_DB_NAME = "CoreDB";

Core::Core(QObject *parent)
    : QObject{parent}
    , _cnf(TConfig::config())
    , _loger(Common::TDBLoger::DBLoger())
{
    Q_CHECK_PTR(_cnf);
    Q_CHECK_PTR(_loger);

    QObject::connect(_loger, SIGNAL(errorOccurred(Common::EXIT_CODE, const QString&)),
                     SLOT(errorOccuredLoger(Common::EXIT_CODE, const QString&)), Qt::DirectConnection);

}

Core::~Core()
{
    stop();
}

QString Core::errorString()
{
    auto res = _errorString;
    _errorString.clear();

    return res;
}

void Core::start()
{
    Q_ASSERT(!_isStarted);

    //Load users
    _users = new Users(_cnf->dbConnectionInfo());

    QObject::connect(_users, SIGNAL(errorOccured(Common::EXIT_CODE, const QString&)), SLOT(errorOccuredUsers(Common::EXIT_CODE, const QString&)));

    _users->loadFromDB();

    _loger->sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("Total users: %1").arg(_users->usersCount()));

    //Load Questionnaire
    _questionnaire = new Questionnaire(_cnf->dbConnectionInfo());

    QObject::connect(_questionnaire, SIGNAL(errorOccured(Common::EXIT_CODE, const QString&)), SLOT(errorOccuredQuestionnaire(Common::EXIT_CODE, const QString&)));

    _questionnaire->loadFromDB();

    //Make TG bot
    auto botSettings = std::make_shared<BotSettings>(_cnf->bot_token());

    _bot = new Bot(botSettings);

    //	Telegram::Bot::errorOccured is emitted every time when the negative response is received from the Telegram server and holds an Error object in arguments. Error class contains the occurred error description and code. See Error.h for details
    QObject::connect(_bot, SIGNAL(errorOccured(Telegram::Error)), SLOT(errorOccuredBot(Telegram::Error)));

    //	Telegram::Bot::networkErrorOccured is emitted every time when there is an error while receiving Updates from the Telegram. Error class contains the occurred error description and code. See Error.h for details
    QObject::connect(_bot, SIGNAL(networkErrorOccured(Telegram::Error)), SLOT(networkErrorOccuredBot(Telegram::Error)));

    // Connecting messageReceived() signal to a lambda that sends the received message back to the sender
    QObject::connect(_bot, SIGNAL(messageReceived(qint32, Telegram::Message)), SLOT(messageReceivedBot(qint32, Telegram::Message)));

    // Emited when bot reseive new incoming callback query */
    QObject::connect(_bot, SIGNAL(callbackQueryReceived(qint32, Telegram::CallbackQuery)), SLOT(callbackQueryReceivedBot(qint32, Telegram::CallbackQuery)));

    qDebug() << "Server bot data: " << _bot->getMe().get().toObject();	// To get basic data about your bot in form of a User object

    // Start update timer
    _updateTimer = new QTimer();

    QObject::connect(_updateTimer, SIGNAL(timeout()), SLOT(updateDataFromServer()));

    _updateTimer->start(5000);

    _loger->sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, "Bot started successfully");

    _isStarted = true;
}

void Core::stop()
{   if (!_isStarted)
    {
        return;
    }

    delete _updateTimer;

    delete _bot;

    delete _users;

     _loger->sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, "Bot stoped");

    _isStarted = false;
}

void Core::errorOccuredLoger(Common::EXIT_CODE errorCode, const QString &errorString)
{
    QString msg = QString("Critical error while the Loger is running. Code: %1 Message: %2").arg(errorCode).arg(errorString);
    qCritical() << msg;

    QCoreApplication::exit(errorCode);
}

void Core::errorOccuredUsers(Common::EXIT_CODE errorCode, const QString &errorString)
{
    QString msg = QString("Critical error while the Users is running. Code: %1 Message: %2").arg(errorCode).arg(errorString);
    qCritical() << msg;
    _loger->sendLogMsg(TDBLoger::MSG_CODE::CRITICAL_CODE, msg);

    QCoreApplication::exit(errorCode);
}

void Core::errorOccuredQuestionnaire(Common::EXIT_CODE errorCode, const QString &errorString)
{
    QString msg = QString("Critical error while the Questionnaire is running. Code: %1 Message: %2").arg(errorCode).arg(errorString);
    qCritical() << msg;
    _loger->sendLogMsg(TDBLoger::MSG_CODE::CRITICAL_CODE, msg);

    QCoreApplication::exit(errorCode);
}

void Core::errorOccuredBot(Telegram::Error error)
{
    _loger->sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE,
        QString("Got negative response is received from the Telegram server. Code: %1. Message: %2")
            .arg(error.error_code)
            .arg(error.description));
}

void Core::networkErrorOccuredBot(Telegram::Error error)
{
    _loger->sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE,
                       QString("Got is an error while receiving Updates from the Telegram server. Code: %1. Message: %2")
                           .arg(error.error_code)
                           .arg(error.description));
}

void Core::messageReceivedBot(qint32 update_id, Telegram::Message message)
{
    _loger->sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("Get message from: %1. Update ID: %2. Message: %3")
                                                                 .arg(message.from->id)
                                                                 .arg(update_id)
                                                                 .arg(message.text.has_value() ? message.text.value() : "NO TEXT"));


    if (!message.text.has_value())
    {
        _bot->sendMessage(message.chat->id, tr("Support only text command"));

        return;
    }

    const auto cmd = message.text.value();
    if (cmd == "/start")
    {
        initUser(message.chat->id, message);
    }
    else  if (cmd == "/stop")
    {
        removeUser(message.chat->id, message.from->id);
    }
    else  if (cmd == "/help")
    {
        _bot->sendMessage(message.chat->id, tr("Support commands:\n/start - start bot. \n/stop - stop bot. \n/help - this help."));
    }
    else if (cmd == tr("Start"))
    {
        startQuestionnaire(message.chat->id, message.from->id);
    }
    else if (cmd == tr("Finish"))
    {
        finishQuestionnaire(message.chat->id, message.from->id);
    }
    else if (cmd == tr("Cancel"))
    {
        cancelQuestionnaire(message.chat->id, message.from->id);
    }
    else if (cmd == tr("Results"))
    {
        resultQuestionnaire(message.chat->id, message.from->id);
    }
    else if (cmd == tr("Users"))
    {

    }
    else if (cmd == tr("Load"))
    {
        loadQuestionnaire(message.chat->id, message.from->id);
    }

    else if (cmd == tr("Save"))
    {
        saveQuestionnaire(message.chat->id, message.from->id);
    }
    else
    {
        _bot->sendMessage(message.chat->id, tr("Unsupport command"));
    }
}

void Core::callbackQueryReceivedBot(qint32 message_id, Telegram::CallbackQuery callback_query)
{
    _bot->answerCallbackQuery(callback_query.id);

    if (!callback_query.data.has_value())
    {
        _loger->sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("User press undefine inline button. Skip. User: %1. Button text: %2")
                                                                 .arg(callback_query.from.id)
                                                                 .arg((callback_query.message.has_value() && callback_query.message->text.has_value()) ? callback_query.message->text.value() : ""));
        return;
    }

    const auto data = ButtonData(callback_query.data.value());

    _loger->sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("User press inline button. User: %1. Button text: %2. Button data: %3")
                                                             .arg(callback_query.from.id)
                                                             .arg((callback_query.message.has_value() && callback_query.message->text.has_value()) ? callback_query.message->text.value() : "")
                                                             .arg(data.toString()));

    const auto buttonName = data.getParam("N");
    if (buttonName == "ANSWER")
    {
        _bot->answerCallbackQuery(callback_query.id);

        const auto userId = data.getParam("U").toInt();
        const auto chatId = data.getParam("C").toInt();
        const auto questionId = data.getParam("Q").toInt();
        const auto answerId = data.getParam("A").toInt();
        auto& user = _users->getUser(userId);
        const auto& uuid = user.currentQUuid();
        if (uuid.isNull())
        {
            _bot->sendMessage(chatId, tr("Questionnaire is not started. Retry"));

            startButton(chatId, user.role());

            _loger->sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("User press button for not started questionnaire. User ID: %1. Questionnaire: %2. Question ID: %3. Answer ID: %4")
                                                                     .arg(userId)
                                                                     .arg(uuid.toString())
                                                                     .arg(questionId)
                                                                     .arg(answerId));

            return;
        }
        if (QString::number(UUIDtoInt(uuid)) != data.getParam("I"))
        {
            _bot->sendMessage(chatId, tr("Questionnaire for this button already finished"));

            _loger->sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("User press button for already finished questionnaire. User ID: %1. Questionnaire: %2. Question ID: %3. Answer ID: %4")
                                                                         .arg(userId)
                                                                         .arg(uuid.toString())
                                                                         .arg(questionId)
                                                                         .arg(answerId));

            return;
        }
\
        const auto maxQuestionId = user.maxQuestionId();
        const auto nextQuestionID = user.setAnswer(questionId , answerId);

        _loger->sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("Get answer from user. User ID: %1. Questionnaire: %2. Question ID: %3. Answer ID: %4")
                                                                 .arg(userId)
                                                                 .arg(uuid.toString())
                                                                 .arg(questionId)
                                                                 .arg(answerId));

        if (nextQuestionID < 0)
        {
            _bot->sendMessage(chatId, tr("Congratulations! You have answered all the questions. Click 'Finish' to save the result"));

            return;
        }

        if (nextQuestionID > maxQuestionId)
        {
            nextQuestions(chatId, userId, nextQuestionID);
        }
    }
    else
    {
        Q_ASSERT(false);
    }

}

void Core::updateDataFromServer()
{
    Q_ASSERT(_isStarted);
    Q_CHECK_PTR(_bot);

    const auto newUpdateId = _bot->update(_cnf->bot_updateId());

    if (newUpdateId != _cnf->bot_updateId())
    {
        _cnf->set_bot_UpdateId(newUpdateId);
    }
}

void Core::initUser(qint32 chatId, const Telegram::Message& message)
{
    const auto userId = message.from->id;
    if (_users->userExist(userId))
    {
        const auto role = _users->userRole(userId);
        switch (role)
        {
        case ::User::EUserRole::USER:
        case ::User::EUserRole::ADMIN:
            startButton(chatId, role);
            break;
        case ::User::EUserRole::NO_CONFIRMED:
            _bot->sendMessage(chatId, tr("Please wait for account confirmation from the administrator"));
            break;
        case ::User::EUserRole::DELETED:
            _bot->sendMessage(chatId, tr("Please wait for account unblocked from the administrator"));
            break;
        case ::User::EUserRole::UNDEFINED:
        default:
            Q_ASSERT(false);
        }

        return;
    }

    const ::User user(userId,
         message.from->first_name,
         message.from->last_name.has_value() ? message.from->last_name.value() : "",
         message.from->username.has_value() ? message.from->username.value() : "");

    _users->addUser(user);

    // Sending reply keyboarduserId
    _bot->sendMessage(chatId, tr("Welcome to TS AZS Check Bot. Please wait for account confirmation from the administrator"));
}

void Core::removeUser(qint32 chatId, qint32 userId)
{
    _users->removeUser(userId);

    // Sending reply keyboard
    _bot->sendMessage({.chat_id = chatId,
                       .text = tr("Good bye"),
                       .reply_markup = ReplyKeyboardMarkup{}});
}

void Core::startQuestionnaire(qint32 chatId, qint32 userId)
{
    const auto role = _users->userRole(userId);
    if (role != ::User::EUserRole::USER && role != ::User::EUserRole::ADMIN)
    {
        _loger->sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("Try to start questionnaire for user has role: %1. User ID: %2:")
                                                                 .arg(static_cast<quint8>(role)).arg(userId));
        _bot->sendMessage(chatId, tr("User is deleted or no confirmed. Please send command /start or try letter"));

        return;
    }

    KeyboardButton finishButton(tr("Finish"));
    KeyboardButton cancelButton(tr("Cancel"));

    ReplyKeyboardMarkup keyboard = {{finishButton}, {cancelButton}};
  //  keyboard .resize_keyboard = true;

    // Sending reply keyboard
    _bot->sendMessage({.chat_id = chatId,
                       .text = tr("Please click 'Finish' button finished and save result or 'Cancel' to cancel"),
                       .reply_markup = keyboard });


    auto& user = _users->getUser(userId);
    const auto questionID = user.startQuestionnaire(_questionnaire);

    nextQuestions(chatId, userId, questionID);

    _loger->sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("Start questionnaire. User ID: %1. Questionnaire: %2").arg(userId).arg(user.currentQUuid().toString()));
}

void Core::finishQuestionnaire(qint32 chatId, qint32 userId)
{
    auto& user = _users->getUser(userId);
    const auto uuid = user.currentQUuid(); // there not reference
    if (uuid.isNull())
    {
        _bot->sendMessage(chatId, tr("Questionnaire is not started. Retry"));

        startButton(chatId, user.role());

        return;
    }

    user.finishQuestionnaire();

    _bot->sendMessage(chatId, tr("Questionnaire finished. Results saved as: %1").arg(uuid.toString()));

    startButton(chatId, user.role());

    _loger->sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("Finish questionnaire. User ID: %1. Questionnaire: %2").arg(userId).arg(uuid.toString()));
}

void Core::cancelQuestionnaire(qint32 chatId, qint32 userId)
{
    auto& user = _users->getUser(userId);
    const auto& uuid = user.currentQUuid();
    if (uuid.isNull())
    {
        _bot->sendMessage(chatId, tr("Questionnaire is not started. Retry"));

        startButton(chatId, user.role());

        return;
    }

    user.cancelQuestionnaire();

    _bot->sendMessage(chatId, tr("Questionnaire has been canceled"));

    startButton(chatId, user.role());

    _loger->sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("Cancel questionnaire. User ID: %1. Questionnaire: %2").arg(userId).arg(uuid.toString()));
}

void Core::resultQuestionnaire(qint32 chatId, qint32 userId)
{
    _bot->sendChatAction(chatId, Telegram::Bot::ChatActionType::UPLOAD_DOCUMENT);

    const auto results = _questionnaire->getAllResults();

    const auto fileName = QString("%1/Results/AllResults_%2.json").arg(QCoreApplication::applicationDirPath()).arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    QFile file(fileName);
    if (!file.open(QIODeviceBase::WriteOnly))
    {
        _loger->sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("Cannot save file: %1. Error: %2").arg(file.fileName()).arg(file.errorString()));

        return;
    }

    file.write(results.toJson());

    file.close();

    file.open(QIODeviceBase::ReadOnly);

    _bot->sendDocument(chatId, &file);

    file.close();

    _loger->sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("File with questionnaires results sended. File: %1. User ID: %2").arg(file.fileName()).arg(userId));
}

void Core::loadQuestionnaire(qint32 chatId, qint32 userId)
{

}

void Core::saveQuestionnaire(qint32 chatId, qint32 userId)
{
    _bot->sendChatAction(chatId, Telegram::Bot::ChatActionType::UPLOAD_DOCUMENT);

    const auto results = _questionnaire->toString();

    const auto fileName = QString("%1/QuestionnaireSave/Questionnaire_%2.json").arg(QCoreApplication::applicationDirPath()).arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    QFile file(fileName);
    if (!file.open(QIODeviceBase::WriteOnly))
    {
        _loger->sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("Cannot save file: %1. Error: %2").arg(file.fileName()).arg(file.errorString()));

        return;
    }

    file.write(results.toUtf8());

    file.close();

    file.open(QIODeviceBase::ReadOnly);

    _bot->sendDocument(chatId, &file);

    file.close();

    _loger->sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("File with questionnaire sended. File: %1. User ID: %2").arg(file.fileName()).arg(userId));
}

void Core::startButton(qint32 chatId, ::User::EUserRole role)
{
    Q_ASSERT(role != ::User::EUserRole::UNDEFINED);

    switch (role)
    {
    case ::User::EUserRole::USER:
        startUserButton(chatId);
        break;
    case ::User::EUserRole::ADMIN:
        startAdminButton(chatId);
        break;
    default:
        Q_ASSERT(false);
        break;
    }
}

void Core::startUserButton(qint32 chatId)
{
    KeyboardButton startButton(tr("Start"));

    ReplyKeyboardMarkup keyboard = {{startButton}};

    // Sending reply keyboard
    _bot->sendMessage({.chat_id = chatId,
                       .text = tr("Please click 'Start' button for new check AZS"),
                       .reply_markup = keyboard});
}

void Core::startAdminButton(qint32 chatId)
{
    KeyboardButton startButton(tr("Start"));
    KeyboardButton usersButton(tr("Users"));
    KeyboardButton saveButton(tr("Save"));
    KeyboardButton loadButton(tr("Load"));
    KeyboardButton resultsButton(tr("Results"));

    ReplyKeyboardMarkup keyboard = {{startButton, resultsButton}, {loadButton, saveButton}, {usersButton}};

    // Sending reply keyboard
    _bot->sendMessage({.chat_id = chatId,
                       .text = tr("Please click:\n'Start' button for new check AZS\n'Users' for edit users data\n'Load' for update questionnaire\n'Results' for download questionnaire results"),
                       .reply_markup = keyboard});
}

void Core::nextQuestions(qint32 chatId, qint32 userId, qint32 questionId)
{
    const auto& user = _users->getUser(userId);
    const auto& uuid = user.currentQUuid();
    const auto& question = _questionnaire->question(questionId);
    QVector<QVector<InlineKeyboardButton>> buttons;
    QVector<InlineKeyboardButton> questionButtons;

    for (const auto& answer: question.getAnswersList())
    {
        ButtonData data;
        data.setParam("N", "ANSWER");
        data.setParam("Q", QString::number(questionId));
        data.setParam("A", QString::number(answer.first));
        data.setParam("U", QString::number(userId));
        data.setParam("C", QString::number(chatId));
        data.setParam("I", QString::number(UUIDtoInt(uuid)));
        InlineKeyboardButton questionButton(answer.second, std::nullopt, std::nullopt, data.toString());

        questionButtons.push_back(questionButton);

        if (questionButtons.size() == 2)
        {
            buttons.push_back(questionButtons);
            questionButtons.clear();
        }
    }

    if (!questionButtons.empty())
    {
        buttons.push_back(questionButtons);
    }

    InlineKeyboardMarkup inlineButtons(buttons);

    // Sending all inline buttons
    _bot->sendMessage({.chat_id = chatId,
                       .text = QString("(%1/%2) %3").arg(_questionnaire->questionPosition(questionId)).arg(_questionnaire->questionCount()).arg(question.text()),
                       .reply_markup = inlineButtons});
}

quint64 Core::UUIDtoInt(const QUuid &uuid)
{
    return uuid.data1 + uuid.data2 + uuid.data3;
}

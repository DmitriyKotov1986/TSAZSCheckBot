//QT
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QSqlError>
#include <QCoreApplication>
#include <QVariant>

//My
#include <Common/common.h>

#include "buttondata.h"
#include "qassert.h"
#include "user.h"
#include "chat.h"
#include "core.h"

using namespace Common;
using namespace Telegram;

quint64 Core::UUIDtoInt(const QUuid &uuid)
{
    return uuid.data1 + uuid.data2 + uuid.data3;
}

void Core::sendMessage(const Telegram::FunctionArguments::SendMessage &arguments)
{
    Q_CHECK_PTR(_bot);

    _bot->sendMessage(arguments);

    QString chatIdStr;
    if (const auto chat_id_qint64 = std::get_if<qint64>(&arguments.chat_id))
    {
        chatIdStr = QString::number(*chat_id_qint64);
    }
    else if (const auto chat_id_string = std::get_if<QString>(&arguments.chat_id))
    {
        chatIdStr = *chat_id_string;
    }

    auto msg = QString("Chat ID: %1 Text: '%2'").arg(chatIdStr).arg(arguments.text);

    if (arguments.reply_markup.has_value())
    {
        const auto& replyMarkup = arguments.reply_markup.value();

        if (const auto buttonsList = std::get_if<InlineKeyboardMarkup>(&replyMarkup))
        {
            msg += QString(" Inline buttons: [");
            qint32 line = 1;
            for (const auto& buttonsGroup: buttonsList->inline_keyboard)
            {
                msg += QString("Line %1:(").arg(line);
                for (const auto& button: buttonsGroup)
                {
                    msg += QString("Text: '%1' Data: %2;").arg(button.text).arg(button.callback_data.has_value() ? button.callback_data.value() : "");
                }
                msg.removeLast();
                msg += QString(");");
                ++line;
            }
            msg.removeLast();
            msg += QString("]");
        }
        else if (const auto buttonsList = std::get_if<ReplyKeyboardMarkup>(&replyMarkup))
        {
            msg += QString(" ReplyKeyboard buttons: [");
            qint32 line = 1;
            for (const auto& buttonsGroup: buttonsList->keyboard)
            {
                msg += QString("Line %1:(").arg(line);
                for (const auto& button: buttonsGroup)
                {
                    msg += QString("Text: %1;").arg(button.text);
                }
                msg.removeLast();
                msg += QString(");");
                ++line;
            }
            msg.removeLast();
            msg += QString("]");
        }
        else if (const auto buttonsList = std::get_if<ReplyKeyboardRemove>(&replyMarkup))
        {
            msg += QString(" ReplyKeyboardRemove");
        }
    }

    _loger->sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("Send to user: %1").arg(msg));
}

Core::Core(QObject *parent)
    : QObject{parent}
    , _cnf(TConfig::config())
    , _loger(Common::TDBLoger::DBLoger())
{
    Q_CHECK_PTR(_cnf);
    Q_CHECK_PTR(_loger);

    QObject::connect(_loger, SIGNAL(errorOccurred(Common::EXIT_CODE, const QString&)),
                     SLOT(errorOccurredLoger(Common::EXIT_CODE, const QString&)), Qt::DirectConnection);

    QObject::connect(_cnf, SIGNAL(errorOccurred(Common::EXIT_CODE, const QString&)), SLOT(errorOccurredConfig(Common::EXIT_CODE, const QString&)), Qt::DirectConnection);
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

    _fileDownloader  = new FileDownloader();

    QObject::connect(_fileDownloader, SIGNAL(downloadComplite(qint64, qint64, const QByteArray&)),
                     SLOT(downloadCompliteDownloader(qint64, qint64, const QByteArray&)));
    QObject::connect(_fileDownloader, SIGNAL(downloadError(qint64, qint64)),
                     SLOT(downloadErrorDownloader(qint64, qint64)));
    QObject::connect(_fileDownloader, SIGNAL(sendLogMsg(Common::TDBLoger::MSG_CODE, const QString&)),
                     SLOT(sendLogMsgDownloader(Common::TDBLoger::MSG_CODE, const QString&)));

    //Load users
    Q_ASSERT(_users == nullptr);
    _users = new Users(_cnf->dbConnectionInfo());

    QObject::connect(_users, SIGNAL(errorOccurred(Common::EXIT_CODE, const QString&)), SLOT(errorOccurredUsers(Common::EXIT_CODE, const QString&)));

    _users->loadFromDB();

    _loger->sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("Total users: %1").arg(_users->usersCount()));

    //Load Questionnaire
    Q_ASSERT(_questionnaire == nullptr);
    _questionnaire = new Questionnaire(_cnf->dbConnectionInfo());

    QObject::connect(_questionnaire, SIGNAL(errorOccurred(Common::EXIT_CODE, const QString&)), SLOT(errorOccurredQuestionnaire(Common::EXIT_CODE, const QString&)));

    _questionnaire->loadFromDB();

    //Make TG bot
    auto botSettings = std::make_shared<BotSettings>(_cnf->bot_token());

    _bot = new Bot(botSettings);

    //	Telegram::Bot::errorOccurred is emitted every time when the negative response is received from the Telegram server and holds an Error object in arguments. Error class contains the occurred error description and code. See Error.h for details
    QObject::connect(_bot, SIGNAL(errorOccurred(Telegram::Error)), SLOT(errorOccurredBot(Telegram::Error)));

    //	Telegram::Bot::networkerrorOccurred is emitted every time when there is an error while receiving Updates from the Telegram. Error class contains the occurred error description and code. See Error.h for details
    QObject::connect(_bot, SIGNAL(networkerrorOccurred(Telegram::Error)), SLOT(networkerrorOccurredBot(Telegram::Error)));

    // Connecting messageReceived() signal to a lambda that sends the received message back to the sender
    QObject::connect(_bot, SIGNAL(messageReceived(qint32, Telegram::Message)), SLOT(messageReceivedBot(qint32, Telegram::Message)));

    // Emited when bot reseive new incoming callback query */
    QObject::connect(_bot, SIGNAL(callbackQueryReceived(qint32, Telegram::CallbackQuery)), SLOT(callbackQueryReceivedBot(qint32, Telegram::CallbackQuery)));

    qDebug() << "Server bot data: " << _bot->getMe().get().toObject();	// To get basic data about your bot in form of a User object

    // Start update timer
    Q_ASSERT(_updateTimer == nullptr);
    _updateTimer = new QTimer();

    QObject::connect(_updateTimer, SIGNAL(timeout()), SLOT(updateDataFromServer()));

    _updateTimer->start(5000);

    _loger->sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, "Bot started successfully");

    rebootUsers(tr("The server has been rebooted by the administrator. Please start over"));

    _isStarted = true;
}

void Core::stop()
{   if (!_isStarted)
    {
        return;
    }

    delete _updateTimer;
    _updateTimer = nullptr;

    delete _bot;
    _bot = nullptr;

    delete _users;
    _users = nullptr;

    delete _fileDownloader;
    _fileDownloader = nullptr;

     _loger->sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, "Bot stoped");

    _isStarted = false;
}

void Core::errorOccurredLoger(Common::EXIT_CODE errorCode, const QString &errorString)
{
    QString msg = QString("Critical error while the Loger is running. Code: %1 Message: %2").arg(errorCode).arg(errorString);
    qCritical() << msg;

    QCoreApplication::exit(errorCode);
}

void Core::errorOccurredUsers(Common::EXIT_CODE errorCode, const QString &errorString)
{
    QString msg = QString("Critical error while the Users is running. Code: %1 Message: %2").arg(errorCode).arg(errorString);
    qCritical() << msg;
    _loger->sendLogMsg(TDBLoger::MSG_CODE::CRITICAL_CODE, msg);

    QCoreApplication::exit(errorCode);
}

void Core::errorOccurredQuestionnaire(Common::EXIT_CODE errorCode, const QString &errorString)
{
    QString msg = QString("Critical error while the Questionnaire is running. Code: %1 Message: %2").arg(errorCode).arg(errorString);
    qCritical() << msg;
    _loger->sendLogMsg(TDBLoger::MSG_CODE::CRITICAL_CODE, msg);

    QCoreApplication::exit(errorCode);
}

void Core::errorOccurredConfig(Common::EXIT_CODE errorCode, const QString &errorString)
{
    QString msg = QString("Critical error while the Config is running. Code: %1 Message: %2").arg(errorCode).arg(errorString);
    qCritical() << msg;
    _loger->sendLogMsg(TDBLoger::MSG_CODE::CRITICAL_CODE, msg);

    QCoreApplication::exit(errorCode);
}

void Core::errorOccurredBot(Telegram::Error error)
{
    _loger->sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE,
        QString("Got negative response is received from the Telegram server. Code: %1. Message: %2")
            .arg(error.error_code)
            .arg(error.description));
}

void Core::networkerrorOccurredBot(Telegram::Error error)
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

    const auto userId = message.from->id;
    const auto chatId = message.chat->id;

    if (message.document.has_value())
    {
        if (_users->userExist(userId))
        {
            auto& user = _users->user(userId);
            if (user.state() == ::User::EUserState::LOAD_QUESTIONNAIRE)
            {
                const auto& document = message.document.value();
                auto fileData = _bot->getFile(document.file_id);
                const auto file = fileData.get();
                if (!file.file_path.has_value())
                {
                    _loger->sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("Cannot download file. File no exist on server"));

                    sendMessage({.chat_id = chatId,
                                 .text = tr("Cannot load file from server. Try again")});

                    setUserState(chatId, userId, ::User::EUserState::READY);

                    return;
                }

                _bot->sendChatAction(chatId, Telegram::Bot::ChatActionType::UPLOAD_DOCUMENT);
                const auto url = QUrl(QString("https://api.telegram.org/file/bot%1/%2").arg(_cnf->bot_token()).arg(file.file_path.value()));
                _fileDownloader->download(chatId, userId, url);

                return;
            }
        }
    }

    if (!message.text.has_value())
    {
        sendMessage({.chat_id = message.chat->id,
                     .text = tr("Support only text command")});

        return;
    }

    const auto& cmd = message.text.value();

    if (_users->userExist(userId))
    {
        auto& user = _users->user(userId);
        if (user.state() == ::User::EUserState::QUESTIONNAIRE && user.currentQuestionId() > 0 &&
            _questionnaire->question(user.currentQuestionId()).type() == Question::EQuestionType::TEXT &&
            cmd != tr("Finish") && cmd != tr("Cancel"))
        {
            saveAnswer(chatId, userId, user.currentQuestionId(), QVariant(cmd));

            return;
        }
    }

    //команды доступные неавторизированным пользователям
    if (cmd == "/start")
    {
        initUser(chatId, message);

        return;
    }
    else  if (cmd == "/help")
    {
        sendMessage({.chat_id = chatId,
                     .text = tr("Support commands:\n/start - start bot\n/stop - stop bot\n/help - this help")});

        return;
    }

    //Проверяем есть ли такой пользователь
    if (!_users->userExist(userId) || _users->user_c(userId).role() == ::User::EUserRole::DELETED)
    {
        sendMessage({.chat_id = chatId,
                     .text = tr("Unknow command. Please send the /start command to start and send message to Administrator")});

        return;
    }

    //Команды доступные
    if (cmd == "/stop")
    {
        removeUser(chatId, userId);

        return;
    }

    //Проверяем активен ли пользователь
    const auto role = _users->user_c(userId).role();
    if (role != ::User::EUserRole::USER && role != ::User::EUserRole::ADMIN)
    {
        sendMessage({.chat_id = chatId,
                     .text = tr("Authorization has not been completed. Try letter")});

        return;
    }

    if (cmd == tr("Start"))
    {
        startQuestionnaire(chatId, userId);

        return;
    }
    else if (cmd == tr("Finish"))
    {
        finishQuestionnaire(chatId, userId);

        return;
    }
    else if (cmd == tr("Cancel"))
    {
        cancel(chatId, userId);

        return;
    }

    //Проверяем активен есть ли права админа
    if (role != ::User::EUserRole::ADMIN)
    {
        sendMessage({.chat_id = chatId,
                     .text = tr("Insufficient rights to perform the action")});

        return;
    }

    if (cmd == tr("Results"))
    {
        selectDate(chatId, userId);
    }
    else if (cmd == tr("Users"))
    {
        startUsersEdit(chatId, userId);
    }
    else if (cmd == tr("Load"))
    {
        loadQuestionnaire(chatId, userId);
    }
    else if (cmd == tr("Save"))
    {
        saveQuestionnaire(chatId, userId);
    }
    else
    {
        sendMessage({.chat_id = chatId,
                     .text = tr("Unsupport command")});
    }
}

void Core::callbackQueryReceivedBot(qint32 message_id, Telegram::CallbackQuery callback_query)
{
    Q_UNUSED(message_id);

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

        const auto userId = data.getParam("U").toLongLong();
        const auto chatId = data.getParam("C").toLongLong();

        auto& user = _users->user(userId);
        if (user.state() != ::User::EUserState::QUESTIONNAIRE)
        {
            sendMessage({.chat_id = chatId,
                         .text = tr("This button is not active. Please click 'Start' on start menu for start questionnaire")});

            return;
        }

        const auto questionId = data.getParam("Q").toInt();
        const auto answerId = data.getParam("A").toInt();
        const auto& uuid = user.currentQUuid();
        if (uuid.isNull())
        {
            sendMessage({.chat_id = chatId,
                         .text = tr("Questionnaire is not started. Retry")});

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
            sendMessage({.chat_id = chatId,
                         .text = tr("Questionnaire for this button already finished")});

            _loger->sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("User press button for already finished questionnaire. User ID: %1. Questionnaire: %2. Question ID: %3. Answer ID: %4")
                                                                         .arg(userId)
                                                                         .arg(uuid.toString())
                                                                         .arg(questionId)
                                                                         .arg(answerId));

            return;
        }
\
        saveAnswer(chatId, userId, questionId, QVariant(answerId));
    }
    else if (buttonName == "USEREDIT")
    {
        const auto userId = data.getParam("U").toLongLong();
        const auto chatId = data.getParam("C").toLongLong();
        auto& user = _users->user(userId);
        if (user.state() != ::User::EUserState::USER_EDIT)
        {
            sendMessage({.chat_id = chatId,
                         .text = tr("This button is not active. Please click 'Users' on start menu for edit users")});

            return;
        }

        const auto action_int = data.getParam("A").toUInt();
        UserEditAction action = UserEditAction::BLOCK;;
        switch (action_int)
        {
        case static_cast<quint8>(UserEditAction::UNBLOCK):
            action = UserEditAction::UNBLOCK;
            break;
        case static_cast<quint8>(UserEditAction::BLOCK):
            action = UserEditAction::BLOCK;
            break;
        default:
            Q_ASSERT(false);
        }

        const auto userWorkId = data.getParam("E");
        if (userWorkId.isEmpty())
        {
            usersSelectList(chatId, userId, action);
        }
        else
        {
            switch (action)
            {
            case UserEditAction::UNBLOCK:
                userConfirm(chatId, userId, userWorkId.toLongLong());
                break;
            case UserEditAction::BLOCK:
                userBlock(chatId, userId, userWorkId.toLongLong());
                break;
            default:
                Q_ASSERT(false);
            }
        }
    }
    else if (buttonName == "RESULT")
    {
        const auto userId = data.getParam("U").toLongLong();
        const auto chatId = data.getParam("C").toLongLong();

        const auto& user = _users->user_c(userId);
        if (user.state() != ::User::EUserState::SAVE_RESULTS)
        {
            sendMessage({.chat_id = chatId,
                         .text = tr("This button is not active. Please click 'Results' on start menu for save results")});

            return;
        }
        const auto start = data.getParam("F").toInt();

        resultQuestionnaire(chatId, userId, QDateTime::currentDateTime().addSecs(-start), QDateTime::currentDateTime());
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

void Core::downloadCompliteDownloader(qint64 chatId, qint64 userId, const QByteArray &data)
{
    auto& user = _users->user(userId);
    if (user.state() == ::User::EUserState::LOAD_QUESTIONNAIRE)
    {
        const auto fileNameBackup = QString("%1/Questionnaire/Backup_%2.json")
                                  .arg(QCoreApplication::applicationDirPath())
                                  .arg(QDateTime::currentDateTime().toString("yyyyMMddhhmmss"));

        _loger->sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("Create backup questionnaire before update. File name: ").arg(fileNameBackup));

        if (!makeFilePath(fileNameBackup))
        {
            _loger->sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("Cannot make dir for file: %1").arg(fileNameBackup));
            sendMessage({.chat_id = chatId,
                         .text = tr("Cannot load new questionnaire from server. Try again")});

            setUserState(chatId, userId, ::User::EUserState::READY);

            return;
        }

        QFile fileBackup(fileNameBackup);
        if (!fileBackup.open(QIODeviceBase::WriteOnly))
        {
            _loger->sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("Cannot save file: %1. Error: %2").arg(fileBackup.fileName()).arg(fileBackup.errorString()));
            sendMessage({.chat_id = chatId,
                         .text = tr("Cannot load new questionnaire from server. Try again")});

            setUserState(chatId, userId, ::User::EUserState::READY);

            return;
        }

        fileBackup.write(_questionnaire->toString().toUtf8());
        fileBackup.close();

        const auto fileNameNew = QString("%1/Questionnaire/New_%2_%3.json")
                                        .arg(QCoreApplication::applicationDirPath())
                                        .arg(userId)
                                        .arg(QDateTime::currentDateTime().toString("yyyyMMddhhmmss"));

        _loger->sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("Save new questionnaire into file: %1").arg(fileNameNew));

        if (!makeFilePath(fileNameNew))
        {
            _loger->sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("Cannot make dir for file: %1").arg(fileNameNew));
            sendMessage({.chat_id = chatId,
                         .text = tr("Cannot load new questionnaire from server. Try again")});

            setUserState(chatId, userId, ::User::EUserState::READY);

            return;
        }

        QFile fileNew(fileNameNew);
        if (!fileNew.open(QIODeviceBase::WriteOnly))
        {
            _loger->sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("Cannot save file: %1. Error: %2").arg(fileNew.fileName()).arg(fileNew.errorString()));
            sendMessage({.chat_id = chatId,
                         .text = tr("Cannot load new questionnaire from server. Try again")});

            setUserState(chatId, userId, ::User::EUserState::READY);

            return;
        }

        fileNew.write(data);
        fileNew.close();

        const auto resultStr = _questionnaire->loadFromString(data);

        if (!resultStr.isEmpty())
        {
            _loger->sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("Cannot load new questionnaire: %1").arg(resultStr));

            sendMessage({.chat_id = chatId,
                         .text = tr("Cannot load new questionnaire: %1").arg(resultStr)});

            setUserState(chatId, userId, ::User::EUserState::READY);
        }
        else
        {
            _loger->sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("New questionnaire load successfully"));
            sendMessage({.chat_id = chatId,
                               .text = tr("New questionnaire load successfully")});

            rebootUsers(tr("The questionnaire has been reloaded by the administrator. Please start over"));

            _questionnaire->loadFromDB();
        }  
    }
}

void Core::downloadErrorDownloader(qint64 chatId, qint64 userId)
{
    sendMessage({.chat_id = chatId,
                 .text = tr("Cannot load new questionnaire from server. Try again")});

    setUserState(chatId, userId, ::User::EUserState::READY);
}

void Core::sendLogMsgDownloader(Common::TDBLoger::MSG_CODE category, const QString &msg)
{
    _loger->sendLogMsg(category, QString("Downloader: %1").arg(msg));
}

void Core::initUser(qint64 chatId, const Telegram::Message& message)
{
    const auto userId = message.from->id;
    if (_users->userExist(userId))
    {
        auto& user = _users->user(userId);
        const auto role = user.role();
        switch (role)
        {
        case ::User::EUserRole::USER:
        case ::User::EUserRole::ADMIN:
            startButton(chatId, role);
            break;
        case ::User::EUserRole::NO_CONFIRMED:
            sendMessage({.chat_id = chatId,
                         .text = tr("Please wait for account confirmation from the administrator")});
            break;
        case ::User::EUserRole::DELETED:
            sendMessage({.chat_id = chatId,
                         .text = tr("Please wait for account unblocked from the administrator")});
            user.setRole(::User::EUserRole::NO_CONFIRMED);
            break;
        case ::User::EUserRole::UNDEFINED:
        default:
            Q_ASSERT(false);
        }

        if (!user.chatExist(chatId))
        {
            auto chat_p = std::make_unique<::Chat>(chatId, ::Chat::EChatState::USE);
            user.addNewChat(std::move(chat_p));
        }
        else
        {
            user.setChatState(chatId, ::Chat::EChatState::USE);
        }

        return;
    }

    auto user_p = std::make_unique<::User>(userId,
         message.from->first_name,
         message.from->last_name.has_value() ? message.from->last_name.value() : "",
         message.from->username.has_value() ? message.from->username.value() : "",
         ::User::EUserRole::NO_CONFIRMED,
         ::User::EUserState::BLOCKED);

    auto chat_p = std::make_unique<::Chat>(chatId, ::Chat::EChatState::USE);
    user_p->addNewChat(std::move(chat_p));

    _users->addUser(std::move(user_p));

    sendMessage({.chat_id = chatId,
                 .text = tr("Welcome to TS AZS Check Bot. Please wait for account confirmation from the administrator")});
}

void Core::removeUser(qint64 chatId, qint64 userId)
{
    _users->removeUser(userId);

    // Sending reply keyboard
    clearButton(chatId);
}

void Core::rebootUsers(const QString& userMessage)
{
    for (const auto& userId: _users->userIdList())
    {
        const auto& user = _users->user(userId);
        const auto role = user.role();
        if (role == ::User::EUserRole::USER || role == ::User::EUserRole::ADMIN)
        {
            if (user.state() != ::User::EUserState::READY)
            {
                for (const auto& chatId: user.useChatIdList())
                {
                    sendMessage({.chat_id = chatId,
                                 .text = userMessage});
                    setUserState(chatId, userId, ::User::EUserState::READY);
                }
            }
        }
    }
}

void Core::setUserState(qint64 chatId, qint64 userId, ::User::EUserState state)
{
    Q_ASSERT(state != ::User::EUserState::UNDEFINED);
    Q_ASSERT(_users->userExist(userId));

    auto& user =_users->user(userId);
    const auto role = user.role();

    if (role != ::User::EUserRole::USER && role != ::User::EUserRole::ADMIN && role != ::User::EUserRole::NO_CONFIRMED)
    {
        sendMessage({.chat_id = chatId,
                     .text = tr("Authorization has not been completed. Please enter the /start command to start and send message to Administrator")});

        return;
    }

    if (role == ::User::EUserRole::NO_CONFIRMED)
    {
        clearButton(chatId);

        return;
    }

    switch (state) {
    case ::User::EUserState::READY:
        startButton(chatId, user.role());
        break;
    case ::User::EUserState::QUESTIONNAIRE:
        startQuestionnaireButton(chatId);
        break;
    case ::User::EUserState::BLOCKED:
        clearButton(chatId);
        break;

    case ::User::EUserState::LOAD_QUESTIONNAIRE:
        cancelButton(chatId);
        break;

    case ::User::EUserState::USER_EDIT:
        cancelButton(chatId);
        break;

    case ::User::EUserState::SAVE_RESULTS:
        cancelButton(chatId);
        break;

    case ::User::EUserState::UNDEFINED:
    default:
        Q_ASSERT(false);
        break;
    }

    user.setState(state);
}

void Core::startQuestionnaire(qint64 chatId, qint64 userId)
{
    setUserState(chatId, userId, ::User::EUserState::QUESTIONNAIRE);

    auto& user = _users->user(userId);
    const auto questionID = user.startQuestionnaire(_questionnaire);

    nextQuestions(chatId, userId, questionID);

    _loger->sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("Start questionnaire. User ID: %1. Questionnaire: %2").arg(userId).arg(user.currentQUuid().toString()));
}

void Core::finishQuestionnaire(qint64 chatId, qint64 userId)
{
    auto& user = _users->user(userId);

    if (user.state() != ::User::EUserState::QUESTIONNAIRE)
    {
        sendMessage({.chat_id = chatId,
                     .text = tr("Questionnaire is not started. Retry")});

        setUserState(chatId, userId, ::User::EUserState::READY);

        return;
    }

    const auto uuid = user.currentQUuid(); // there not reference

    user.finishQuestionnaire();

    sendMessage({.chat_id = chatId,
                 .text = tr("Questionnaire finished. Results saved as: %1").arg(uuid.toString())});

    setUserState(chatId, userId, ::User::EUserState::READY);

    _loger->sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("Finish questionnaire. User ID: %1. Questionnaire: %2").arg(userId).arg(uuid.toString()));
}

void Core::cancel(qint64 chatId, qint64 userId)
{
    auto& user = _users->user(userId);

    switch (user.state())
    {
    case ::User::EUserState::QUESTIONNAIRE:
        user.cancelQuestionnaire();
        sendMessage({.chat_id = chatId,
                     .text = tr("Questionnaire has been canceled")});
        _loger->sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("Cancel questionnaire. User ID: %1. Questionnaire: %2").arg(userId).arg(user.currentQUuid().toString()));
        break;
    case ::User::EUserState::LOAD_QUESTIONNAIRE:
    case ::User::EUserState::USER_EDIT:
        break;
    case ::User::EUserState::READY:
    case ::User::EUserState::BLOCKED:
        break;
    case ::User::EUserState::UNDEFINED:
    default:
        break;
    }

    setUserState(chatId, userId, ::User::EUserState::READY);
}

void Core::resultQuestionnaire(qint64 chatId, qint64 userId, const QDateTime& start, const QDateTime& end)
{
    Q_ASSERT(chatId != 0);
    Q_ASSERT(userId != 0);
    Q_ASSERT(start < end);

    _bot->sendChatAction(chatId, Telegram::Bot::ChatActionType::UPLOAD_DOCUMENT);

    const auto results = _questionnaire->getAllResults(start, end);

    const auto fileName = QString("%1/Results/Results_%2_%3.csv")
                            .arg(QCoreApplication::applicationDirPath())
                            .arg(start.toString("yyyyMMddhhmmss"))
                            .arg(end.toString("yyyyMMddhhmmss"));

    if (!makeFilePath(fileName))
    {
        _loger->sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("Cannot make dir for file: %1").arg(fileName));

        sendMessage({.chat_id = chatId,
                     .text = tr("Cannot save questionnaires results. Try again")});

        setUserState(chatId, userId, ::User::EUserState::READY);

        return;
    }

    QFile file(fileName);
    if (!file.open(QIODeviceBase::WriteOnly))
    {
        _loger->sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("Cannot save file: %1. Error: %2").arg(file.fileName()).arg(file.errorString()));

        sendMessage({.chat_id = chatId,
                     .text = tr("Cannot save questionnaires results. Try again")});

        setUserState(chatId, userId, ::User::EUserState::READY);

        return;
    }

    file.write(results.toUtf8());
    file.close();

    file.open(QIODeviceBase::ReadOnly);

    _bot->sendDocument(chatId, &file);

    file.close();

    _loger->sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("File with questionnaires results sended. File: %1. User ID: %2").arg(file.fileName()).arg(userId));

    setUserState(chatId, userId, ::User::EUserState::READY);
}

void Core::loadQuestionnaire(qint64 chatId, qint64 userId)
{
    setUserState(chatId, userId, ::User::EUserState::LOAD_QUESTIONNAIRE);

    sendMessage({.chat_id = chatId,
                 .text =  tr("Please send file contain new questionnire")});
}

void Core::saveQuestionnaire(qint64 chatId, qint64 userId)
{
    _bot->sendChatAction(chatId, Telegram::Bot::ChatActionType::UPLOAD_DOCUMENT);

    const auto results = _questionnaire->toString();

    const auto fileName = QString("%1/QuestionnaireSave/Questionnaire_%2.json").arg(QCoreApplication::applicationDirPath()).arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));

    if (!makeFilePath(fileName))
    {
        _loger->sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("Cannot make dir for file: %1").arg(fileName));

        sendMessage({.chat_id = chatId,
                     .text = tr("Cannot send questionnaires file. Try again")});

        setUserState(chatId, userId, ::User::EUserState::READY);

        return;
    }

    QFile file(fileName);
    if (!file.open(QIODeviceBase::WriteOnly))
    {
        _loger->sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("Cannot save file: %1. Error: %2").arg(file.fileName()).arg(file.errorString()));

        sendMessage({.chat_id = chatId,
                     .text = tr("Cannot send questionnaires file. Try again")});

        setUserState(chatId, userId, ::User::EUserState::READY);

        return;
    }

    file.write(results.toUtf8());

    file.close();

    file.open(QIODeviceBase::ReadOnly);

    _bot->sendDocument(chatId, &file);

    file.close();

    _loger->sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("File with questionnaire sended. File: %1. User ID: %2").arg(file.fileName()).arg(userId));
}

void Core::startUsersEdit(qint64 chatId, qint64 userId)
{
    auto& user = _users->user(userId);
    if (user.role() != ::User::EUserRole::ADMIN)
    {
        sendMessage({.chat_id = chatId,
                     .text = tr("Insufficient rights to perform the action")});
        return;
    }

    setUserState(chatId, userId, ::User::EUserState::USER_EDIT);

    QVector<InlineKeyboardButton> questionButtons;
    {
        ButtonData unblockButtonData;
        unblockButtonData.setParam("N", "USEREDIT");
        unblockButtonData.setParam("A", QString::number(static_cast<quint8>(UserEditAction::UNBLOCK)));
        unblockButtonData.setParam("U", QString::number(userId));
        unblockButtonData.setParam("C", QString::number(chatId));

        InlineKeyboardButton unblockButton(tr("Unblock"), std::nullopt, std::nullopt, unblockButtonData.toString());
        questionButtons.push_back(unblockButton);
    }
    {
        ButtonData blockButtonData;
        blockButtonData.setParam("N", "USEREDIT");
        blockButtonData.setParam("A", QString::number(static_cast<quint8>(UserEditAction::BLOCK)));
        blockButtonData.setParam("U", QString::number(userId));
        blockButtonData.setParam("C", QString::number(chatId));

        InlineKeyboardButton blockButton(tr("Block"), std::nullopt, std::nullopt, blockButtonData.toString());

        questionButtons.push_back(blockButton);
    }

    QVector<QVector<InlineKeyboardButton>> buttons;
    buttons.push_back(questionButtons);

    // Sending all inline buttons
    sendMessage({.chat_id = chatId,
                 .text = tr("Please click to 'Unblock' for confirmed user registration or 'Block' for user blocked and delete"),
                 .reply_markup = buttons});
}

void Core::usersSelectList(qint64 chatId, qint64 userId, UserEditAction action)
{
    Users::UsersIDList userList;

    switch (action)
    {
    case UserEditAction::UNBLOCK:
        userList = _users->noConfirmUserIdList();
        break;
    case UserEditAction::BLOCK:
        userList = _users->confirmUserIdList();
        break;
    default:
        Q_ASSERT(false);
    }

    QVector<QVector<InlineKeyboardButton>> buttons;
    QVector<InlineKeyboardButton> questionButtons;

    for (const auto userWorkId: userList)
    {
        if (userWorkId == userId)
        {
            continue;
        }

        const auto& userWork = _users->user_c(userWorkId);

        ButtonData data;
        data.setParam("N", "USEREDIT");
        data.setParam("U", QString::number(userId));
        data.setParam("C", QString::number(chatId));
        data.setParam("A", QString::number(static_cast<quint8>(action)));
        data.setParam("E", QString::number(userWork.telegramID()));

        InlineKeyboardButton questionButton(userWork.getViewUserName(), std::nullopt, std::nullopt, data.toString());

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

    if (inlineButtons.isEmpty())
    {
        sendMessage({.chat_id = chatId,
                     .text = tr("No user for edit")});

        setUserState(chatId, userId, ::User::EUserState::READY);

        return;
    }

    // Sending all inline buttons
    sendMessage({.chat_id = chatId,
                 .text = tr("Please select user"),
                 .reply_markup = inlineButtons});
}

void Core::userConfirm(qint64 chatId, qint64 userId, qint64 userWorkId)
{
    Q_ASSERT(chatId != 0);
    Q_ASSERT(userId != 0);
    Q_ASSERT(userWorkId != 0);

    auto& userWork = _users->user(userWorkId);
    userWork.setRole(::User::EUserRole::USER);

    for (const auto& chatWorkID: userWork.useChatIdList())
    {
        sendMessage({.chat_id = chatWorkID,
                     .text = tr("Congratulations! Administrator has added you to the list of confirmed users")});

        setUserState(userWorkId, chatWorkID, ::User::EUserState::READY);
    }

    sendMessage({.chat_id = chatId,
                 .text = tr("User %1 successfully confirmed").arg(userWork.userName())});

    _loger->sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("User %1 successfully confirmed").arg(userWork.userName()));

    setUserState(chatId, userId, ::User::EUserState::READY);
}

void Core::userBlock(qint64 chatId, qint64 userId, qint64 userWorkId)
{
    Q_ASSERT(chatId != 0);
    Q_ASSERT(userId != 0);
    Q_ASSERT(userWorkId != 0);

    auto& userWork = _users->user(userWorkId);

    for (const auto& chatWorkID: userWork.useChatIdList())
    {
       sendMessage({.chat_id = chatWorkID,
                     .text = tr("Administrator has deleted you from the list of confirmed users. Please send the /start command to start and send message to Administrator")});
        setUserState(userWorkId, chatWorkID, ::User::EUserState::BLOCKED);
    }

    userWork.setRole(::User::EUserRole::DELETED);

    sendMessage({.chat_id = chatId,
                 .text = tr("User %1 successfully deleted").arg(userWork.userName())});

   _loger->sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("User %1 successfully delete").arg(userWork.userName()));

    setUserState(chatId, userId, ::User::EUserState::READY);
}

void Core::selectDate(qint64 chatId, qint64 userId)
{
    Q_ASSERT(chatId != 0);
    Q_ASSERT(userId != 0);

    auto& user = _users->user(userId);
    if (user.role() != ::User::EUserRole::ADMIN)
    {
        sendMessage({.chat_id = chatId,
                     .text = tr("Insufficient rights to perform the action")});
        return;
    }

    setUserState(chatId, userId, ::User::EUserState::SAVE_RESULTS);

    QVector<InlineKeyboardButton> questionButtons;
    QVector<QVector<InlineKeyboardButton>> buttons;
    {
        ButtonData oneDayButtonData;
        oneDayButtonData.setParam("N", "RESULT");
        oneDayButtonData.setParam("F", QString::number(QDateTime::currentDateTime().addDays(-1).secsTo(QDateTime::currentDateTime())));
        oneDayButtonData.setParam("U", QString::number(userId));
        oneDayButtonData.setParam("C", QString::number(chatId));

        InlineKeyboardButton oneDayButton(tr("One day"), std::nullopt, std::nullopt, oneDayButtonData.toString());
        questionButtons.push_back(oneDayButton);
    }

    {
        ButtonData oneWeekButtonData;
        oneWeekButtonData.setParam("N", "RESULT");
        oneWeekButtonData.setParam("F", QString::number(QDateTime::currentDateTime().addDays(-7).secsTo(QDateTime::currentDateTime())));
        oneWeekButtonData.setParam("U", QString::number(userId));
        oneWeekButtonData.setParam("C", QString::number(chatId));

        InlineKeyboardButton oneWeekButton(tr("One week"), std::nullopt, std::nullopt, oneWeekButtonData.toString());
        questionButtons.push_back(oneWeekButton);
    }

    buttons.push_back(questionButtons);
    questionButtons.clear();

    {
        ButtonData twoWeeksButtonData;
        twoWeeksButtonData.setParam("N", "RESULT");
        twoWeeksButtonData.setParam("F", QString::number(QDateTime::currentDateTime().addDays(-14).secsTo(QDateTime::currentDateTime())));
        twoWeeksButtonData.setParam("U", QString::number(userId));
        twoWeeksButtonData.setParam("C", QString::number(chatId));

        InlineKeyboardButton twoWeeksButton(tr("Two weeks"), std::nullopt, std::nullopt, twoWeeksButtonData.toString());
        questionButtons.push_back(twoWeeksButton);
    }

    {
        ButtonData threeWeeksButtonData;
        threeWeeksButtonData.setParam("N", "RESULT");
        threeWeeksButtonData.setParam("F", QString::number(QDateTime::currentDateTime().addDays(-7).secsTo(QDateTime::currentDateTime())));
        threeWeeksButtonData.setParam("U", QString::number(userId));
        threeWeeksButtonData.setParam("C", QString::number(chatId));

        InlineKeyboardButton threeWeeksButton(tr("Three weeks"), std::nullopt, std::nullopt, threeWeeksButtonData.toString());
        questionButtons.push_back(threeWeeksButton);
    }

    buttons.push_back(questionButtons);
    questionButtons.clear();

    {
        ButtonData oneMonthButtonData;
        oneMonthButtonData.setParam("N", "RESULT");
        oneMonthButtonData.setParam("F", QString::number(QDateTime::currentDateTime().addMonths(-1).secsTo(QDateTime::currentDateTime())));
        oneMonthButtonData.setParam("U", QString::number(userId));
        oneMonthButtonData.setParam("C", QString::number(chatId));

        InlineKeyboardButton oneMonthButton(tr("One months"), std::nullopt, std::nullopt, oneMonthButtonData.toString());
        questionButtons.push_back(oneMonthButton);
    }

    {
        ButtonData twoMonthsButtonData;
        twoMonthsButtonData.setParam("N", "RESULT");
        twoMonthsButtonData.setParam("F", QString::number(QDateTime::currentDateTime().addMonths(-2).secsTo(QDateTime::currentDateTime())));
        twoMonthsButtonData.setParam("U", QString::number(userId));
        twoMonthsButtonData.setParam("C", QString::number(chatId));

        InlineKeyboardButton twoMonthsButton(tr("Two months"), std::nullopt, std::nullopt, twoMonthsButtonData.toString());
        questionButtons.push_back(twoMonthsButton);
    }

    buttons.push_back(questionButtons);
    questionButtons.clear();

    {
        ButtonData threeMonthsButtonData;
        threeMonthsButtonData.setParam("N", "RESULT");
        threeMonthsButtonData.setParam("F", QString::number(QDateTime::currentDateTime().addMonths(-1).secsTo(QDateTime::currentDateTime())));
        threeMonthsButtonData.setParam("U", QString::number(userId));
        threeMonthsButtonData.setParam("C", QString::number(chatId));

        InlineKeyboardButton threeMonthsButton(tr("Three months"), std::nullopt, std::nullopt, threeMonthsButtonData.toString());
        questionButtons.push_back(threeMonthsButton);
    }

    {
        ButtonData fourMonthsButtonData;
        fourMonthsButtonData.setParam("N", "RESULT");
        fourMonthsButtonData.setParam("F", QString::number(QDateTime::currentDateTime().addMonths(-2).secsTo(QDateTime::currentDateTime())));
        fourMonthsButtonData.setParam("U", QString::number(userId));
        fourMonthsButtonData.setParam("C", QString::number(chatId));

        InlineKeyboardButton fourMonthsButton(tr("Four months"), std::nullopt, std::nullopt, fourMonthsButtonData.toString());
        questionButtons.push_back(fourMonthsButton);
    }

    buttons.push_back(questionButtons);
    questionButtons.clear();

    {
        ButtonData halfYearButtonData;
        halfYearButtonData.setParam("N", "RESULT");
        halfYearButtonData.setParam("F", QString::number(QDateTime::currentDateTime().addMonths(-6).secsTo(QDateTime::currentDateTime())));
        halfYearButtonData.setParam("U", QString::number(userId));
        halfYearButtonData.setParam("C", QString::number(chatId));

        InlineKeyboardButton halfYearButton(tr("Half year"), std::nullopt, std::nullopt, halfYearButtonData.toString());
        questionButtons.push_back(halfYearButton);
    }

    {
        ButtonData oneYearButtonData;
        oneYearButtonData.setParam("N", "RESULT");
        oneYearButtonData.setParam("F", QString::number(QDateTime::currentDateTime().addYears(-1).secsTo(QDateTime::currentDateTime())));
        oneYearButtonData.setParam("U", QString::number(userId));
        oneYearButtonData.setParam("C", QString::number(chatId));

        InlineKeyboardButton oneYearButton(tr("One year"), std::nullopt, std::nullopt, oneYearButtonData.toString());
        questionButtons.push_back(oneYearButton);
    }

    buttons.push_back(questionButtons);
    questionButtons.clear();

    // Sending all inline buttons
    sendMessage({.chat_id = chatId,
                 .text = tr("Please select period for report of results"),
                 .reply_markup = buttons});
}

void Core::startButton(qint64 chatId, ::User::EUserRole role)
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

void Core::startUserButton(qint64 chatId)
{
    KeyboardButton startButton(tr("Start"));

    ReplyKeyboardMarkup keyboard = {{startButton}};

    // Sending reply keyboard
    sendMessage({.chat_id = chatId,
                 .text = tr("Please click 'Start' button for new check AZS"),
                 .reply_markup = keyboard});
}

void Core::startAdminButton(qint64 chatId)
{
    KeyboardButton startButton(tr("Start"));
    KeyboardButton usersButton(tr("Users"));
    KeyboardButton saveButton(tr("Save"));
    KeyboardButton loadButton(tr("Load"));
    KeyboardButton resultsButton(tr("Results"));

    ReplyKeyboardMarkup keyboard = {{startButton, resultsButton}, {loadButton, saveButton}, {usersButton}};

    // Sending reply keyboard
    sendMessage({.chat_id = chatId,
                 .text = tr("Please click:\n'Start' button for new check AZS\n"
                            "'Users' for edit users data\n"
                            "'Save' for save current questionnaire\n"
                            "'Load' for update questionnaire\n"
                            "'Results' for download questionnaire results"),
                 .reply_markup = keyboard});
}

Telegram::ReplyKeyboardMarkup Core::startFinishButton()
{
    KeyboardButton finishButton(tr("Finish"));
    KeyboardButton cancelButton(tr("Cancel"));

    return {{finishButton}, {cancelButton}};
}

void Core::startQuestionnaireButton(qint64 chatId)
{
    // Sending reply keyboard
    sendMessage({.chat_id = chatId,
                 .text = tr("Please click 'Finish' button finished and save result or 'Cancel' to cancel"),
                 .reply_markup = startFinishButton() });
}

void Core::clearButton(qint64 chatId)
{
    sendMessage({.chat_id = chatId,
                 .text = tr("Goodbye"),
                 .reply_markup = ReplyKeyboardRemove()});
}

void Core::cancelButton(qint64 chatId)
{
    KeyboardButton cancelButton(tr("Cancel"));

    ReplyKeyboardMarkup keyboard = {{cancelButton}};
    //  keyboard .resize_keyboard = true;

    sendMessage({.chat_id = chatId,
                 .text = tr("Please click 'Cancel' for cancel"),
                 .reply_markup = keyboard });
}

void Core::nextQuestions(qint64 chatId, qint64 userId, qint32 questionId)
{
    const auto& user = _users->user(userId);
    const auto& uuid = user.currentQUuid();
    const auto& question = _questionnaire->question(questionId);

    switch (question.type())
    {
    case Question::EQuestionType::CHECHED:
    {
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
            InlineKeyboardButton questionButton(answer.second.toString(), std::nullopt, std::nullopt, data.toString());

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
        sendMessage({.chat_id = chatId,
                     .text = tr("(%1/%2) %3").arg(_questionnaire->questionPosition(questionId)).arg(_questionnaire->questionCount()).arg(question.text()),
                     .reply_markup = inlineButtons});

        break;
    }
    case Question::EQuestionType::TEXT:
    {
        sendMessage({.chat_id = chatId,
                     .text = tr("(%1/%2) %3").arg(_questionnaire->questionPosition(questionId)).arg(_questionnaire->questionCount()).arg(question.text()),
                     .reply_markup = startFinishButton()});

        break;
    }
    case Question::EQuestionType::DELETED:
    case Question::EQuestionType::UNDEFINED:
    default:
        Q_ASSERT(false);
        break;
    }
}

void Core::saveAnswer(qint64 chatId, qint64 userId, qint32 questionId, const QVariant &answer)
{
    auto& user = _users->user(userId);
    const auto maxQuestionId = user.maxQuestionId();
    const auto nextQuestionID = user.setAnswer(questionId , answer);

    _loger->sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("Get answer from user. User ID: %1. Questionnaire: %2. Question ID: %3. Answer ID: %4")
                                                                 .arg(userId)
                                                                 .arg(user.currentQUuid().toString())
                                                                 .arg(questionId)
                                                                 .arg(answer.toString()));

    if (nextQuestionID < 0)
    {
        sendMessage({.chat_id = chatId,
                     .text = tr("Congratulations! You have answered all the questions")});

        startQuestionnaireButton(chatId);

        return;
    }

    if (nextQuestionID > maxQuestionId)
    {
        nextQuestions(chatId, userId, nextQuestionID);
    }
}

//STL
#include <stdexcept>

//Qt
#include <QSqlQuery>

#include "users.h"

static const QString DB_CONNECTION_NAME = "UsersDB";

using namespace Common;

///////////////////////////////////////////////////////////////////////////////
/// class Users
Users::Users(const Common::DBConnectionInfo &dbConnectionInfo, QObject* parent /* = nullptr */)
    : QObject{parent}
    , _dbConnectionInfo(dbConnectionInfo)
{
}

Users::~Users()
{
    if (_db.isOpen())
    {
        closeDB(_db);
    }
}

void Users::loadFromDB()
{
    try
    {
        connectToDB(_db, _dbConnectionInfo, DB_CONNECTION_NAME);
    }
    catch (const SQLException& err)
    {
        emit errorOccured(EXIT_CODE::SQL_NOT_CONNECT, err.what());

        return;
    }

    class LoadException
        : public std::runtime_error
    {
    public:
        LoadException(const QString& err)
            : std::runtime_error(err.toStdString())
        {}

    private:
        LoadException() = delete;

    };

    QStringList userIdList;
    try
    {
        transactionDB(_db);

        QSqlQuery query(_db);
        query.setForwardOnly(true);

        QString queryText =
            QString("SELECT `ID`, `TelegramID`, `FirstName`, `LastName`, `UserName`, `Role`, `State`"
                    "FROM `Users` "
                    "ORDER BY `ID` ");

        DBQueryExecute(_db, query, queryText);

        while (query.next())
        {
            const auto recordId = query.value("ID").toLongLong();
            const qint64 userId = query.value("TelegramID").toLongLong();
            if (userId <= 0)
            {
                throw LoadException(QString("User ID cannot be null or less in [Users]/TelegramID. Record: %1").arg(recordId));
            }

            const auto role = ::User::intToEUserRole(query.value("Role").toUInt());
            if (role == ::User::EUserRole::UNDEFINED)
            {
                throw LoadException(QString("User role cannot be UNDEFINE in [Users]/Role. Record: %1").arg(recordId));
            }

            const auto state = ::User::intToEUserState(query.value("State").toUInt());
            if (role == ::User::EUserRole::UNDEFINED)
            {
                throw LoadException(QString("User state cannot be UNDEFINE in [Users]/State. Record: %1").arg(recordId));
            }

            auto user_p=
                std::make_unique<User>(userId,
                                       query.value("FirstName").toString(),
                                       query.value("LastName").toString(),
                                       query.value("UserName").toString(),
                                       role,
                                       state);

            userIdList.push_back(QString::number(userId));

            QObject::connect(user_p.get(), SIGNAL(roleChenged(qint64, User::EUserRole)), SLOT(roleChenged(qint64, User::EUserRole)));
            QObject::connect(user_p.get(), SIGNAL(stateChenged(qint64, User::EUserState)), SLOT(stateChenged(qint64, User::EUserState)));
            QObject::connect(user_p.get(), SIGNAL(addNewChat(qint64, qint64, Chat::EChatState)), SLOT(addNewChat(qint64, qint64, Chat::EChatState)));
            QObject::connect(user_p.get(), SIGNAL(chatStateChenged(qint64, qint64, Chat::EChatState)), SLOT(chatStateChenged(qint64, qint64, Chat::EChatState)));

            _users.emplace(std::move(userId), std::move(user_p));
        }

        //Пользовтелей совсем нет, больше нечего загружать....
        if (userIdList.isEmpty())
        {
            return;
        }

        query.clear();

        queryText =
            QString("SELECT `ID`, `UserID`, `ChatID`, `State` "
                    "FROM `Chats` "
                    "WHERE `UserID` In (%1)")
                .arg(userIdList.join(u','));

        DBQueryExecute(_db, query, queryText);

        while (query.next())
        {
            const auto recordId = query.value("ID").toLongLong();
            const qint64 userId = query.value("UserID").toLongLong();
            if (userId <= 0)
            {
                throw LoadException(QString("User ID cannot be null or less in [Chats]/UserID. Record: %1").arg(recordId));
            }

            const qint64 chatId = query.value("ChatID").toLongLong();
            if (chatId <= 0)
            {
                throw LoadException(QString("Chat ID cannot be null or less in [Chats]/ChatID. Record: %1").arg(recordId));
            }
            const auto state = Chat::intToEChatState(query.value("State").toUInt());
            if (state == Chat::EChatState::UNDEFINED)
            {
                throw LoadException(QString("User state cannot be UNDEFINE in [Chats]/State. Record: %1").arg(recordId));
            }

            auto chat_p = std::make_unique<Chat>(chatId, state);
            auto& user = _users.find(userId)->second;
            user->addExistChat(std::move(chat_p));
        }

        commitDB(_db);
    }
    catch (const SQLException& err)
    {
        emit errorOccured(EXIT_CODE::SQL_EXECUTE_QUERY_ERR, err.what());

        return;
    }
    catch (const LoadException& err)
    {
        _db.rollback();

        emit errorOccured(EXIT_CODE::LOAD_CONFIG_ERR, err.what());

        return;
    }
}

void Users::addUser(std::unique_ptr<::User> user_p)
{
    Q_CHECK_PTR(user_p);

    Q_ASSERT(user_p->telegramID() != 0);
    Q_ASSERT(user_p->role() != ::User::EUserRole::UNDEFINED);
    Q_ASSERT(user_p->state() != ::User::EUserState::UNDEFINED);
    Q_ASSERT(!_users.contains(user_p->telegramID()));

    auto userId = user_p->telegramID();

    try
    {
        transactionDB(_db);

        QSqlQuery query(_db);

        auto queryText =
        QString("INSERT INTO `Users` (`TelegramID`, `FirstName`, `LastName`, `UserName`, `Role`, `State`) "
                "VALUES (%1, '%2', '%3', '%4', %5, %6)")
                               .arg(userId)
                               .arg(user_p->firstName())
                               .arg(user_p->lastName())
                               .arg(user_p->userName())
                               .arg(static_cast<quint8>(user_p->role()))
                               .arg(static_cast<quint8>(user_p->state()));


        DBQueryExecute(_db, query, queryText);

        for (const auto chatId: user_p->chatIdList())
        {
            const auto& chat = user_p->chat(chatId);

            queryText =
                QString("INSERT INTO `Chats` (`UserID`, `ChatId`, `State`) "
                        "VALUES (%1, %2, %3)")
                    .arg(userId)
                    .arg(chat.chatId())
                    .arg(static_cast<quint8>(chat.state()));

            DBQueryExecute(_db, query, queryText);
        }

        commitDB(_db);
    }
    catch (const SQLException& err)
    {
        emit errorOccured(EXIT_CODE::SQL_EXECUTE_QUERY_ERR, err.what());

        return;
    }

    QObject::connect(user_p.get(), SIGNAL(roleChenged(qint64, User::EUserRole)), SLOT(roleChenged(qint64, User::EUserRole)));
    QObject::connect(user_p.get(), SIGNAL(stateChenged(qint64, User::EUserState)), SLOT(stateChenged(qint64, User::EUserState)));
    QObject::connect(user_p.get(), SIGNAL(addNewChat(qint64, qint64, Chat::EChatState)), SLOT(addNewChat(qint64, qint64, Chat::EChatState)));
    QObject::connect(user_p.get(), SIGNAL(chatStateChenged(qint64, qint64, Chat::EChatState)), SLOT(chatStateChenged(qint64, qint64, Chat::EChatState)));

    _users.emplace(std::move(userId), std::move(user_p));
}

void Users::removeUser(qint64 userId)
{
    Q_ASSERT(userId != 0);
    Q_ASSERT(_users.contains(userId));

    const auto& user = _users.find(userId)->second;

    user->setRole(::User::EUserRole::DELETED);
}

Users::UsersIDList Users::userIdList() const
{
    UsersIDList results;

    for (const auto& user: _users)
    {
        results.insert(user.first);
    }

    return results;
}

Users::UsersIDList Users::noConfirmUserIdList() const
{
    UsersIDList results;

    for (const auto& user: _users)
    {
        if (user.second->role() == User::EUserRole::NO_CONFIRMED)
        {
            results.insert(user.first);
        }
    }

    return results;
}

Users::UsersIDList Users::confirmUserIdList() const
{
    UsersIDList results;

    for (const auto& user: _users)
    {
        const auto role = user.second->role();
        if (role == User::EUserRole::USER || role == User::EUserRole::ADMIN)
        {
            results.insert(user.first);
        }
    }

    return results;
}

User &Users::user(qint64 userId) const
{
    Q_ASSERT(userId != 0);
    Q_ASSERT(_users.contains(userId));

    return *_users.at(userId).get();
}

const User& Users::user_c(qint64 userId) const
{
    return user(userId);
}

bool Users::userExist(qint64 userId) const
{
    Q_ASSERT(userId != 0);

    return _users.contains(userId);
}

quint64 Users::usersCount() const
{
    return _users.size();
}

void Users::roleChenged(qint64 userId, User::EUserRole role)
{
    Q_ASSERT(userId != 0);
    Q_ASSERT(role != ::User::EUserRole::UNDEFINED);
    Q_ASSERT(_users.contains(userId));

    const auto queryText = QString("UPDATE `Users` "
                                   "SET `Role` = %1 "
                                   "WHERE `TelegramID` = %2")
                              .arg(static_cast<quint8>(role))
                              .arg(userId);

    try
    {
        DBQueryExecute(_db, queryText);
    }
    catch (const SQLException& err)
    {
        emit errorOccured(EXIT_CODE::SQL_EXECUTE_QUERY_ERR, err.what());

        return;
    }
}

void Users::stateChenged(qint64 userId, User::EUserState state)
{
    Q_ASSERT(userId != 0);
    Q_ASSERT(state != ::User::EUserState::UNDEFINED);
    Q_ASSERT(_users.contains(userId));

    const auto queryText = QString("UPDATE `Users` "
                                   "SET `State` = %1 "
                                   "WHERE `TelegramID` = %2")
                               .arg(static_cast<quint8>(state))
                               .arg(userId);

    try
    {
        DBQueryExecute(_db, queryText);
    }
    catch (const SQLException& err)
    {
        emit errorOccured(EXIT_CODE::SQL_EXECUTE_QUERY_ERR, err.what());

        return;
    }
}

void Users::addNewChat(qint64 userId, qint64 chatId, Chat::EChatState state)
{
    Q_ASSERT(userId != 0);
    Q_ASSERT(chatId != 0);
    Q_ASSERT(_users.contains(userId));
    Q_ASSERT(_users.at(userId).get()->chatExist(chatId));

    const auto queryText =
        QString("INSERT INTO `Chats` (`UserID`, `ChatId`, `State`) "
                "VALUES (%1, %2, %3)")
            .arg(userId)
            .arg(chatId)
            .arg(static_cast<quint8>(state));

    try
    {
        DBQueryExecute(_db, queryText);
    }
    catch (const SQLException& err)
    {
        emit errorOccured(EXIT_CODE::SQL_EXECUTE_QUERY_ERR, err.what());

        return;
    }
}

void Users::chatStateChenged(qint64 userId, qint64 chatId, Chat::EChatState state)
{
    Q_ASSERT(userId != 0);
    Q_ASSERT(chatId != 0);
    Q_ASSERT(state != Chat::EChatState::UNDEFINED);
    Q_ASSERT(_users.contains(userId));
    Q_ASSERT(_users.at(userId)->chatExist(chatId));

    const auto queryText = QString("UPDATE `Chats` "
                                   "SET `State` = %1 "
                                   "WHERE `UserID` = %2 AND `ChatID` = %3")
                               .arg(static_cast<quint8>(state))
                               .arg(userId)
                               .arg(chatId);

    try
    {
        DBQueryExecute(_db, queryText);
    }
    catch (const SQLException& err)
    {
        emit errorOccured(EXIT_CODE::SQL_EXECUTE_QUERY_ERR, err.what());

        return;
    }
}




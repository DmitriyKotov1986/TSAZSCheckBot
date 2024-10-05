//STL
#include <exception>

//Qt
#include <QSqlQuery>

#include "users.h"

static const QString DB_CONNECTION_NAME = "UsersDB";

using namespace Common;

///////////////////////////////////////////////////////////////////////////////
/// class User
User::EUserRole User::intToEUserRole(quint8 role)
{
    switch (role)
    {
    case static_cast<quint8>(EUserRole::ADMIN): return EUserRole::ADMIN;
    case static_cast<quint8>(EUserRole::USER): return EUserRole::USER;
    case static_cast<quint8>(EUserRole::DELETED): return EUserRole::DELETED;
    case static_cast<quint8>(EUserRole::NO_CONFIRMED): return EUserRole::NO_CONFIRMED;
    case static_cast<quint8>(EUserRole::UNDEFINED):
    default:
        break;
    }

    return EUserRole::UNDEFINED;
}

User::User(qint64 telegramID, const QString &firstName, const QString lastName, const QString userName, const QDateTime &addDateTime /* = QDateTime::currentDateTime() */)
    : _telegramID(telegramID)
    , _firstName(firstName)
    , _lastName(lastName)
    , _userName(userName)
    , _addDateTime(addDateTime)
{
}

User::~User()
{
}

User::EUserRole User::role() const
{
    return _role;
}

void User::setRole(EUserRole role)
{
    _role = role;
}

qint32 User::startQuestionnaire(Questionnaire *questionnaire)
{
    Q_CHECK_PTR(questionnaire);

    _questionnaire = questionnaire;
    _questions = _questionnaire->makeQuesions();
    _currentQuestions = _questions.begin();
    _startDateTime = QDateTime::currentDateTime();
    _currentUUID = QUuid::createUuid();
    _maxQuestionId = _currentQuestions != _questions.end() ? _currentQuestions->first : -1;

    return _currentQuestions != _questions.end() ? _currentQuestions->first : -1;
}

qint32 User::setAnswer(qint32 questionID, qint32 answerID)
{
    auto questions_it = _questions.find(questionID);
    if (questions_it == _questions.end())
    {
        Q_ASSERT(false);
    }

    questions_it->second = answerID;

    _currentQuestions = questions_it;

    return nextQuestionId();
}

qint32 User::nextQuestionId()
{
    if (_currentQuestions != _questions.end())
    {
        _currentQuestions = std::next(_currentQuestions);
        _maxQuestionId = std::max(_maxQuestionId, _currentQuestions->first);
    }
    return _currentQuestions != _questions.end() ? _currentQuestions->first : -1;
}

qint32 User::prevQuestionId()
{
    if (_currentQuestions != _questions.end())
    {
        _currentQuestions = std::prev(_currentQuestions);
    }
    return _currentQuestions != _questions.end() ? _currentQuestions->first : -1;
}

void User::finishQuestionnaire()
{
    Q_CHECK_PTR(_questionnaire);

    _questionnaire->saveResults(_telegramID, _currentUUID, _startDateTime, _questions);

    clear();
}

void User::cancelQuestionnaire()
{
    Q_CHECK_PTR(_questionnaire);

    clear();
}

void User::clear()
{
    _questions.clear();
    _currentQuestions = _questions.end();
    _questionnaire = nullptr;
    _currentUUID = QUuid();
    _maxQuestionId = -1;
}

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

    const auto queryText =
        QString("SELECT `ID`, `TelegramID`, `FirstName`, `LastName`, `UserName`, `AddDateTime`, `Role` "
                "FROM `Users` "
                "ORDER BY `ID` ");

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

    try
    {
        transactionDB(_db);

        QSqlQuery query(_db);
        query.setForwardOnly(true);

        DBQueryExecute(_db, query, queryText);

        while (query.next())
        {
            const auto recordId = query.value("ID").toLongLong();
            const qint32 userId = query.value("TelegramID").toInt();
            if (userId <= 0)
            {
                throw LoadException(QString("User ID cannot be null or less in [Users]/TelegramID. Record: %1").arg(recordId));
            }

            User tmp(userId,
            query.value("FirstName").toString(),
            query.value("LastName").toString(),
            query.value("UserName").toString(),
            query.value("AddDateTime").toDateTime());

            const auto role = ::User::intToEUserRole(query.value("Role").toUInt());
            if (role == ::User::EUserRole::UNDEFINED)
            {
                throw LoadException(QString("User role cannot be UNDEFINE in [Users]/TelegramID. Record: %1").arg(recordId));
            }
            tmp.setRole(role);

            _users.emplace(std::move(userId), std::move(tmp));
        }

        commitDB(_db);
    }
    catch (const SQLException& err)
    {
        emit errorOccured(EXIT_CODE::SQL_NOT_CONNECT, err.what());

        return;
    }
    catch (const LoadException& err)
    {
        _db.rollback();

        emit errorOccured(EXIT_CODE::LOAD_CONFIG_ERR, err.what());

        return;
    }
}

void Users::addUser(const ::User &user)
{
    Q_ASSERT(user.role() != ::User::EUserRole::UNDEFINED);

    const auto users_it = _users.find(user.telegramID());
    if  (users_it != _users.end())
    {
        return;
        //setUserRole(users_it->first, User::EUserRole::NO_CONFIRMED);
    }

    const auto queryText =
        QString("INSERT INTO `Users` (`TelegramID`, `FirstName`, `LastName`, `UserName`, `AddDateTime`, `Role`) "
                "VALUES (%1, '%2', '%3', '%4', CAST('%5' AS DATETIME)), %6 ")
                               .arg(user.telegramID())
                               .arg(user.firstName())
                               .arg(user.lastName())
                               .arg(user.userName())
                               .arg(user.addDateTime().toString(DATETIME_FORMAT))
                               .arg(static_cast<quint8>(user.role()));

    try
    {
        DBQueryExecute(_db, queryText);
    }
    catch (const SQLException& err)
    {
        emit errorOccured(EXIT_CODE::SQL_NOT_CONNECT, err.what());

        return;
    }

    _users.insert({user.telegramID(), user});
}

bool Users::removeUser(qint32 id)
{
    Q_ASSERT(_users.contains(id));

    setUserRole(id, ::User::EUserRole::DELETED);

    return true;
}

void Users::setUserRole(qint32 id, User::EUserRole role)
{
    Q_ASSERT(role != ::User::EUserRole::UNDEFINED);

    const auto users_it = _users.find(id);
    if  (users_it == _users.end())
    {
        Q_ASSERT(false);
    }

    const auto queryText =
        QString("UPDATE `Users` "
                "SET `Role` = %1 ")
            .arg(static_cast<quint8>(role));

    try
    {
        DBQueryExecute(_db, queryText);
    }
    catch (const SQLException& err)
    {
        emit errorOccured(EXIT_CODE::SQL_NOT_CONNECT, err.what());

        return;
    }

    users_it->second.setRole(role);
}

User::EUserRole Users::userRole(qint32 id) const
{
    Q_ASSERT(_users.contains(id));

    return _users.at(id).role();
}

User &Users::getUser(qint32 id)
{
    Q_ASSERT(_users.contains(id));

    return _users.at(id);
}

bool Users::userExist(qint32 id) const
{
    return _users.contains(id);
}

quint64 Users::usersCount() const
{
    return _users.size();
}




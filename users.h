#pragma once

//STL
#include <unordered_map>

//Qt
#include <QObject>
#include <QSqlDatabase>
#include <QDateTime>
#include <QUuid>

//My
#include <Common/common.h>

#include "questionnaire.h"

class Users;

class User final
{
    friend Users;

public:
    /*!
        Роли пользователей
    */
    enum class EUserRole: quint8
    {
        UNDEFINED = 0,  ///< не определено
        NO_CONFIRMED = 1,///< пользователь зарегистрировался иждет подтверждения админа
        USER = 2, ///< пользовтель - обычный пользователь и может только заполнять анкеты
        ADMIN = 3, ///< пользователь - админ
        DELETED = 4   ///< пользователь удален или заблокирован
    };



    static  EUserRole intToEUserRole(quint8 role);

public:
    User(qint64 telegramID, const QString& firstName, const QString lastName, const QString userName, const QDateTime& addDateTime = QDateTime::currentDateTime());
    ~User();

    qint64 telegramID() const { return  _telegramID; }
    const QString& firstName() const { return _firstName; }
    const QString& lastName() const { return _lastName; }
    const QString& userName() const { return _userName; }
    const QDateTime& addDateTime() const { return _addDateTime; }


    EUserRole role() const;

    const QUuid& currentQUuid() const { return _currentUUID; };

    qint32 startQuestionnaire(Questionnaire* questionnaire);
    qint32 setAnswer(qint32 questionID, qint32 answerID);
    qint32 nextQuestionId();
    qint32 prevQuestionId();
    void finishQuestionnaire();
    void cancelQuestionnaire();
    qint32 maxQuestionId() const { return _maxQuestionId; };

private:
    void clear();
    void setRole(EUserRole role);

private:
    using QuestionsIterator = Questionnaire::Questions::const_iterator;

private:
    const qint64 _telegramID = 0;
    const QString _firstName;
    const QString _lastName;
    const QString _userName;
    const QDateTime _addDateTime = QDateTime::currentDateTime();
    EUserRole _role = EUserRole::UNDEFINED;

    Questionnaire* _questionnaire = nullptr;
    QUuid _currentUUID;
    Questionnaire::Questions _questions;
    QuestionsIterator _currentQuestions;
    QDateTime _startDateTime = QDateTime::currentDateTime();
    qint32 _maxQuestionId = -1;

};

class Users final
    : public QObject
{
    Q_OBJECT

public:
    explicit Users(const Common::DBConnectionInfo& dbConnectionInfo, QObject* parent = nullptr);
    ~Users();

    void loadFromDB();

    void addUser(const ::User& user);
    bool removeUser(qint32 id);

    User::EUserRole userRole(qint32 id) const;
    void setUserRole(qint32 id, User::EUserRole role);

    User& getUser(qint32 id);

    bool userExist(qint32 id) const;

    quint64 usersCount() const;

signals:
    void errorOccured(Common::EXIT_CODE errorCode, const QString &errorString);

private:
    Users() = delete;
    Q_DISABLE_COPY_MOVE(Users);

private:
    const Common::DBConnectionInfo& _dbConnectionInfo;

    QSqlDatabase _db;

    std::unordered_map<qint32, ::User> _users;

};


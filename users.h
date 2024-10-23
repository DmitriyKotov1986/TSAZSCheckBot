#pragma once

//STL
#include <unordered_map>
#include <unordered_set>
#include <memory>

//Qt
#include <QObject>
#include <QSqlDatabase>
#include <QDateTime>
#include <QUuid>

//My
#include <Common/common.h>

#include "user.h"

class Users final
    : public QObject
{
    Q_OBJECT

public:
    using UsersIDList = std::unordered_set<qint64>;

public:
    explicit Users(const Common::DBConnectionInfo& dbConnectionInfo, QObject* parent = nullptr);
    ~Users();

    void loadFromDB();

    void addUser(std::unique_ptr<::User> user_p);
    void removeUser(qint64 userId);

    UsersIDList userIdList() const;
    UsersIDList noConfirmUserIdList() const;
    UsersIDList confirmUserIdList() const;

    ::User& user(qint64 userId) const;
    const ::User& user_c(qint64 userId) const;

    bool userExist(qint64 userId) const;

    quint64 usersCount() const;

signals:
    void errorOccured(Common::EXIT_CODE errorCode, const QString &errorString);

private slots:
    void roleChenged(qint64 userId, User::EUserRole role);
    void stateChenged(qint64 userId, User::EUserState state);
    void addNewChat(qint64 userId, qint64 chatId, Chat::EChatState state);
    void chatStateChenged(qint64 userId, qint64 chatId, Chat::EChatState state);

private:
    Users() = delete;
    Q_DISABLE_COPY_MOVE(Users);

private:
    const Common::DBConnectionInfo& _dbConnectionInfo;

    QSqlDatabase _db;

    std::unordered_map<qint64, std::unique_ptr<::User>> _users;

};


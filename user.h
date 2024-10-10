#pragma once

//SLT
#include <unordered_map>
#include <unordered_set>
#include <memory>

//Qt
#include <QObject>
#include <QDateTime>
#include <QUuid>
#include <QVariant>

//My
#include <Common/common.h>

#include "chat.h"
#include "questionnaire.h"

class User final
    : public QObject
{
    Q_OBJECT

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

    static EUserRole intToEUserRole(quint8 role);

    enum class EUserState: quint8
    {
        UNDEFINED = 0,
        BLOCKED = 1,
        READY = 2,
        QUESTIONNAIRE = 3,
        USER_EDIT = 4,
        LOAD_QUESTIONNAIRE = 5,
        SAVE_RESULTS = 6
    };

    static EUserState intToEUserState(quint8 state);

    using ChatsIdList = std::unordered_set<qint32>;

public:
    User(qint64 telegramID, const QString& firstName, const QString& lastName, const QString& userName, EUserRole role, EUserState state, QObject* parent = nullptr);
    ~User();

    qint64 telegramID() const { return  _telegramID; }
    const QString& firstName() const { return _firstName; }
    const QString& lastName() const { return _lastName; }
    const QString& userName() const { return _userName; }

    EUserRole role() const;
    void setRole(EUserRole role);

    void addExistChat(std::unique_ptr<Chat> chat_p);
    void addNewChat(std::unique_ptr<Chat> chat_p);

    void setChatState(qint32 chatId, Chat::EChatState state);
    bool chatExist(qint32 chatId);
    Chat& chat(qint32 chatId) const;
    const Chat &chat_c(qint32 chatId)const;
    ChatsIdList chatIdList() const;
    ChatsIdList useChatIdList() const;

    EUserState state() const;
    void setState(EUserState state);

    const QUuid& currentQUuid() const { return _currentUUID; };

    qint32 startQuestionnaire(Questionnaire* questionnaire);
    qint32 setAnswer(qint32 questionID, const QVariant& answerID);

    qint32 nextQuestionId();
    qint32 prevQuestionId();
    qint32 currentQuestionId() const;
    void finishQuestionnaire();
    void cancelQuestionnaire();
    qint32 maxQuestionId() const { return _maxQuestionId; };

    void clear();

signals:
    void roleChenged(qint32 userId, User::EUserRole role);
    void stateChenged(qint32 userId, User::EUserState state);
    void addNewChat(qint32 userId, qint32 chatId, Chat::EChatState state);
    void chatStateChenged(qint32 userId, qint32 chatId, Chat::EChatState state);

private slots:
    void on_chatStateChenged(qint32 chatId, Chat::EChatState state);

private:
    using QuestionsIterator = Questionnaire::AnswersQuestions::const_iterator;

private:
    const qint64 _telegramID = 0;
    const QString _firstName;
    const QString _lastName;
    const QString _userName;
    EUserRole _role = EUserRole::UNDEFINED;
    EUserState _state = EUserState::UNDEFINED;
    std::unordered_map<qint32, std::unique_ptr<Chat>> _chats;

    Questionnaire* _questionnaire = nullptr;
    QUuid _currentUUID;
    Questionnaire::AnswersQuestions _answersQuestions;
    QuestionsIterator _currentQuestions;
    QDateTime _startDateTime = QDateTime::currentDateTime();
    qint32 _maxQuestionId = -1;

};

Q_DECLARE_METATYPE(User::EUserRole)
Q_DECLARE_METATYPE(User::EUserState)

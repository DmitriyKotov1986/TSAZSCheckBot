#include "user.h"

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

User::EUserState User::intToEUserState(quint8 state)
{
    switch (state)
    {
    case static_cast<quint8>(EUserState::LOAD_QUESTIONNAIRE): return EUserState::LOAD_QUESTIONNAIRE;
    case static_cast<quint8>(EUserState::QUESTIONNAIRE): return EUserState::QUESTIONNAIRE;
    case static_cast<quint8>(EUserState::USER_EDIT): return EUserState::USER_EDIT;
    case static_cast<quint8>(EUserState::READY): return EUserState::READY;
    case static_cast<quint8>(EUserState::BLOCKED): return EUserState::BLOCKED;
    case static_cast<quint8>(EUserState::UNDEFINED):
    default:
        break;
    }

    return EUserState::UNDEFINED;
}

User::User(qint64 telegramID, const QString &firstName, const QString& lastName, const QString& userName,
           EUserRole role, EUserState state, QObject* parent /* = nullptr */)
    : QObject{parent}
    , _telegramID(telegramID)
    , _firstName(firstName)
    , _lastName(lastName)
    , _userName(userName)
    , _role(role)
    , _state(state)
{
    Q_ASSERT(role != EUserRole::UNDEFINED);
    Q_ASSERT(state != EUserState::UNDEFINED);

    qRegisterMetaType<User::EUserRole>("User::EUserRole");
    qRegisterMetaType<User::EUserState>("User::EUserState");
}

User::~User()
{
}

::User::EUserRole User::role() const
{
    return _role;
}

User::EUserState User::state() const
{
    return _state;
}

void User::setRole(EUserRole role)
{
    Q_ASSERT(role != EUserRole::UNDEFINED);

    _role = role;

    if (role == EUserRole::DELETED)
    {
        for (auto& chat: _chats)
        {
            chat.second->setState(Chat::EChatState::DELETED);
        }
    }

    emit roleChenged(_telegramID, _role);
}

void User::addNewChat(std::unique_ptr<Chat> chat_p)
{
    QObject::connect(chat_p.get(), SIGNAL(stateChenged(qint64, Chat::EChatState)), SLOT(on_chatStateChenged(qint64, Chat::EChatState)));

    const auto chatId = chat_p->chatId();
    const auto chatState = chat_p->state();

    _chats.emplace(chatId, std::move(chat_p));

    emit addNewChat(_telegramID, chatId, chatState);
}

void User::addExistChat(std::unique_ptr<Chat> chat_p)
{
    Q_CHECK_PTR(chat_p);

    QObject::connect(chat_p.get(), SIGNAL(stateChenged(qint64, Chat::EChatState)), SLOT(on_chatStateChenged(qint64, Chat::EChatState)));

    const auto chatId = chat_p->chatId();

    _chats.emplace(chatId, std::move(chat_p));
}

void User::setChatState(qint64 chatId, Chat::EChatState state)
{
    Q_ASSERT(chatId != 0);
    Q_ASSERT(state != Chat::EChatState::UNDEFINED);
    Q_ASSERT(_chats.contains(chatId));

    auto chat_p = _chats.at(chatId).get();
    chat_p->setState(state);
}

bool User::chatExist(qint64 chatId)
{
    return _chats.contains(chatId);
}

Chat &User::chat(qint64 chatId) const
{
    Q_ASSERT(chatId != 0);
    Q_ASSERT(_chats.contains(chatId));

    return *_chats.at(chatId).get();
}

const Chat &User::chat_c(qint64 chatId) const
{
    return chat(chatId);
}

User::ChatsIdList User::chatIdList() const
{
    ChatsIdList result;

    for (const auto& chat: _chats)
    {
        result.insert(chat.first);
    }

    return result;
}

User::ChatsIdList User::useChatIdList() const
{
    ChatsIdList result;

    for (const auto& chat: _chats)
    {
        if (chat.second->state() == Chat::EChatState::USE)
        {
            result.insert(chat.first);
        }
    }

    return result;
}

void User::setState(EUserState state)
{
    Q_ASSERT(state != EUserState::UNDEFINED);

    _state = state;

    emit stateChenged(_telegramID, _state);
}

qint32 User::startQuestionnaire(Questionnaire *questionnaire)
{
    Q_CHECK_PTR(questionnaire);

    _questionnaire = questionnaire;
    _answersQuestions = _questionnaire->makeAnswersQuestions();
    _currentQuestions = _answersQuestions.begin();
    _startDateTime = QDateTime::currentDateTime();
    _currentUUID = QUuid::createUuid();
    _maxQuestionId = _currentQuestions != _answersQuestions.end() ? _currentQuestions->first : -1;

    return _currentQuestions != _answersQuestions.end() ? _currentQuestions->first : -1;
}

qint32 User::setAnswer(qint32 questionID, const QVariant& answer)
{
    Q_ASSERT(!answer.isNull());

    auto questions_it = _answersQuestions.find(questionID);

    Q_ASSERT(questions_it != _answersQuestions.end());

    questions_it->second = answer;

    _currentQuestions = questions_it;

    return nextQuestionId();
}

qint32 User::nextQuestionId()
{
    if (_currentQuestions != _answersQuestions.end())
    {
        _currentQuestions = std::next(_currentQuestions);
        _maxQuestionId = std::max(_maxQuestionId, _currentQuestions->first);
    }
    return currentQuestionId();
}

qint32 User::prevQuestionId()
{
    if (_currentQuestions != _answersQuestions.end())
    {
        _currentQuestions = std::prev(_currentQuestions);
    }
    return currentQuestionId();
}

qint32 User::currentQuestionId() const
{
    return _currentQuestions != _answersQuestions.end() ? _currentQuestions->first : -1;
}

void User::finishQuestionnaire()
{
    Q_CHECK_PTR(_questionnaire);

    _questionnaire->saveResults(_telegramID, _currentUUID, _startDateTime, _answersQuestions);

    clear();
}

void User::cancelQuestionnaire()
{
    Q_CHECK_PTR(_questionnaire);

    clear();
}

void User::clear()
{
    _answersQuestions.clear();
    _currentQuestions = _answersQuestions.end();
    _questionnaire = nullptr;
    _currentUUID = QUuid();
    _maxQuestionId = -1;
}

void User::on_chatStateChenged(qint64 chatId, Chat::EChatState state)
{
    emit chatStateChenged(_telegramID, chatId, state);
}

#include "chat.h"

///////////////////////////////////////////////////////////////////////////////
/// class User
Chat::EChatState Chat::intToEChatState(quint8 state)
{
    switch (state)
    {
    case static_cast<quint8>(EChatState::DELETED): return EChatState::DELETED;
    case static_cast<quint8>(EChatState::USE): return EChatState::USE;
    case static_cast<quint8>(EChatState::UNDEFINED):
    default:
        break;
    }

    return EChatState::UNDEFINED;
}

Chat::Chat(qint64 chatId, EChatState state)
    : _chatId(chatId)
    , _state(state)
{
    Q_ASSERT(state != EChatState::UNDEFINED);

    qRegisterMetaType<Chat::EChatState>("Chat::EChatState");
}

Chat::~Chat()
{
}

qint64 Chat::chatId() const
{
    return _chatId;
}

Chat::EChatState Chat::state() const
{
    return _state;
}

void Chat::setState(EChatState state)
{
    Q_ASSERT(state != EChatState::UNDEFINED);

    _state = state;

    emit stateChenged(_chatId, state);
}

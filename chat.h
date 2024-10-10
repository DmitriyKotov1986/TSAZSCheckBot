#pragma once

//STL
#include <optional>

//Qt
#include <QObject>

class Chat
    : public QObject
{
    Q_OBJECT

public:
    enum class EChatState: quint8
    {
        UNDEFINED = 0,
        USE = 1,
        DELETED = 2
    };

    static EChatState intToEChatState(quint8 state);

    using RemoveDateTime = std::optional<QDateTime>;

public:
    explicit Chat(qint32 chatId, EChatState state);
    ~Chat();

    qint32 chatId() const;

    EChatState state() const;
    void setState(EChatState state);

signals:
    void stateChenged(qint32 chatId, Chat::EChatState state);

private:
    Chat() = delete;
    Q_DISABLE_COPY_MOVE(Chat);

private:
    qint32 _chatId  = 0;
    EChatState _state = EChatState::UNDEFINED;

};

Q_DECLARE_METATYPE(Chat:: EChatState)

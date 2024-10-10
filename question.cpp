#include "question.h"

Question::EQuestionType Question::intToEQuestionType(quint8 type)
{
    switch (type)
    {
    case static_cast<quint8>(EQuestionType::DELETED): return EQuestionType::DELETED;
    case static_cast<quint8>(EQuestionType::CHECHED): return EQuestionType::CHECHED;
    case static_cast<quint8>(EQuestionType::TEXT): return EQuestionType::TEXT;
    case static_cast<quint8>(EQuestionType::UNDEFINED):
    default:
        break;
    }

    return EQuestionType::UNDEFINED;
}

Question::EAnswerType Question::intToEAnswerType(quint8 type)
{
    switch (type)
    {
    case static_cast<quint8>(EAnswerType::DELETED): return EAnswerType::DELETED;
    case static_cast<quint8>(EAnswerType::CHECHED): return EAnswerType::CHECHED;
    case static_cast<quint8>(EAnswerType::UNDEFINED):
    default:
        break;
    }

    return EAnswerType::UNDEFINED;
}

Question::Question(qint32 index, const QString &text, EQuestionType type)
    : _index(index)
    , _text(text)
    , _type(type)
{
    Q_ASSERT(!text.isEmpty());
    Q_ASSERT(type != EQuestionType::UNDEFINED);
}

qint32 Question::index() const
{
    return _index;
}

const QString &Question::text() const
{
    return _text;
}

Question::EQuestionType Question::type() const
{
    return _type;
}

void Question::addAnswer(qint32 index, const QVariant& data)
{
    Q_ASSERT(index != 0);
    Q_ASSERT(!data.isNull());
    Q_ASSERT(_type != Question::EQuestionType::TEXT);

    _answers.insert({index, data});
}

const Question::AnswersList& Question::getAnswersList() const
{
    return _answers;
}

const QVariant& Question::getAnswer(qint32 index)
{
    Q_ASSERT(index != 0);
    Q_ASSERT(_type != Question::EQuestionType::TEXT);
    Q_ASSERT(_answers.contains(index));

    return _answers.at(index);
}

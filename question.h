#pragma once

//STL
#include <map>

//Qt
#include <QString>
#include <QVariant>

class Question
{
public:
    using AnswersList = std::map<qint32, QVariant>;

    enum class EQuestionType: quint8
    {
        UNDEFINED = 0,
        DELETED = 1,
        CHECHED = 2,
        TEXT = 3
    };

    static EQuestionType intToEQuestionType(quint8 type);

    enum class EAnswerType: quint8
    {
        UNDEFINED = 0,
        DELETED = 1,
        CHECHED = 2
    };

    static EAnswerType intToEAnswerType(quint8 type);

public:
    Question(qint32 index, const QString& text, EQuestionType type);

    qint32 index() const;
    const QString& text() const;
    EQuestionType type() const;

    void addAnswer(qint32 index, const QVariant& data);
    const AnswersList& getAnswersList() const;
    const QVariant& getAnswer(qint32 index);

private:
    const quint32 _index = 0;
    const QString _text;
    EQuestionType _type = EQuestionType::UNDEFINED;

    AnswersList _answers;
};

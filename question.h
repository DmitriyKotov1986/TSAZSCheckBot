#pragma once

//STL
#include <map>

//Qt
#include <QString>

class Question
{
public:
    using CheckAnswersList = std::map<qint32, QString>;

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
        CHECHED = 2,
        DEFAULT_TEXT = 3
    };

    static EAnswerType intToEAnswerType(quint8 type);

public:
    Question(qint32 index, const QString& text, EQuestionType type);

    qint32 index() const;
    const QString& text() const;
    EQuestionType type() const;

    void addAnswer(qint32 index, const QString& text);
    const CheckAnswersList& getAnswersList() const;

private:


private:
    const quint32 _index = 0;
    const QString _text;
    EQuestionType _type = EQuestionType::UNDEFINED;

    CheckAnswersList _answers;
};

#pragma once

//STL
#include <map>
#include <memory>
#include <list>

//Qt
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUuid>
#include <QJsonDocument>
#include <QVariant>
#include <QJsonParseError>
#include <QSqlDatabase>

//My
#include <Common/common.h>
#include <Common/sql.h>

#include <question.h>

class Questionnaire
    : public QObject
{
    Q_OBJECT

public:
    using QuestionsIndexList = std::list<qint32>;
    using AnswersQuestions = std::map<qint32, QVariant>; //Key -  QuestionID, value - AnswerID or answer text

public:
    explicit Questionnaire(const Common::DBConnectionInfo& dbConnectionInfo, QObject* parent = nullptr);

    void loadFromDB();
    QString loadFromString(const QString& data);
    QString toString();

    const Question& question(qint32 index) const;
    qint32 questionCount() const;
    qint32 questionPosition(qint32 index) const;

    AnswersQuestions makeAnswersQuestions() const;
    void saveResults(qint64 userId, const QUuid& uuid, const QDateTime startDateTime, const AnswersQuestions& questions);

    QString getAllResults(const QDateTime& startDateTime, const QDateTime& endDateTime);

signals:
    void errorOccurred(Common::EXIT_CODE errorCode, const QString &errorString);

private:
    Questionnaire() = delete;
    Q_DISABLE_COPY_MOVE(Questionnaire)

    static QString QJsonParseErrorToString(QJsonParseError::ParseError error);

private:
    using QuestionsMap = std::map<qint32, std::unique_ptr<Question>>;

    struct AnswerData
    {
        qint32 questionId = 0;
        qint32 index = 0;
        QString text;
        Question::EAnswerType type = Question::EAnswerType::UNDEFINED;
    };

    struct QuestionData
    {
        qint32 index = 0;
        QString text;
        Question::EQuestionType type = Question::EQuestionType::UNDEFINED;
        std::map<qint32, AnswerData> answers;
    };

    using QuestionsData = std::map<qint32, QuestionData>;

private:
    const Common::DBConnectionInfo& _dbConnectionInfo;
    QSqlDatabase _db;

    QString _currentAZSCode;

    QuestionsMap _questions;

};


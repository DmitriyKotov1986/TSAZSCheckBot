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

//My
#include <Common/common.h>

#include <question.h>

class Questionnaire
    : public QObject
{
    Q_OBJECT

public:
    using QuestionsIndexList = std::list<qint32>;
    using Questions = std::map<qint32, qint32>; //Key -  QuestionID, value - AnswerID

public:
    explicit Questionnaire(const Common::DBConnectionInfo& dbConnectionInfo, QObject* parent = nullptr);

    void loadFromDB();
    void loadFromFile(const QString& fileName);
    QString toString();

    const Question& question(qint32 index) const;
    qint32 questionCount() const;
    qint32 questionPosition(qint32 index) const;

    Questions makeQuesions() const;
    void saveResults(qint32 userId, const QUuid& uuid, const QDateTime startDateTime, const Questions& questions);

    QJsonDocument getAllResults();

signals:
    void errorOccured(Common::EXIT_CODE errorCode, const QString &errorString);

private:
    Questionnaire() = delete;
    Q_DISABLE_COPY_MOVE(Questionnaire)

private:
    using QuestionsMap = std::map<qint32, std::unique_ptr<Question>>;

private:
    const Common::DBConnectionInfo& _dbConnectionInfo;
    QSqlDatabase _db;

    QString _currentAZSCode;

    QuestionsMap _questions;

};


//STL
#include <limits>

//Qt
#include "QJsonArray"
#include "QJsonObject"

#include "questionnaire.h"

static const QString DB_CONNECTION_NAME = "QuestionnaireDB";

using namespace Common;

Questionnaire::Questionnaire(const Common::DBConnectionInfo& dbConnectionInfo, QObject* parent /* = nullptr */)
    : _dbConnectionInfo(dbConnectionInfo)
{
}

void Questionnaire::loadFromDB()
{
    try
    {
        connectToDB(_db, _dbConnectionInfo, DB_CONNECTION_NAME);
    }
    catch (const SQLException& err)
    {
        emit errorOccured(EXIT_CODE::SQL_NOT_CONNECT, err.what());

        return;
    }

    class LoadException
        : public std::runtime_error
    {
    public:
        LoadException(const QString& err)
            : std::runtime_error(err.toStdString())
        {}

    private:
        LoadException() = delete;

    };

    try
    {
        transactionDB(_db);

        QSqlQuery query(_db);
        query.setForwardOnly(true);

        auto queryText =
            QString("SELECT `ID`, `Index`, `Text`, `Type` "
                    "FROM `Questions` "
                    "WHERE `Type` <> %1 "
                    "ORDER BY `Index` ")
            .arg(static_cast<quint8>(Question::EQuestionType::DELETED));

        DBQueryExecute(_db, query, queryText);

        QStringList indexes;
        while (query.next())
        {
            const auto recordId =  query.value("ID").toULongLong();
            const qint32 index = query.value("Index").toInt();
            if (index <= 0)
            {
                throw LoadException(QString("Text cannot be null or less in [Questions]/Index. Record ID: %1").arg(recordId));
            }

            const auto text = query.value("Text").toString();
            if (text.isEmpty())
            {
                throw LoadException(QString("Text cannot be empty [Questions]/Text. Record ID: %1").arg(recordId));
            }

            const auto type = Question::intToEQuestionType(query.value("Type").toInt());
            if (type == Question::EQuestionType::UNDEFINED)
            {
                throw LoadException(QString("Question type cannot be UNDEFINE [Questions]/Type. Record ID: %1").arg(recordId));
            }

            auto question = std::make_unique<Question>(index, text, type);

            indexes.push_back(QString::number(index));
            _questions.emplace(index, std::move(question));
        }

        queryText =
            QString("SELECT `ID`, `QuestionID`, `Index`, `Text`, `Type` "
                    "FROM `Answers` "
                    "WHERE `QuestionID` IN (%1) AND `Type` <> %2 "
                    "ORDER BY `Index` ")
                .arg(indexes.join(u','))
                .arg(static_cast<quint8>(Question::EAnswerType::DELETED));

        DBQueryExecute(_db, query, queryText);

        while (query.next())
        {
            const auto recordId =  query.value("ID").toULongLong();
            const qint32 index = query.value("Index").toInt();
            const qint32 questionId = query.value("QuestionID").toInt();
            const QString text = query.value("Text").toString();

            auto questions_it = _questions.find(questionId);
            if (questions_it != _questions.end())
            {
                if (questions_it->second->type() == Question::EQuestionType::CHECHED)
                {
                    questions_it->second->addAnswer(index, text);
                }
                else  if (questions_it->second->type() == Question::EQuestionType::TEXT)
                {
                    questions_it->second->addAnswer(1, text);
                }
                else
                {
                    throw LoadException(QString("Question has check answers but have type CHECKED in [Answers]. Record ID: %1").arg(recordId));
                }
            }
        }

        commitDB(_db);
    }
    catch (const SQLException& err)
    {        
        emit errorOccured(EXIT_CODE::SQL_NOT_CONNECT, err.what());

        return;
    }
    catch (const LoadException& err)
    {
        _db.rollback();

        emit errorOccured(EXIT_CODE::LOAD_CONFIG_ERR, err.what());

        return;
    }
}

void Questionnaire::loadFromFile(const QString &fileName)
{

}

QString Questionnaire::toString()
{
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

    std::map<qint32, QuestionData> questions;

    try
    {
        transactionDB(_db);

        QSqlQuery query(_db);
        query.setForwardOnly(true);

        auto queryText =
            QString("SELECT `ID`, `Index`, `Text`, `Type` "
                    "FROM `Questions` "
                    "ORDER BY `Index` ")
                .arg(static_cast<quint8>(Question::EQuestionType::DELETED));

        DBQueryExecute(_db, query, queryText);

        while (query.next())
        {
            const auto index = static_cast<qint32>(query.value("Index").toInt());

            QuestionData question;

            question.index = index;
            question.text = query.value("Text").toString();
            question.type = Question::intToEQuestionType(query.value("Type").toInt());

            questions.emplace(index, std::move(question));
        }

        queryText =
            QString("SELECT `ID`, `QuestionID`, `Index`, `Text`, `Type` "
                    "FROM `Answers` "
                    "ORDER BY `Index` ");

        DBQueryExecute(_db, query, queryText);

        while (query.next())
        {
            const auto index = static_cast<qint32>(query.value("Index").toInt());

            AnswerData answer;

            answer.index = index;
            answer.questionId = static_cast<qint32>(query.value("QuestionID").toInt());
            answer.text = query.value("Text").toString();
            answer.type = Question::intToEAnswerType(query.value("Type").toInt());

            auto questions_it = questions.find(answer.questionId);
            if (questions_it == questions.end())
            {
                questions_it = questions.emplace(std::numeric_limits<qint32>::max(), QuestionData{}).first;
            }

            questions_it->second.answers.emplace(index, std::move(answer));
        }

        commitDB(_db);
    }
    catch (const SQLException& err)
    {
        emit errorOccured(EXIT_CODE::SQL_NOT_CONNECT, err.what());

        return {};
    }

    QJsonArray questionsJSON;

    for (const auto& question: questions)
    {
        QJsonObject questionJSON;

        questionJSON.insert("Index", question.second.index);
        questionJSON.insert("Text", question.second.text);
        questionJSON.insert("Type", static_cast<quint8>(question.second.type));

        QJsonArray answersJSON;
        for (const auto& answer: question.second.answers)
        {
            QJsonObject answerJSON;
            answerJSON.insert("Index", answer.second.index);
            answerJSON.insert("Text", answer.second.text);
            answerJSON.insert("Type", static_cast<quint8>(answer.second.type));

            answersJSON.push_back(answerJSON);
        }
        questionJSON.insert("Answers", answersJSON);

        questionsJSON.push_back(questionJSON);
    }

    QJsonDocument result(questionsJSON);

    return result.toJson();
}

const Question &Questionnaire::question(qint32 index) const
{
    const auto questions_it = _questions.find(index);
    if (questions_it == _questions.end())
    {
        Q_ASSERT(false);
    }

    return *questions_it->second.get();
}

qint32 Questionnaire::questionCount() const
{
    return _questions.size();
}

qint32 Questionnaire::questionPosition(qint32 index) const
{
    const auto questions_it = _questions.find(index);
    if (questions_it == _questions.end())
    {
        Q_ASSERT(false);
    }
    return std::distance(_questions.begin(), questions_it) + 1;
}

Questionnaire::Questions Questionnaire::makeQuesions() const
{
    Questions result;
    for (const auto& question: _questions)
    {
        result.insert({question.first, -1});
    }

    return result;
}

void Questionnaire::saveResults(qint32 userId, const QUuid &uuid, const QDateTime startDateTime, const Questions &questions)
{
    Q_ASSERT(_db.open());

    try
    {
        auto queryText =
            QString("INSERT INTO `ResultQuestionnaire` "
                    "(`UUID`, `UserID`, `StartDateTime`, `FinishDateTime`) "
                    "VALUES ('%1', %2, CAST('%3' AS DATETIME), CAST('%4' AS DATETIME))")
                .arg(uuid.toString())
                .arg(userId)
                .arg(startDateTime.toString(DATETIME_FORMAT))
                .arg(QDateTime::currentDateTime().toString(DATETIME_FORMAT));

        DBQueryExecute(_db, queryText);

        for (const auto& question: questions)
        {
            auto queryText =
                QString("INSERT INTO `ResultQuestion` "
                        "(`QuestionnaireUUID`, `QuestionID`,`AnswerID`) "
                                     "VALUES('%1', %2, %3)")

                    .arg(uuid.toString())
                    .arg(question.first)
                    .arg(question.second);

            DBQueryExecute(_db, queryText);
        }
    }
    catch (const SQLException& err)
    {
        emit errorOccured(EXIT_CODE::SQL_NOT_CONNECT, err.what());

        return;
    }
}

QJsonDocument Questionnaire::getAllResults()
{
    Q_ASSERT(_db.open());

    QJsonArray result;
    try
    {
        transactionDB(_db);

        QSqlQuery query(_db);
        query.setForwardOnly(true);

        const auto queryText =
            QString("SELECT `UUID`, `UserName`, `StartDateTime`, `FinishDateTime`, `QuestionText`,  `AnswerText` "
                    "FROM `ResultQuestionnaire` AS A "
                    "INNER JOIN ( "
                        "SELECT `QuestionnaireUUID`, `QuestionText`, `AnswerText` "
                        "FROM `ResultQuestion` AS B "
                        "INNER JOIN ( "
                            "SELECT `Index`, `Text` AS `QuestionText` "
                            "FROM `Questions` "
                        ") AS C "
                        "ON C.`Index` = B.`QuestionID` "
                        "INNER JOIN ( "
                            "SELECT `QuestionID`, `Index`, `Text` AS `AnswerText` "
                            "FROM `Answers` "
                        ") AS D "
                        "ON D.`Index` = B.`AnswerID` AND D.`QuestionID` = B.`QuestionID` "
                    ") AS E "
                    "ON E.`QuestionnaireUUID` = A.`UUID` "
                    "INNER JOIN ( "
                        "SELECT `TelegramID`, `UserName` "
                        "FROM `Users` "
                    ") AS F "
                    "ON F.`TelegramID` = A.`UserID` "
                    "ORDER BY `UUID`, `FinishDateTime`");

        DBQueryExecute(_db, query, queryText);

        while (query.next())
        {
            QJsonObject answer;
            answer.insert("UUID", query.value("UUID").toString());
            answer.insert("UserName", query.value("UserName").toString());
            answer.insert("StartDateTime", query.value("StartDateTime").toString());
            answer.insert("FinishDateTime", query.value("FinishDateTime").toString());
            answer.insert("QuestionText", query.value("QuestionText").toString());
            answer.insert("AnswerText", query.value("AnswerText").toString());

            result.push_back(answer);
        }

        commitDB(_db);
    }
    catch (const SQLException& err)
    {
        emit errorOccured(EXIT_CODE::SQL_NOT_CONNECT, err.what());

        return QJsonDocument{};
    }

    return QJsonDocument(result);
}


//STL
#include <limits>

//Qt
#include <QJsonArray>
#include <QJsonObject>

//My
#include "QtCSV/stringdata.h"
#include "QtCSV/writer.h"

#include "questionnaire.h"

static const QString DB_CONNECTION_NAME = "QuestionnaireDB";

using namespace Common;
using namespace QtCSV;

Questionnaire::Questionnaire(const Common::DBConnectionInfo& dbConnectionInfo, QObject* parent /* = nullptr */)
    : QObject{parent}
    , _dbConnectionInfo(dbConnectionInfo)

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
                throw LoadException(QString("Index cannot be null or less in [Questions]/Index. Record ID: %1").arg(recordId));
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
            const qint32 questionId = query.value("QuestionID").toInt();
            if (questionId <= 0)
            {
                throw LoadException(QString("QuestionId cannot be null or less in [Answers]/QuestionId. Record ID: %1").arg(recordId));
            }

            const qint32 index = query.value("Index").toInt();
            if (index <= 0)
            {
                throw LoadException(QString("Index cannot be null or less in [Answers]/Index. Record ID: %1").arg(recordId));
            }

            const QString text = query.value("Text").toString();

            auto questions_it = _questions.find(questionId);
            if (questions_it != _questions.end())
            {
                const auto& question = questions_it->second;
                switch (question->type())
                {
                case Question::EQuestionType::CHECHED:
                {
                    if (text.isEmpty())
                    {
                        throw LoadException(QString("Answer text cannot be empty. Record ID: %2").arg(questionId).arg(recordId));
                    }
                    questions_it->second->addAnswer(index, text);

                    break;
                }
                case Question::EQuestionType::TEXT:
                {
                     throw LoadException(QString("Question has type TEXT cannot have check answers. Record ID: %1").arg(recordId));
                }
                case Question::EQuestionType::DELETED:
                case Question::EQuestionType::UNDEFINED:
                default:
                {
                    throw LoadException(QString("Incorrect question type for aswer. Record ID: %1").arg(recordId));
                }
                }
            }
            else
            {
                throw LoadException(QString("Question with ID %1 not exist. Record ID: %2").arg(questionId).arg(recordId));
            }
        }

        commitDB(_db);
    }
    catch (const SQLException& err)
    {
        _db.rollback();

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

QString Questionnaire::QJsonParseErrorToString(QJsonParseError::ParseError error)
{
    switch (error)
    {
    case QJsonParseError::NoError: return tr("No error");
    case QJsonParseError::UnterminatedObject: return tr("An object is not correctly terminated with a closing curly bracket");
    case QJsonParseError::MissingNameSeparator: return tr("A comma separating different items is missing");
    case QJsonParseError::UnterminatedArray: return tr("The array is not correctly terminated with a closing square bracket");
    case QJsonParseError::MissingValueSeparator: return tr("A colon separating keys from values inside objects is missing");
    case QJsonParseError::IllegalValue: return tr("The value is illegal");
    case QJsonParseError::TerminationByNumber: return tr("The input stream ended while parsing a number");
    case QJsonParseError::IllegalNumber: return tr("The number is not well formed");
    case QJsonParseError::IllegalEscapeSequence: return tr("An illegal escape sequence occurred in the input");
    case QJsonParseError::IllegalUTF8String: return tr("An illegal UTF8 sequence occurred in the input");
    case QJsonParseError::UnterminatedString: return tr("A string wasn't terminated with a quote");
    case QJsonParseError::MissingObject: return tr("An object was expected but couldn't be found");
    case QJsonParseError::DeepNesting: return tr("The JSON document is too deeply nested for the parser to parse it");
    case QJsonParseError::DocumentTooLarge: return tr("The JSON document is too large for the parser to parse it");
    case QJsonParseError::GarbageAtEnd: return tr("The parsed document contains additional garbage characters at the end");
    default:
        break;
    }

    return tr("Unknow error");
}

QString Questionnaire::loadFromString(const QString &data)
{
    Q_ASSERT(!data.isEmpty());

    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(data.toUtf8(), &error);

    if (error.error != QJsonParseError::NoError)
    {
        return tr("Error parsing questionnaire on line: %1. Error: %2")
                    .arg(error.offset)
                    .arg(QJsonParseErrorToString(error.error));
    }

    if (!document.isArray())
    {
        return tr("Questionnaire must contain array questions");
    }

    const auto questionsJson = document.array();
    if (questionsJson.isEmpty())
    {
        return tr("Questionnaire cannot be empty");
    }

    QuestionsData questions;

    for (int i = 0; i < questionsJson.count(); ++i)
    {
        const auto questionJsonValue = questionsJson.at(i);
        if (!questionJsonValue.isObject())
        {
            return tr("Questions have incorrect format (is not object). Array index: %1").arg(i);
        }
        const auto questionJson = questionJsonValue.toObject();

        QuestionData questionData;
        questionData.index = questionJson.value("Index").toInt();
        if (questionData.index <= 0)
        {
            return tr("Field 'Index' of question cannot be equil null or negative or value undefine or have incorrect format. Array index: %1").arg(i);
        }
        if (questions.contains(questionData.index))
        {
            return tr("Questionnaire must have only one question with index: %1").arg(questionData.index);
        }

        questionData.type = Question::intToEQuestionType(questionJson.value("Type").toInt());
        if (questionData.type == Question::EQuestionType::UNDEFINED)
        {
            return tr("Field 'Type' of question must be define and have value equil 1 for deleted questions, equil 2 for checked question or equil 3 for text question. Question index: %1").arg(questionData.index);
        }

        questionData.text = questionJson.value("Text").toString();
        if (questionData.text.isEmpty())
        {
            return tr("Field 'Text' of question cannot be empty or undefine. Question index: %1").arg(questionData.index);
        }

        const auto answersJsonValue = questionJson.value("Answers");
        if (!answersJsonValue.isArray())
        {
            return tr("Answers for question fave incorrect format (is not array). Question index: %1").arg(questionData.index);
        }

        const auto answersJson = answersJsonValue.toArray();
        for (int j = 0; j < answersJson .count(); ++j)
        {
            const auto answerJsonValue = answersJson.at(j);
            if (!answerJsonValue.isObject())
            {
                return tr("Answer for question fave incorrect format (is not object). Question index: %1. Array of answer index: %2").arg(questionData.index). arg(j);
            }

            const auto answerJson = answerJsonValue.toObject();

            AnswerData answerData;
            answerData.index = answerJson.value("Index").toInt();
            if (answerData.index <= 0)
            {
                return tr("Field 'Index' of answer question cannot be equil null or negative or value undefine or have incorrect format. Question index: %1. Array of answer index: %2 ").arg(questionData.index).arg(j);
            }
            if (questionData.answers.contains(answerData.index))
            {
                return tr("The question must have only one answer with index %1. Question index: %2").arg(answerData.index).arg(questionData.index);
            }

            answerData.type = Question::intToEAnswerType(answerJson.value("Type").toInt());
            if (answerData.type == Question::EAnswerType::UNDEFINED)
            {
                return tr("Field 'Type' of answer question must be define and have value equil 1 for deleted questions, equil 2 for checked question. Question index: %1. Answer index: %2").arg(questionData.index).arg(answerData.index);
            }
            if (questionData.type == Question::EQuestionType::TEXT)
            {
                return tr("The question with type TEXT cannot have answers. Question index: %1. Answer index: %2").arg(questionData.index).arg(answerData.index);
            }

            answerData.text = answerJson.value("Text").toString();
            if (answerData.text.isEmpty())
            {
                return tr("Field 'Text' of answer question cannot be empty or undefine. Question index: %1. Answer index: %2").arg(questionData.index).arg(answerData.index);
            }

            questionData.answers.insert({answerData.index, answerData});
        }

        if (questionData.type == Question::EQuestionType::TEXT && !questionData.answers.empty())
        {
            return tr("The question with type TEXT cannot have answers. Question index: %1").arg(questionData.index);
        }
        if (questionData.type == Question::EQuestionType::CHECHED && questionData.answers.empty())
        {
            return tr("The question with type CHECHED cannot not have answers. Question index: %1").arg(questionData.index);
        }

        questions.insert({questionData.index, questionData});
    }

    try
    {
        transactionDB(_db);

        QSqlQuery query(_db);
        query.setForwardOnly(true);

        //очищаем таблицы с вопросами и ответами
        auto queryText = QString("TRUNCATE `Questions`");

        DBQueryExecute(_db, query, queryText);

        queryText = QString("TRUNCATE `Answers`");

        DBQueryExecute(_db, query, queryText);

        for (const auto& questionData: questions)
        {
            const auto& question = questionData.second;
            queryText = QString("INSERT INTO `Questions` (`Index`, `Text`, `Type`) "
                                "VALUES (%1, '%2', %3)")
                            .arg(question.index)
                            .arg(question.text)
                            .arg(static_cast<quint8>(question.type));

            DBQueryExecute(_db, query, queryText);

            for (const auto& answerData: question.answers)
            {
                const auto& answer = answerData.second;

                queryText = QString("INSERT INTO `Answers` (`QuestionID`, `Index`, `Text`, `Type`) "
                                    "VALUES (%1, %2, '%3', %4)")
                                .arg(question.index)
                                .arg(answer.index)
                                .arg(answer.text)
                                .arg(static_cast<quint8>(answer.type));

                DBQueryExecute(_db, query, queryText);
            }
        }

        commitDB(_db);
    }
    catch (const SQLException& err)
    {
        _db.rollback();

        emit errorOccured(EXIT_CODE::SQL_NOT_CONNECT, err.what());

        return tr("Cannot save questionnaire to DB");
    }

    return {};
}


QString Questionnaire::toString()
{
    QuestionsData questions;
    try
    {
        transactionDB(_db);

        QSqlQuery query(_db);
        query.setForwardOnly(true);

        auto queryText =
            QString("SELECT `ID`, `Index`, `Text`, `Type` "
                    "FROM `Questions` "
                    "ORDER BY `Index` ");

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

Questionnaire::AnswersQuestions  Questionnaire::makeAnswersQuestions() const
{
    AnswersQuestions result;
    for (const auto& question: _questions)
    {
        switch (question.second->type())
        {
        case Question::EQuestionType::CHECHED:
            result.insert({question.first, QVariant(-1)});
            break;
        case Question::EQuestionType::TEXT:
            result.insert({question.first, QVariant(QString())});
            break;
        case Question::EQuestionType::DELETED:
        case Question::EQuestionType::UNDEFINED:
        default:
            Q_ASSERT(false);
            break;
        }
    }

    return result;
}

void Questionnaire::saveResults(qint64 userId, const QUuid &uuid, const QDateTime startDateTime, const AnswersQuestions& questions)
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

            const auto& questionInfo = _questions.at(question.first);
            const auto& questionText = questionInfo->text();

            QString answerText;
            switch (questionInfo->type())
            {
            case Question::EQuestionType::TEXT:
            {
                answerText = question.second.toString();
                if (answerText .isEmpty())
                {
                    continue;
                }
                break;
            }
            case Question::EQuestionType::CHECHED:
            {
                const auto answerIndex = question.second.toInt();
                if (answerIndex <= 0)
                {
                    continue;
                }

                answerText = questionInfo->getAnswer(answerIndex).toString();
                break;
            }
            case Question::EQuestionType::DELETED:
            case Question::EQuestionType::UNDEFINED:
            default:
                Q_ASSERT(false);
            }

            auto queryText =
                QString("INSERT INTO `ResultQuestion` "
                        "(`QuestionnaireUUID`, `Question`, `Answer`) "
                        "VALUES('%1', '%2', '%3')")
                    .arg(uuid.toString())
                    .arg(questionText)
                    .arg(answerText);

            DBQueryExecute(_db, queryText);
        }
    }
    catch (const SQLException& err)
    {
        emit errorOccured(EXIT_CODE::SQL_NOT_CONNECT, err.what());

        return;
    }
}

QString Questionnaire::getAllResults(const QDateTime& startDateTime, const QDateTime& endDateTime)
{
    Q_ASSERT(_db.open());

    QtCSV::StringData result;

    QStringList title;
    title << tr("Questionnare UUID") << tr("User name") << tr("Start questionnare") << tr("Finish questionnare") << tr("Question") << tr("Answer");
    result.addRow(title);

    try
    {
        transactionDB(_db);

        QSqlQuery query(_db);
        query.setForwardOnly(true);

        const auto queryText =
            QString("SELECT `UUID`, `TelegramID`, `UserName`, `FirstName`, `LastName`, `StartDateTime`, `FinishDateTime`, `QuestionText`, `AnswerText` "
                    "FROM `ResultQuestionnaire` AS A "
                    "INNER JOIN ( "
                        "SELECT `TelegramID`, `UserName`, `FirstName`, `LastName` "
                        "FROM `Users` AS B "
                    ") AS C "
                    "ON C.`TelegramID` = A.`UserID` "
                    "INNER JOIN ( "
                        "SELECT `QuestionnaireUUID`, `Question` AS `QuestionText`, `Answer` AS `AnswerText` "
                        "FROM `ResultQuestion` AS D "
                    ") AS E "
                    "ON E.`QuestionnaireUUID` = A.`UUID` "
                    "WHERE `StartDateTime` >= CAST('%1' AS DATETIME) AND `FinishDateTime` <=  CAST('%2' AS DATETIME) "
                    "ORDER BY `FinishDateTime`, `UUID`")
                .arg(startDateTime.toString(DATETIME_FORMAT))
                .arg(endDateTime.toString(DATETIME_FORMAT));

        DBQueryExecute(_db, query, queryText);

        while (query.next())
        {
            auto userName = query.value("UserName").toString();
            if (userName.isEmpty())
            {
                userName = query.value("FirstName").toString();
            }
            if (userName.isEmpty())
            {
                userName = query.value("LastName").toString();
            }
            if (userName.isEmpty())
            {
                userName = query.value("TelegramID").toString();
            }

            QStringList data;

            data << query.value("UUID").toString();
            data << userName;
            data << query.value("StartDateTime").toDateTime().toString(SIMPLY_DATETIME_FORMAT);
            data << query.value("FinishDateTime").toDateTime().toString(SIMPLY_DATETIME_FORMAT);
            data << query.value("QuestionText").toString();
            data << query.value("AnswerText").toString();

            result.addRow(data);
        }

        commitDB(_db);
    }
    catch (const SQLException& err)
    {
        _db.rollback();

        emit errorOccured(EXIT_CODE::SQL_NOT_CONNECT, err.what());

        return {};
    }

    QString resultString;

    QtCSV::Writer::write(resultString, result, "\t");

    return resultString;
}


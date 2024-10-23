#include "filedownloader.h"

using namespace Common;

FileDownloader::FileDownloader(QObject* parent /* = nullptr */)
    : QObject{parent}
{
}

FileDownloader::~FileDownloader()
{
    delete _query;
}

void FileDownloader::download(qint64 chatId, qint64 userId, const QUrl &url)
{
    Q_ASSERT(chatId != 0);
    Q_ASSERT(userId != 0);
    Q_ASSERT(url.isValid());

    makeQuery();

    Q_CHECK_PTR(_query);

    const auto id =  _query->send(url, HTTPSSLQuery::RequestType::GET, {}, {});

    DownloadInfo info;
    info.url = url;
    info.id = id;
    info.userId = userId;
    info.chatId = chatId;

    emit sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("Start download file ID %1: %2").arg(id).arg(url.toString()));

    _downloads.emplace(std::move(id), std::move(info));
}

void FileDownloader::getAnswerHTTP(const QByteArray &answer, quint64 id)
{
    const auto downloads_it = _downloads.find(id);
    if (downloads_it == _downloads.end())
    {
        Q_ASSERT(false);
    }

    const auto& info = downloads_it->second;

    emit downloadComplite(info.chatId, info.userId, answer);

    emit sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("ID %1 is complite successfully").arg(id));

    _downloads.erase(downloads_it);
}

void FileDownloader::errorOccurredHTTP(QNetworkReply::NetworkError code, quint64 serverCode, const QString &msg, quint64 id)
{
    const auto downloads_it = _downloads.find(id);
    if (downloads_it == _downloads.end())
    {
        Q_ASSERT(false);
    }

    const auto& info = downloads_it->second;

    emit downloadError(info.chatId, info.userId);

    emit sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("ID %1 complite with error: %2").arg(id).arg(msg));

    _downloads.erase(downloads_it);
}

void FileDownloader::sendLogMsgHTTP(Common::TDBLoger::MSG_CODE category, const QString &msg, quint64 id)
{
    emit sendLogMsg(category, QString("ID %1: %2").arg(id).arg(msg));
}

void FileDownloader::makeQuery()
{
    if (!_query)
    {
        _query = new HTTPSSLQuery();

        QObject::connect(_query, SIGNAL(getAnswer(const QByteArray&, quint64)),
                         SLOT(getAnswerHTTP(const QByteArray&, quint64)));
        QObject::connect(_query, SIGNAL(errorOccurred(QNetworkReply::NetworkError, quint64, const QString&, quint64)),
                         SLOT(errorOccurredHTTP(QNetworkReply::NetworkError, quint64, const QString&, quint64)));
        QObject::connect(_query, SIGNAL(sendLogMsg(Common::TDBLoger::MSG_CODE, const QString&, quint64)),
                         SLOT(sendLogMsgHTTP(Common::TDBLoger::MSG_CODE, const QString&, quint64)));
    }
}

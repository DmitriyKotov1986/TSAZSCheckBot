#pragma once

//STL
#include <unordered_map>

//Qt
#include <QObject>
#include <QUrl>

//My
#include <Common/httpsslquery.h>
#include <Common/tdbloger.h>

class FileDownloader
    : public QObject
{
    Q_OBJECT

public:
    FileDownloader(QObject* parent = nullptr);
    ~FileDownloader();

    void download(qint64 chatId, qint64 userId, const QUrl& url);

signals:
    void downloadComplite(qint64 chatId, qint64 userId, const QByteArray& data);
    void downloadError(qint64 chatId, qint64 userId);
    void sendLogMsg(Common::TDBLoger::MSG_CODE category, const QString& msg);

private slots:
    void getAnswerHTTP(const QByteArray& answer, quint64 id);
    void errorOccurredHTTP(QNetworkReply::NetworkError code, quint64 serverCode, const QString& msg, quint64 id);
    void sendLogMsgHTTP(Common::TDBLoger::MSG_CODE category, const QString& msg, quint64 id);

private:
    Q_DISABLE_COPY_MOVE(FileDownloader);
    void makeQuery();

private:
    struct DownloadInfo
    {
        QUrl url;
        qint64 chatId = 0;
        qint64 userId = 0;
        quint64 id = 0;
    };

private:
    Common::HTTPSSLQuery* _query = nullptr;

    std::unordered_map<quint64, DownloadInfo> _downloads;
};

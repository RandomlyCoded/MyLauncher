#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include <QNetworkAccessManager>
#include <QObject>

namespace randomly {

struct DownloadInfo
{
    QString url;
    QString path;
    qsizetype size;
    QString sha1;
    bool native = false;
};

class Downloader : public QObject
{
    Q_OBJECT
public:
    explicit Downloader(QObject *parent = nullptr);

    void download(const DownloadInfo &info);

    QList<DownloadInfo> queuedDownloads() { return m_downloads.values(); }

signals:
    void downloadCompleted(int downloadsRemaining);

private:
    void confirmDownload(QNetworkReply *reply);
    void extractNative(const DownloadInfo &info);

    QNetworkAccessManager *m_ctrl;

    QHash<QString, DownloadInfo> m_downloads;
};

} // namespace randomly

#endif // DOWNLOADER_H

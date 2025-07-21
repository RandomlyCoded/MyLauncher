#include "downloader.h"

#include <QDir>
#include <QFile>
#include <QLoggingCategory>
#include <QNetworkReply>
#include <qjsonobject.h>

namespace randomly {

Q_LOGGING_CATEGORY(lcDownload, "randomly.MyLauncher.Download")

Downloader::Downloader(QObject *parent)
    : QObject{parent}
    , m_ctrl(new QNetworkAccessManager(this))
{
    connect(m_ctrl, &QNetworkAccessManager::finished, this, &Downloader::confirmDownload);
}

void Downloader::download(const DownloadInfo &info)
{
    qCInfo(lcDownload) << "downloading" << info.url << "to" << info.path;
    m_ctrl->get(QNetworkRequest(info.url));
    m_downloads.insert(info.url, info);
}

void Downloader::confirmDownload(QNetworkReply *reply)
{
    const auto info = m_downloads[reply->url().toString()];

    qCInfo(lcDownload) << "recieved reply for" << info.url;

    if (info.size != reply->size()) {
        qCWarning(lcDownload, "failed to download %ls: size doesn't match (%lli vs %lli)", qUtf16Printable(info.url), info.size, reply->size());
        return;
    }

    const auto data = reply->readAll();

    QCryptographicHash sha1(QCryptographicHash::Sha1);
    sha1.addData(data);

    const auto hashResult = sha1.result().toHex();

    if (hashResult != info.sha1.toLocal8Bit()) {
        qCWarning(lcDownload, "failed to download %ls: hash doesn't match", qUtf16Printable(info.url));
        return;
    }

    // create full path
    QDir path = info.path;

    // ok this might be stupid, but idk another way around QDir's cd and cdUp limitations
    auto parent = path.filesystemAbsolutePath().parent_path();
    QDir(parent).mkpath(".");

    QFile output(info.path);

    if (!output.open(QFile::WriteOnly)) {
        qCWarning(lcDownload, "failed to open %ls: %ls", qUtf16Printable(info.path), qUtf16Printable(output.errorString()));
        return;
    }

    m_downloads.remove(info.url);

    output.write(data);
}

} // namespace randomly

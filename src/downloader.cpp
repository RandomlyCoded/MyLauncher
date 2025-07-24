#include "downloader.h"

#include "config.h"

#include <QDir>
#include <QFile>
#include <QLoggingCategory>
#include <QNetworkReply>
#include <QJsonObject>

#include <archive.h>
#include <archive_entry.h>

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
    auto req = QNetworkRequest(info.url);

    req.setRawHeader("Cache-Control", "no-cache");

    if (info.size == 0)
        qCWarning(lcDownload) << "requesting 0 B file: " << info.url;

    if (info.sha1 == "")
        qCWarning(lcDownload) << "requesting file without hash: " << info.url;

    m_ctrl->get(req);
    m_downloads.insert(info.url, info);
}

void Downloader::confirmDownload(QNetworkReply *reply)
{
    const auto info = m_downloads[reply->url().toString()];
    if (info.url == "")
        return; // already downloaded/duplicate

    qCInfo(lcDownload) << "recieved reply for" << info.url;

    if (info.size != reply->size() && info.size != 0) {
        qInfo().noquote() << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute)
                          << reply->size()
                          << reply->header(QNetworkRequest::ContentLengthHeader)
                          << reply->rawHeaderPairs();
        qCCritical(lcDownload, "failed to download %ls: size doesn't match (actual: %lli expected: %lli)", qUtf16Printable(info.url), reply->size(), info.size);
        return;
    }

    const auto data = reply->readAll();

    // can't compare hashes if none is provided
    if (info.sha1 != "") {
        QCryptographicHash sha1(QCryptographicHash::Sha1);
        sha1.addData(data);

        const auto hashResult = sha1.result().toHex();

        if (hashResult != info.sha1.toLocal8Bit()) {
            qCWarning(lcDownload, "failed to download %ls: hash doesn't match", qUtf16Printable(info.url));
            return;
        }
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

    output.close();

    if (info.native)
        extractNative(info);

    emit downloadCompleted(m_downloads.size());
}

void Downloader::extractNative(const DownloadInfo &info)
{
    qInfo() << "extracting native" << info.path;

    archive_entry *entry;
    archive *a = archive_read_new();
    archive *aw = archive_write_disk_new();
    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);

    int r = archive_read_open_filename(a, info.path.toUtf8().data(), 16384);
    if (r != ARCHIVE_OK) {
        qCInfo(lcDownload) << "unable to read archive" << info.path;
        return;
    }

    const auto targetDir = QDir(Config::instance()->getConfig("mcRoot").toString() + "/bin/" + Config::instance()->getTemp("version_name").toString() + "/");

    while (true) {
        r = archive_read_next_header(a, &entry);

        if (r == ARCHIVE_EOF)
            break;
        if (r != ARCHIVE_OK)
            qCWarning(lcDownload) << "archive_read_next_header() failed: " << archive_error_string(a);

        auto dest = targetDir.absoluteFilePath(archive_entry_pathname(entry));

        if (dest.endsWith('/')) {// directory
            targetDir.mkpath(dest);
            continue;
        }

        qCInfo(lcDownload) << "inflating" << dest;

        archive_entry_set_pathname(entry, dest.toLocal8Bit().data());
        r = archive_write_header(aw, entry);

        if (r != ARCHIVE_OK)
            qCWarning(lcDownload) << "archive_write_header(): " << archive_error_string(aw);

        while(true) {
            const char *buff;
            size_t size;
            off_t offset;
            r = archive_read_data_block(a, (const void**)&buff, &size, &offset);
            if (r == ARCHIVE_EOF)
                break;

            qInfo() << size << offset;

            r = archive_write_data_block(aw, buff, size, offset);

            if (r != ARCHIVE_OK) {
                qCWarning(lcDownload) << "archive_write_data_block(): " << archive_error_string(aw);
                break;
            }
        }
    }
}

} // namespace randomly

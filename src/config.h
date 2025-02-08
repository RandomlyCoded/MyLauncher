#ifndef CONFIG_H
#define CONFIG_H

#include <QObject>
#include <QPointer>
#include <QSettings>
#include <QVariant>

namespace randomly {

class Config : public QObject
{
    Q_OBJECT
public:
    explicit Config(QObject *parent = nullptr);

    static QPointer<Config> instance();

    QVariant getConfig(QString name);
    void setConfig(QString name, QVariant value);

signals:

private:
    QSettings m_settings;
};

} // namespace randomly

#endif // CONFIG_H

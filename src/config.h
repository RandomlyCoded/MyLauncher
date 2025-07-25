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

    ~Config();

    static QPointer<Config> instance();

    QVariant getConfig(QString name);
    void setConfig(QString name, QVariant value);

    QVariant getTemp(QString name);
    void setTemp(QString name, QVariant value);

    QVariant getAny(QString name);

    void saveTemp(QString name);
    void loadConfigAsTemp(QString name);

    const QSettings &settings() const { return m_settings; }
    const QHash<QString, QVariant> &temp() const { return m_temp; }

private:
    QSettings m_settings;
    QHash <QString, QVariant> m_temp;
};

} // namespace randomly

#endif // CONFIG_H

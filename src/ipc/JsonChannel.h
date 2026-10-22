#pragma once
#include <QObject>
#include <QJsonObject>
#include <QLocalSocket>

namespace ShireiaIpc {
inline constexpr int Version = 1;
inline constexpr int MaxFrame = 65536;
bool validId(const QJsonValue& value, bool allowZero = false);
bool validEnvelope(const QJsonObject& message, const QString& session);
QJsonObject envelope(const QString& type, int id, const QString& session);
class JsonChannel final : public QObject {
    Q_OBJECT
public:
    explicit JsonChannel(QLocalSocket* socket, QObject* parent = nullptr);
    bool send(const QJsonObject& message);
signals:
    void message(const QJsonObject& message);
    void invalid(const QString& reason);
private:
    QLocalSocket* socket_;
    QByteArray buffer_;
    bool failed_ = false;
    void read();
    void reject(const QString& reason);
};
}

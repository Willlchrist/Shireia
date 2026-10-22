#include "JsonChannel.h"
#include <QJsonDocument>
#include <QTimer>
#include <cmath>

namespace ShireiaIpc {
bool validId(const QJsonValue& value, bool allowZero) {
    if (!value.isDouble()) return false;
    const auto number = value.toDouble();
    return std::isfinite(number) && number == std::floor(number) && number >= (allowZero ? 0 : 1) && number <= 2147483647;
}
bool validEnvelope(const QJsonObject& message, const QString& session) {
    return message["v"].isDouble() && message["v"].toDouble() == Version
        && message["session"].isString() && message["session"].toString() == session
        && message["type"].isString() && validId(message["id"], true);
}
QJsonObject envelope(const QString& type, int id, const QString& session) {
    return {{"v", Version}, {"session", session}, {"type", type}, {"id", id}};
}
JsonChannel::JsonChannel(QLocalSocket* socket, QObject* parent) : QObject(parent), socket_(socket) {
    socket_->setReadBufferSize(MaxFrame * 2);
    connect(socket_, &QLocalSocket::readyRead, this, &JsonChannel::read);
    QTimer::singleShot(0, this, &JsonChannel::read);
}
void JsonChannel::reject(const QString& reason) {
    if (failed_) return;
    failed_ = true;
    emit invalid(reason);
    socket_->abort();
}
bool JsonChannel::send(const QJsonObject& message) {
    auto bytes = QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n';
    if (failed_ || bytes.size() > MaxFrame || socket_->state() != QLocalSocket::ConnectedState || socket_->bytesToWrite() > MaxFrame * 4) return false;
    return socket_->write(bytes) == bytes.size();
}
void JsonChannel::read() {
    if (failed_) return;
    buffer_ += socket_->readAll();
    if (buffer_.size() > MaxFrame * 2) { reject("IPC buffer limit"); return; }
    while (true) {
        const auto end = buffer_.indexOf('\n');
        if (end < 0) { if (buffer_.size() >= MaxFrame) reject("IPC frame limit"); return; }
        if (end <= 0 || end >= MaxFrame) { reject("Invalid IPC frame length"); return; }
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(buffer_.first(end), &error);
        buffer_.remove(0, end + 1);
        if (error.error != QJsonParseError::NoError || !document.isObject()) { reject("Invalid IPC JSON object"); return; }
        emit message(document.object());
        if (failed_ || socket_->state() != QLocalSocket::ConnectedState) return;
    }
}
}

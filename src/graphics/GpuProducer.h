#pragma once
#include <QJsonObject>
#include <QString>
#include <memory>

// Shared D3D11 transport: explicit diagnostic pattern or real headless frame upload.
class GpuProducer final {
public:
    GpuProducer();
    ~GpuProducer();
    QJsonObject start(const QString& session, bool emulation = false, int width = 1280, int height = 720);
    QJsonObject tick(const QByteArray& pixels = {});
    void stop();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

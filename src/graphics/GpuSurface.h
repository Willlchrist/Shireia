#pragma once
#include <QQuickItem>
#include <QImage>
#include <QJsonObject>
#include <QMutex>
#include <QPointer>
#include <array>
class EmulationManager;

class GpuSurface : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(QObject* manager READ manager WRITE setManager NOTIFY managerChanged)
    Q_PROPERTY(int framesPresented READ framesPresented NOTIFY framesPresentedChanged)
    Q_PROPERTY(int framesSkipped READ framesSkipped NOTIFY framesSkippedChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
public:
    explicit GpuSurface(QQuickItem* parent = nullptr);
    QObject* manager() const;
    void setManager(QObject* object);
    int framesPresented() const { return frames_; }
    int framesSkipped() const { return framesSkipped_; }
    QString error() const { return error_; }
    Q_INVOKABLE QImage grabPresentedImage() const;
signals:
    void managerChanged();
    void framesPresentedChanged();
    void framesSkippedChanged();
    void errorChanged();
protected:
    QSGNode* updatePaintNode(QSGNode* old, UpdatePaintNodeData*) override;
private:
    QPointer<EmulationManager> manager_;
    QJsonObject descriptor_;
    std::array<int,3> pending_{};
    int frames_ = 0;
    int framesSkipped_ = 0;
    QString error_;
    void setError(const QString& error);
};

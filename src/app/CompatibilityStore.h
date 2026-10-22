#pragma once

#include <QObject>
#include <functional>
#include <QVariantList>
#include <QVariantMap>

class CompatibilityStore final : public QObject {
    Q_OBJECT
public:
    explicit CompatibilityStore(QObject* parent = nullptr);

    Q_INVOKABLE QVariantMap record(const QString& titleId) const;
    Q_INVOKABLE QVariantList history(const QString& titleId) const;
    Q_INVOKABLE QVariantList crashHistory() const;
    Q_INVOKABLE void markResolutionRequested(const QString& sessionId, bool accepted);
signals:
    void crashHistoryChanged();

public:
    Q_INVOKABLE QString statusLabel(const QString& titleId) const;
    Q_INVOKABLE bool setAssessment(const QString& titleId, const QString& assessment, const QString& notes);
    Q_INVOKABLE bool beginSession(const QVariantMap& game, const QVariantMap& configuration);
    Q_INVOKABLE void updateSessionState(const QString& state);
    Q_INVOKABLE void recordSessionError(const QString& category, const QString& error);
    Q_INVOKABLE void finishSession(const QString& outcome, const QString& detail = {});

private:
    QString activeTitleId_;
    QString activeSessionId_;

    static QString normalizedTitleId(const QString& titleId);
    static QString prefix(const QString& titleId);
    QVariantList readHistory(const QString& titleId) const;
    void writeHistory(const QString& titleId, const QVariantList& history);
    void updateActiveSession(const std::function<void(QVariantMap&)>& update);
};

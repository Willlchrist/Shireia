#include "CompatibilityStore.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QRegularExpression>
#include <QSettings>
#include <QUuid>
#include <algorithm>

namespace {
const QStringList assessments{
    QStringLiteral("untested"), QStringLiteral("does-not-start"), QStringLiteral("major-issues"),
    QStringLiteral("playable-issues"), QStringLiteral("playable-no-known-issues"), QStringLiteral("extensively-tested")
};
const QMap<QString, QString> labels{
    {"untested", QStringLiteral("Não testado")},
    {"does-not-start", QStringLiteral("Não inicia")},
    {"major-issues", QStringLiteral("Problemas graves")},
    {"playable-issues", QStringLiteral("Jogável com problemas")},
    {"playable-no-known-issues", QStringLiteral("Jogável sem problemas conhecidos")},
    {"extensively-tested", QStringLiteral("Testado extensivamente")}
};

QString redact(const QString& input) {
    static const QRegularExpression paths(QStringLiteral("(?:[A-Za-z]:[\\\\/][^\\r\\n\\\"']*|\\\\\\\\[^\\\\]+\\\\[^\\r\\n\\\"']*)"));
    static const QRegularExpression tokens(QStringLiteral("\\b[A-Fa-f0-9]{48,}\\b"));
    return QString(input).trimmed().left(2048).replace(paths, QStringLiteral("[caminho ocultado]"))
        .replace(tokens, QStringLiteral("[token ocultado]"));
}

QStringList configKeys() {
    return {"graphics/resolution", "graphics/docked", "graphics/vsync", "graphics/gpuAccuracy",
        "graphics/asynchronousShaders", "graphics/asynchronousGpu", "graphics/pipelineCache",
        "graphics/antiAliasing", "core/multicore", "core/cpuAccuracy", "core/speedLimit"};
}
}

CompatibilityStore::CompatibilityStore(QObject* parent) : QObject(parent) {
    QSettings settings;
    const auto orphanTitleId = normalizedTitleId(settings.value("compatibility/active/titleId").toString());
    const auto orphanSessionId = settings.value("compatibility/active/sessionId").toString();
    if (orphanTitleId.isEmpty() || orphanSessionId.isEmpty()) return;

    auto history = readHistory(orphanTitleId);
    for (auto& value : history) {
        auto entry = value.toMap();
        if (entry.value("sessionId").toString() != orphanSessionId || entry.value("endedUtc").toString().size()) continue;
        entry["endedUtc"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
        entry["outcome"] = QStringLiteral("interrupted-or-app-closed");
        entry["detail"] = QStringLiteral("O aplicativo foi fechado antes do fim da sessão.");
        value = entry;
        break;
    }
    writeHistory(orphanTitleId, history);
    emit crashHistoryChanged();
    settings.remove("compatibility/active");
}

QString CompatibilityStore::normalizedTitleId(const QString& titleId) {
    const auto id = titleId.trimmed().toUpper();
    static const QRegularExpression valid(QStringLiteral("^[0-9A-F]{16}$"));
    return valid.match(id).hasMatch() && id.toULongLong(nullptr, 16) != 0 ? id : QString{};
}

QString CompatibilityStore::prefix(const QString& titleId) {
    return QStringLiteral("compatibility/games/") + titleId;
}

QVariantList CompatibilityStore::readHistory(const QString& titleId) const {
    return QSettings().value(prefix(titleId) + QStringLiteral("/history")).toList();
}

void CompatibilityStore::writeHistory(const QString& titleId, const QVariantList& history) {
    QSettings settings;
    settings.setValue(prefix(titleId) + QStringLiteral("/history"), history);
    settings.sync();
}

QVariantMap CompatibilityStore::record(const QString& titleId) const {
    const auto id = normalizedTitleId(titleId);
    if (id.isEmpty()) return {};
    QSettings settings;
    const auto root = prefix(id);
    return {{"titleId", id},
        {"title", settings.value(root + QStringLiteral("/title")).toString()},
        {"assessment", settings.value(root + QStringLiteral("/assessment"), QStringLiteral("untested")).toString()},
        {"notes", settings.value(root + QStringLiteral("/notes")).toString()},
        {"gameVersion", settings.value(root + QStringLiteral("/gameVersion"), QStringLiteral("Não informado pelo leitor")).toString()},
        {"emulatorVersion", settings.value(root + QStringLiteral("/emulatorVersion"), QCoreApplication::applicationVersion()).toString()},
        {"historyCount", readHistory(id).size()}};
}

QVariantList CompatibilityStore::history(const QString& titleId) const {
    const auto id = normalizedTitleId(titleId);
    return id.isEmpty() ? QVariantList{} : readHistory(id);
}

QVariantList CompatibilityStore::crashHistory() const {
    QSettings settings;
    QMap<QString, QVariantMap> byGame;
    settings.beginGroup(QStringLiteral("compatibility/games"));
    for (const auto& titleId : settings.childGroups()) {
        const auto id = normalizedTitleId(titleId);
        if (id.isEmpty()) continue;
        for (const auto& value : readHistory(id)) {
            auto entry = value.toMap();
            const auto outcome = entry.value("outcome").toString();
            const bool isCrash = outcome == QStringLiteral("core-failure")
                || outcome == QStringLiteral("interrupted-or-app-closed");
            if (!isCrash) continue;
            entry["resolutionRequested"] = entry.value("resolutionRequested", false).toBool();
            entry["resolutionDeclined"] = entry.value("resolutionDeclined", false).toBool();
            auto game = byGame.value(id);
            game["titleId"] = id;
            game["title"] = entry.value("title");
            auto entries = game.value("crashes").toList();
            entries.append(entry);
            std::sort(entries.begin(), entries.end(), [](const QVariant& left, const QVariant& right) {
                return left.toMap().value("startedUtc").toString() > right.toMap().value("startedUtc").toString();
            });
            game["crashes"] = entries;
            game["latestUtc"] = entries.first().toMap().value("startedUtc");
            byGame.insert(id, game);
        }
    }
    settings.endGroup();
    QVariantList games;
    for (const auto& game : byGame) games.append(game);
    std::sort(games.begin(), games.end(), [](const QVariant& left, const QVariant& right) {
        return left.toMap().value("latestUtc").toString() > right.toMap().value("latestUtc").toString();
    });
    return games;
}

void CompatibilityStore::markResolutionRequested(const QString& sessionId, bool accepted) {
    if (sessionId.trimmed().isEmpty()) return;
    QSettings settings;
    settings.beginGroup(QStringLiteral("compatibility/games"));
    const auto ids = settings.childGroups();
    settings.endGroup();
    for (const auto& titleId : ids) {
        const auto id = normalizedTitleId(titleId);
        if (id.isEmpty()) continue;
        auto history = readHistory(id);
        bool changed = false;
        for (auto& value : history) {
            auto entry = value.toMap();
            if (entry.value("sessionId").toString() != sessionId) continue;
            entry["resolutionRequested"] = accepted;
            entry["resolutionDeclined"] = !accepted;
            value = entry;
            changed = true;
            break;
        }
        if (changed) { writeHistory(id, history); emit crashHistoryChanged(); return; }
    }
}

QString CompatibilityStore::statusLabel(const QString& titleId) const {
    const auto id = normalizedTitleId(titleId);
    if (id.isEmpty()) return QStringLiteral("Title ID indisponível");
    const auto assessment = QSettings().value(prefix(id) + QStringLiteral("/assessment"), QStringLiteral("untested")).toString();
    return labels.value(assessment, labels.value(QStringLiteral("untested")));
}

bool CompatibilityStore::setAssessment(const QString& titleId, const QString& assessment, const QString& notes) {
    const auto id = normalizedTitleId(titleId);
    if (id.isEmpty() || !assessments.contains(assessment) || notes.size() > 4000) return false;
    QSettings settings;
    const auto root = prefix(id);
    settings.setValue(root + QStringLiteral("/assessment"), assessment);
    settings.setValue(root + QStringLiteral("/notes"), notes.trimmed());
    settings.sync();
    return settings.status() == QSettings::NoError;
}

bool CompatibilityStore::beginSession(const QVariantMap& game, const QVariantMap& configuration) {
    const auto titleId = normalizedTitleId(game.value("titleId").toString());
    const auto gameName = game.value("title").toString().trimmed().left(256);
    if (titleId.isEmpty() || gameName.isEmpty()) return false;
    if (!activeSessionId_.isEmpty()) finishSession(QStringLiteral("superseded"), QStringLiteral("Uma nova sessão foi iniciada."));

    QVariantMap safeConfiguration;
    for (const auto& key : configKeys())
        if (configuration.contains(key)) safeConfiguration.insert(key, configuration.value(key));

    QSettings settings;
    const auto root = prefix(titleId);
    settings.setValue(root + QStringLiteral("/title"), gameName);
    settings.setValue(root + QStringLiteral("/gameVersion"),
        game.value("version").toString().trimmed().isEmpty() ? QStringLiteral("Não informado pelo leitor") : game.value("version").toString().left(64));
    settings.setValue(root + QStringLiteral("/emulatorVersion"), QCoreApplication::applicationVersion());

    auto history = readHistory(titleId);
    QVariantMap session{{"sessionId", QUuid::createUuid().toString(QUuid::WithoutBraces)},
        {"title", gameName}, {"titleId", titleId}, {"gameVersion", game.value("version").toString().trimmed().isEmpty() ? QStringLiteral("Não informado pelo leitor") : game.value("version").toString().left(64)},
        {"emulatorVersion", QCoreApplication::applicationVersion()},
        {"startedUtc", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {"endedUtc", QString{}}, {"durationSeconds", 0}, {"state", QStringLiteral("launch-requested")},
        {"sawFirstFrame", false}, {"outcome", QStringLiteral("launch-requested")}, {"detail", QString{}},
        {"configuration", safeConfiguration}, {"events", QVariantList{}}};
    history.append(session);
    while (history.size() > 100) history.removeFirst();
    writeHistory(titleId, history);
    settings.setValue("compatibility/active/titleId", titleId);
    settings.setValue("compatibility/active/sessionId", session.value("sessionId"));
    settings.setValue("compatibility/active/startedUtc", session.value("startedUtc"));
    settings.sync();
    activeTitleId_ = titleId;
    activeSessionId_ = session.value("sessionId").toString();
    return true;
}

void CompatibilityStore::updateActiveSession(const std::function<void(QVariantMap&)>& update) {
    if (activeTitleId_.isEmpty() || activeSessionId_.isEmpty()) return;
    auto history = readHistory(activeTitleId_);
    bool changed = false;
    for (auto& value : history) {
        auto entry = value.toMap();
        if (entry.value("sessionId").toString() != activeSessionId_) continue;
        update(entry);
        value = entry;
        changed = true;
        break;
    }
    if (changed) { writeHistory(activeTitleId_, history); emit crashHistoryChanged(); }
}

void CompatibilityStore::updateSessionState(const QString& state) {
    const auto safeState = state.left(64);
    if (safeState.isEmpty()) return;
    updateActiveSession([&](QVariantMap& entry) {
        entry["state"] = safeState;
        if (safeState == QStringLiteral("Running")) {
            entry["sawFirstFrame"] = true;
            entry["outcome"] = QStringLiteral("first-frame-received");
        }
        if (safeState == QStringLiteral("Paused")) entry["outcome"] = QStringLiteral("paused-after-frame");
    });
}

void CompatibilityStore::recordSessionError(const QString& category, const QString& error) {
    const auto safeCategory = category.trimmed().left(64);
    const auto safeError = redact(error);
    if (safeCategory.isEmpty() || safeError.isEmpty()) return;
    updateActiveSession([&](QVariantMap& entry) {
        auto events = entry.value("events").toList();
        events.append(QVariantMap{{"timeUtc", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
            {"type", safeCategory}, {"detail", safeError}});
        while (events.size() > 100) events.removeFirst();
        entry["events"] = events;
    });
}

void CompatibilityStore::finishSession(const QString& outcome, const QString& detail) {
    if (activeTitleId_.isEmpty() || activeSessionId_.isEmpty()) return;
    const auto completedAt = QDateTime::currentDateTimeUtc();
    const auto safeOutcome = outcome.trimmed().left(64);
    const auto safeDetail = redact(detail);
    updateActiveSession([&](QVariantMap& entry) {
        const auto started = QDateTime::fromString(entry.value("startedUtc").toString(), Qt::ISODateWithMs);
        entry["endedUtc"] = completedAt.toString(Qt::ISODateWithMs);
        entry["durationSeconds"] = started.isValid() ? qMax<qint64>(0, started.secsTo(completedAt)) : 0;
        entry["outcome"] = safeOutcome.isEmpty() ? QStringLiteral("ended") : safeOutcome;
        if (!safeDetail.isEmpty()) entry["detail"] = safeDetail;
        if (!entry.value("sawFirstFrame").toBool() && safeOutcome == QStringLiteral("stopped"))
            entry["outcome"] = QStringLiteral("stopped-before-first-frame");
    });
    QSettings settings;
    settings.remove("compatibility/active");
    settings.sync();
    activeTitleId_.clear();
    activeSessionId_.clear();
    emit crashHistoryChanged();
}

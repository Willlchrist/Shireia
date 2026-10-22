#include "SetupAssistant.h"

#include "AppModel.h"
#include "LibraryResources.h"
#include "../settings/SettingsStore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMetaObject>

namespace {
constexpr auto legalVersionKey = "setup/legalVersion";
constexpr auto currentLegalVersion = "shireia-legal-2";
}

SetupAssistant::SetupAssistant(SettingsStore& settings, LibraryResources& resources, AppModel& model,
                               bool enabled, QObject* parent)
    : QObject(parent), settings_(settings), resources_(resources), model_(model), enabled_(enabled) {
    completed_ = store_.value("setup/completed", false).toBool();
    step_ = qBound(0, store_.value("setup/step", 0).toInt(), 5);
    if (store_.value(legalVersionKey).toString() != QString::fromLatin1(currentLegalVersion)) {
        completed_ = false;
        step_ = 0;
        store_.setValue("setup/completed", false);
        store_.setValue("setup/step", step_);
        store_.sync();
    }
    const auto configured = settings_.values();
    const bool resourcesReadyToCheck = !configured.value("system/prodKeys").toString().isEmpty()
        && !configured.value("system/firmwareFolder").toString().isEmpty();
    const bool firmwareStepResumed = legalAccepted() && step_ >= 3 && resourcesReadyToCheck;
    resources_.setEnabled(enabled_ && (completed_ || firmwareStepResumed));
    connect(&resources_, &LibraryResources::statusChanged, this, &SetupAssistant::onResourceStateChanged);
    if (completed_) QMetaObject::invokeMethod(this, &SetupAssistant::onResourceStateChanged, Qt::QueuedConnection);
}

bool SetupAssistant::legalAccepted() const {
    return store_.value(legalVersionKey).toString() == QString::fromLatin1(currentLegalVersion)
        && !store_.value("setup/legalAcceptedAtUtc").toString().isEmpty();
}
bool SetupAssistant::keysSelected() const {
    const QFileInfo file(settings_.values().value("system/prodKeys").toString());
    return file.isFile() && file.isReadable() && file.fileName().compare("prod.keys", Qt::CaseInsensitive) == 0;
}
bool SetupAssistant::keysValidated() const { return resources_.keysLoaded(); }
bool SetupAssistant::firmwareValidated() const { return resources_.firmwareReady(); }
bool SetupAssistant::resourceCheckComplete() const { return resources_.validationComplete(); }
bool SetupAssistant::busy() const { return resources_.busy(); }
QString SetupAssistant::status() const { return resources_.status(); }
QString SetupAssistant::acceptedAt() const { return store_.value("setup/legalAcceptedAtUtc").toString(); }
QString SetupAssistant::firmwareResult() const { return resources_.firmwareResult(); }

void SetupAssistant::saveStep(int step) {
    step_ = qBound(0, step, 5);
    store_.setValue("setup/step", step_);
    store_.sync();
    emit changed();
}

void SetupAssistant::begin() { if (enabled_ && !completed_) saveStep(1); }

bool SetupAssistant::acceptLegal(bool accepted) {
    error_.clear();
    if (!accepted) { emit changed(); return false; }
    store_.setValue(legalVersionKey, QString::fromLatin1(currentLegalVersion));
    store_.setValue("setup/legalAcceptedAtUtc", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    store_.setValue("setup/completed", false);
    store_.sync();
    saveStep(2);
    emit resourceStateChanged();
    return true;
}

void SetupAssistant::back() {
    error_.clear();
    if (step_ == 1) saveStep(0);
    else if (step_ > 1 && step_ < 5) saveStep(step_ - 1);
    emit changed();
}

bool SetupAssistant::selectKeys(const QUrl& file) {
    error_.clear();
    if (!legalAccepted()) { error_ = tr("Confirme primeiro o aviso legal."); emit changed(); return false; }
    error_ = settings_.selectProdKeys(file);
    if (!error_.isEmpty()) { emit changed(); return false; }
    if (!settings_.apply()) { error_ = tr("Não foi possível salvar a referência local das chaves."); emit changed(); return false; }
    if (enabled_ && !settings_.values().value("system/firmwareFolder").toString().isEmpty()) resources_.refresh();
    emit changed();
    emit resourceStateChanged();
    return true;
}

bool SetupAssistant::selectFirmware(const QUrl& source) {
    error_.clear();
    if (busy() || !keysSelected()) { error_ = tr("Selecione primeiro um arquivo prod.keys legivel."); emit changed(); return false; }
    error_ = settings_.selectFirmware(source);
    if (!error_.isEmpty()) { emit changed(); return false; }
    if (!settings_.apply()) { error_ = tr("Não foi possível salvar a referência local do firmware."); emit changed(); return false; }
    resources_.setEnabled(enabled_);
    if (enabled_) resources_.refresh();
    emit resourceStateChanged();
    return true;
}

void SetupAssistant::retryResourceCheck() {
    error_.clear();
    if (!legalAccepted()) { error_ = tr("Confirme primeiro o aviso legal."); emit changed(); return; }
    if (!enabled_) { error_ = tr("A validação está indisponível neste modo de teste."); emit changed(); return; }
    resources_.refresh();
    emit resourceStateChanged();
}

QString SetupAssistant::licenseText(const QString& name) const {
    static const QHash<QString, QString> bundled{
        {QStringLiteral("Componentes incluídos"), QStringLiteral(":/third_party/NOTICES.md")},
        {QStringLiteral("Suyu — GNU GPL v3 ou posterior"), QStringLiteral(":/third_party/licenses/Suyu-LICENSE.txt")},
        {QStringLiteral("FFmpeg — GNU GPL v3 ou posterior"), QStringLiteral(":/third_party/licenses/GPL-3.0-or-later.txt")},
        {QStringLiteral("GNU GPL v2 ou posterior"), QStringLiteral(":/third_party/licenses/GPL-2.0-or-later.txt")},
        {QStringLiteral("GNU GPL v3 ou posterior"), QStringLiteral(":/third_party/licenses/GPL-3.0-or-later.txt")},
        {QStringLiteral("Qt — GNU LGPL v3"), QStringLiteral(":/third_party/licenses/LGPL-3.0-only.txt")},
        {QStringLiteral("Qt — exceção GPL"), QStringLiteral(":/third_party/licenses/Qt-GPL-exception-1.0.txt")},
        {QStringLiteral("GNU LGPL v2.1 ou posterior"), QStringLiteral(":/third_party/licenses/LGPL-2.1-or-later.txt")},
        {QStringLiteral("SDL 2 — licença zlib"), QStringLiteral(":/third_party/licenses/SDL2-zlib-LICENSE.txt")},
        {QStringLiteral("SDL 3 — licença zlib"), QStringLiteral(":/third_party/licenses/SDL3-zlib-LICENSE.txt")},
        {QStringLiteral("libarchive — licença BSD"), QStringLiteral(":/third_party/licenses/libarchive-LICENSE.txt")},
        {QStringLiteral("OpenSSL — Apache 2.0"), QStringLiteral(":/third_party/licenses/OpenSSL-LICENSE.txt")},
        {QStringLiteral("zlib — licença zlib"), QStringLiteral(":/third_party/licenses/zlib-LICENSE.txt")},
        {QStringLiteral("Boost — BSL 1.0"), QStringLiteral(":/third_party/licenses/BSL-1.0.txt")},
        {QStringLiteral("Dynarmic — 0BSD"), QStringLiteral(":/third_party/licenses/Dynarmic-LICENSE.txt")},
        {QStringLiteral("fmt — MIT"), QStringLiteral(":/third_party/licenses/fmt-MIT-LICENSE.txt")},
        {QStringLiteral("ENet — MIT"), QStringLiteral(":/third_party/licenses/ENet-LICENSE.txt")},
        {QStringLiteral("Sirit \u2014 GNU AGPL v3"), QStringLiteral(":/third_party/licenses/sirit-AGPL-3.0-LICENSE.txt")},
        {QStringLiteral("cubeb — ISC"), QStringLiteral(":/third_party/licenses/cubeb-LICENSE.txt")},
        {QStringLiteral("MPL 2.0 — decodificador BC"), QStringLiteral(":/third_party/licenses/MPL-2.0.txt")},
        {QStringLiteral("BSD 2 cláusulas — tz"), QStringLiteral(":/third_party/licenses/BSD-2-Clause.txt")},
        {QStringLiteral("Apache 2.0 com exceção LLVM"), QStringLiteral(":/third_party/licenses/Apache-2.0-LLVM-exception.txt")},
        {QStringLiteral("Creative Commons Atribuição 4.0 — dados SPIR-V"), QStringLiteral(":/third_party/licenses/CC-BY-4.0.txt")},
        {QStringLiteral("MIT — stb"), QStringLiteral(":/third_party/licenses/MIT.txt")},
        {QStringLiteral("Lagoon — MIT"), QStringLiteral(":/third_party/licenses/Lagoon-MIT-LICENSE.txt")}
    };
    const auto path = bundled.value(name);
    if (path.isEmpty()) return {};
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024) return tr("Não foi possível abrir este aviso de licença.");
    return QString::fromUtf8(file.readAll());
}

bool SetupAssistant::continueFromKeys() {
    error_.clear();
    if (!keysSelected()) { error_ = tr("Selecione primeiro um arquivo local prod.keys legivel."); emit changed(); return false; }
    saveStep(3);
    return true;
}

bool SetupAssistant::continueFromFirmware() {
    error_.clear();
    if (!firmwareValidated()) { error_ = resourceCheckComplete() ? tr("O núcleo não reconheceu firmware compatível.") : tr("Aguarde a validação do firmware pelo núcleo."); emit changed(); return false; }
    saveStep(4);
    return true;
}

bool SetupAssistant::addGamesFolder(const QUrl& folder) {
    error_.clear();
    const auto path = folder.isLocalFile() ? QFileInfo(folder.toLocalFile()).canonicalFilePath() : QString{};
    if (path.isEmpty() || !QFileInfo(path).isDir()) { error_ = tr("Selecione uma pasta local legível."); emit changed(); return false; }
    const auto before = model_.folders();
    model_.addFolder(QUrl::fromLocalFile(path));
    settings_.refresh();
    if (!model_.folders().contains(path)) { error_ = tr("Não foi possível adicionar a pasta selecionada."); emit changed(); return false; }
    store_.setValue("setup/libraryChoice", "configured");
    store_.sync();
    saveStep(5);
    if (before == model_.folders()) error_.clear();
    return true;
}

void SetupAssistant::skipGames() {
    error_.clear();
    store_.setValue("setup/libraryChoice", "skipped");
    store_.sync();
    saveStep(5);
}

bool SetupAssistant::finish() {
    error_.clear();
    if (!legalAccepted() || !keysValidated() || !firmwareValidated()) {
        error_ = tr("Para concluir, confirme o aviso e aguarde a validação real das chaves e do firmware pelo núcleo.");
        emit changed();
        return false;
    }
    if (store_.value("setup/libraryChoice").toString().isEmpty()) store_.setValue("setup/libraryChoice", "skipped");
    store_.setValue("setup/completed", true);
    store_.setValue("setup/step", 5);
    store_.setValue("setup/keysReference", settings_.values().value("system/prodKeys"));
    store_.setValue("setup/firmwareReference", settings_.values().value("system/firmwareFolder"));
    store_.sync();
    if (store_.status() != QSettings::NoError) {
        error_ = tr("Não foi possível salvar a conclusão da configuração inicial.");
        emit changed();
        return false;
    }
    completed_ = true;
    step_ = 5;
    emit changed();
    return true;
}

void SetupAssistant::onResourceStateChanged() {
    // Completion is a one-time onboarding decision. If resources later change
    // or fail revalidation, keep the wizard dismissed; users can repair them
    // from Settings without being sent through first-run setup again.
    emit resourceStateChanged();
}

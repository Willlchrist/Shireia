#include <QCoreApplication>
#include <QTemporaryDir>
#include <QTimer>
#include <QElapsedTimer>
#include <QThread>
#include <QFile>
#include <QDebug>
#include <QUuid>
#include <QProcessEnvironment>
#include <QJsonDocument>
#include <QDir>
#include <QtEndian>
#include "../src/emulation/EmulationManager.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <signal.h>
#endif

#define CHECK_CORE(condition) do { if (!(condition)) { qCritical() << "CORE FAIL" << __LINE__ << #condition; return 1; } } while (false)
template<typename Predicate> bool until(Predicate predicate, int timeout = 15000) {
    QElapsedTimer clock; clock.start();
    while (!predicate() && clock.elapsed() < timeout) { QCoreApplication::processEvents(QEventLoop::AllEvents, 20); QThread::msleep(5); }
    return predicate();
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir temp;
    const auto exe = QCoreApplication::applicationDirPath() + "/ShireiaCore.exe";
    EmulationManager manager(nullptr, exe, temp.path() + "/data");
    CHECK_CORE(manager.coreAvailable());
    int failures = 0;
    QObject::connect(&manager, &EmulationManager::failed, [&] { ++failures; });
    CHECK_CORE(QDir().mkpath(temp.path()+"/data"));
    QLockFile importLock(temp.path()+"/data/resources.lock");
    CHECK_CORE(importLock.tryLock(0));
    manager.startCore(); CHECK_CORE(!manager.active() && failures==0);
    importLock.unlock();
    manager.startCore(); manager.startCore();
    CHECK_CORE(until([&] { return manager.state() == "Ready" || manager.state() == "Failed"; }));
    if (manager.state() != "Ready") qCritical() << manager.message() << manager.log();
    CHECK_CORE(manager.state() == "Ready" && manager.processId() > 0);
    CHECK_CORE(!importLock.tryLock(0));
    CHECK_CORE(manager.capabilities()["initialize"].toBool());
    CHECK_CORE(manager.capabilities()["execute"].toBool() && manager.capabilities()["gameInput"].toBool() && !manager.capabilities()["nativeGpuFrames"].toBool());
    const auto pid = manager.processId();
    manager.queryState(); CHECK_CORE(until([&] { return manager.message().contains("confirmado"); }));
    QFile header(temp.path() + "/synthetic-header.nro");
    CHECK_CORE(header.open(QIODevice::WriteOnly));
    QByteArray data(128, '\0'); data.replace(0x10, 4, "NRO0"); header.write(data); header.close();
    QString format;
    QObject::connect(&manager, &EmulationManager::identified, [&](const QString& value) { format = value; });
    manager.identifyFile(header.fileName()); CHECK_CORE(until([&] { return !format.isEmpty(); }));
    CHECK_CORE(format == "NRO" && manager.state() == "Ready" && manager.processId() == pid);
    QFile invalid(temp.path() + "/invalid.nro"); CHECK_CORE(invalid.open(QIODevice::WriteOnly)); invalid.write("not a program"); invalid.close();
    format.clear(); manager.identifyFile(invalid.fileName()); CHECK_CORE(until([&] { return !format.isEmpty(); })); CHECK_CORE(format != "NRO");
    // A synthetic PFS0 with a nonfunctional NCA entry checks container routing only.
    // Recognition must never be reported as successful loading or execution.
    QFile container(temp.path() + "/synthetic-container.nsp");
    CHECK_CORE(container.open(QIODevice::WriteOnly));
    QByteArray pfs(16 + 24 + 10 + 0x400, '\0'); pfs.replace(0,4,"PFS0");
    qToLittleEndian<quint32>(1,pfs.data()+4);
    qToLittleEndian<quint32>(10,pfs.data()+8);
    qToLittleEndian<quint64>(0x400,pfs.data()+24);
    pfs.replace(40,9,"dummy.nca");
    CHECK_CORE(container.write(pfs)==pfs.size()); container.close();
    format.clear(); manager.identifyFile(container.fileName()); CHECK_CORE(until([&] { return !format.isEmpty(); }));
    CHECK_CORE(format=="NSP" && manager.state()=="Ready" && manager.processId()==pid && !manager.gameLoaded());
    manager.pauseGame(); CHECK_CORE(manager.state() == "Ready");
    manager.resumeGame(); CHECK_CORE(manager.state() == "Ready");
    manager.stopGame(); manager.stopGame();
    CHECK_CORE(until([&] { return !manager.active(); }));
    CHECK_CORE(manager.state() == "Stopped" && failures == 0);
    CHECK_CORE(importLock.tryLock(0)); importLock.unlock();
    CHECK_CORE(manager.exits().first().toMap()["expected"].toBool());
    manager.startCore(); CHECK_CORE(until([&] { return manager.state() == "Ready" || manager.state() == "Failed"; })); CHECK_CORE(manager.state() == "Ready");
    // Terminate only the child owned by this test, without a fake crash command.
#ifdef _WIN32
    const auto handle = OpenProcess(PROCESS_TERMINATE, FALSE, DWORD(manager.processId()));
    CHECK_CORE(handle); CHECK_CORE(TerminateProcess(handle, 37)); CloseHandle(handle);
#else
    CHECK_CORE(::kill(pid_t(manager.processId()), SIGKILL) == 0);
#endif
    CHECK_CORE(until([&] { return !manager.active() && manager.state() == "Failed"; }));
    CHECK_CORE(failures > 0 && !manager.exits().first().toMap()["expected"].toBool());
    manager.startCore(); CHECK_CORE(until([&] { return manager.state() == "Ready" || manager.state() == "Failed"; })); CHECK_CORE(manager.state() == "Ready");
    manager.stopGame(); CHECK_CORE(until([&] { return !manager.active(); })); CHECK_CORE(manager.state() == "Stopped");
    // Cancel a queued game while the core process is still handshaking. The
    // following session must not retain the stale pendingLoad_ state.
    manager.startCore();
    manager.startGame(invalid.fileName());
    manager.stopGame();
    CHECK_CORE(until([&] { return !manager.active(); })); CHECK_CORE(manager.state() == "Stopped");
    manager.startCore();
    CHECK_CORE(until([&] { return manager.state() == "Ready" || manager.state() == "Failed"; }));
    CHECK_CORE(manager.state() == "Ready");
    manager.startGame(invalid.fileName());
    CHECK_CORE(until([&] { return manager.state() == "Failed" || manager.state() == "Running"; }));
    CHECK_CORE(manager.state() == "Failed" && manager.message().contains("Invalid NRO image"));
    manager.startCore();
    CHECK_CORE(until([&] { return manager.state() == "Ready" || manager.state() == "Failed"; }));
    CHECK_CORE(manager.state() == "Ready");
    manager.stopGame(); CHECK_CORE(until([&] { return !manager.active(); })); CHECK_CORE(manager.state() == "Stopped");

    QLocalServer server;
    const auto session = QUuid::createUuid().toString(QUuid::WithoutBraces);
    CHECK_CORE(server.listen("shireia-test-" + session));
    QProcess child;
    auto env = QProcessEnvironment::systemEnvironment(); env.insert("SHIREIA_CORE_TOKEN", QString(64, 'a'));
    child.setProcessEnvironment(env);
    child.start(exe, {"--server", server.serverName(), "--shireia-session", session, "--data-dir", temp.path() + "/protocol"});
    CHECK_CORE(until([&] { return server.hasPendingConnections(); }));
    auto* socket = server.nextPendingConnection(); CHECK_CORE(socket);
    QByteArray received;
    CHECK_CORE(until([&] { received += socket->readAll(); return received.contains('\n'); }));
    CHECK_CORE(received.contains("hello") && received.contains("suyu-original-prebuilt"));
    CHECK_CORE(!ShireiaIpc::validId(QJsonValue("1")));
    CHECK_CORE(!ShireiaIpc::validId(QJsonValue(1.5)));
    CHECK_CORE(!ShireiaIpc::validId(QJsonValue(-1)));
    auto request = ShireiaIpc::envelope("request", 1, session); request["command"] = "status";
    const auto bytes = QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n';
    socket->write(bytes.first(10)); socket->flush(); QCoreApplication::processEvents();
    CHECK_CORE(child.state() == QProcess::Running);
    socket->write(bytes.sliced(10)); socket->flush(); received.clear();
    CHECK_CORE(until([&] { received += socket->readAll(); return received.contains('\n'); })); CHECK_CORE(received.contains("response"));
    request = ShireiaIpc::envelope("request", 2, session); request["command"] = "performance-capture-start";
    socket->write(QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n'); socket->flush(); received.clear();
    CHECK_CORE(until([&] { received += socket->readAll(); return received.contains('\n'); }));
    CHECK_CORE(QJsonDocument::fromJson(received.trimmed()).object()["ok"] == false);
    request = ShireiaIpc::envelope("request", 3, session); request["command"] = "pause";
    socket->write(QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n'); socket->flush(); received.clear();
    CHECK_CORE(until([&] { received += socket->readAll(); return received.contains('\n'); }));
    CHECK_CORE(QJsonDocument::fromJson(received.trimmed()).object()["ok"] == false);
    socket->write("[]\n"); socket->flush();
    CHECK_CORE(until([&] { return child.state() == QProcess::NotRunning; }));
    CHECK_CORE(child.exitCode() != 0);
    delete socket;

    auto rejects = [&](bool oversize) {
        QLocalServer negativeServer;
        const auto negativeSession = QUuid::createUuid().toString(QUuid::WithoutBraces);
        if (!negativeServer.listen("shireia-invalid-" + negativeSession)) return false;
        QProcess invalidChild; invalidChild.setProcessEnvironment(env);
        invalidChild.start(exe, {"--server", negativeServer.serverName(), "--shireia-session", negativeSession, "--data-dir", temp.path() + "/negative"});
        if (!until([&] { return negativeServer.hasPendingConnections(); })) return false;
        auto* peer = negativeServer.nextPendingConnection();
        QByteArray hello;
        if (!until([&] { hello += peer->readAll(); return hello.contains('\n'); })) return false;
        if (oversize) peer->write(QByteArray(ShireiaIpc::MaxFrame + 1, 'x'));
        else {
            auto wrong = ShireiaIpc::envelope("request", 1, negativeSession);
            wrong["v"] = 999; wrong["command"] = "initialize";
            peer->write(QJsonDocument(wrong).toJson(QJsonDocument::Compact) + '\n');
        }
        peer->flush();
        const auto rejected = until([&] { return invalidChild.state() == QProcess::NotRunning; }) && invalidChild.exitCode() != 0;
        delete peer;
        return rejected;
    };
    CHECK_CORE(rejects(false)); CHECK_CORE(rejects(true));
    EmulationManager missing(nullptr, temp.path() + "/missing.exe", temp.path());
    missing.startCore(); CHECK_CORE(missing.state() == "Unavailable" && !missing.active());
    EmulationManager rejected(nullptr, exe, temp.path()+"/rejected-data");
    rejected.startGame(invalid.fileName());
    CHECK_CORE(until([&] { return rejected.state()=="Failed" && !rejected.active(); }));
    CHECK_CORE(rejected.message().contains("Invalid NRO image") && !rejected.gameLoaded());
    qInfo() << "PASS: real core initialization, IPC, header identification (not loading), stop, crash isolation/restart, early stop, fragmented/malformed messages.";
    return 0;
}

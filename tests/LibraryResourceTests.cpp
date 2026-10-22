#include "../src/app/LibraryResources.h"
#include "../src/app/AppModel.h"
#include "../src/settings/SettingsStore.h"
#include "../src/emulation/EmulationManager.h"
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QImage>
#include <QBuffer>
#include <QtEndian>
#include <QElapsedTimer>
#include <QThread>
#include <QDebug>
#include <QUrl>

#define CHECK_RESOURCE(c) do { if (!(c)) { qCritical() << "RESOURCE FAIL" << __LINE__ << #c << resources.status(); return 1; } } while(false)
template<class Predicate> bool until(Predicate predicate) {
    QElapsedTimer timer; timer.start();
    while (!predicate() && timer.elapsed() < 20000) { QCoreApplication::processEvents(); QThread::msleep(10); }
    return predicate();
}
bool fixture(const QString& path, const QByteArray& title) {
    QImage image(32,32,QImage::Format_RGB32); image.fill(Qt::darkMagenta);
    QByteArray icon; QBuffer buffer(&icon); buffer.open(QIODevice::WriteOnly); image.save(&buffer,"PNG");
    QByteArray bytes(0x3000 + 0x38 + icon.size() + 0x4000, '\0');
    bytes.replace(0x10,4,"NRO0"); qToLittleEndian<quint32>(0x3000,bytes.data()+0x18);
    bytes.replace(0x3000,4,"ASET");
    qToLittleEndian<quint64>(0x38,bytes.data()+0x3008);
    qToLittleEndian<quint64>(icon.size(),bytes.data()+0x3010);
    qToLittleEndian<quint64>(0x38+icon.size(),bytes.data()+0x3018);
    qToLittleEndian<quint64>(0x4000,bytes.data()+0x3020);
    bytes.replace(0x3038,icon.size(),icon);
    for (int i=0;i<16;++i) bytes.replace(0x3038+icon.size()+0x300*i,title.size(),title);
    QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(bytes)==bytes.size();
}
int main(int argc,char** argv) {
    QGuiApplication app(argc,argv);
    app.setOrganizationName("ShireiaTests"); app.setApplicationName("Resources");
    QTemporaryDir temporary;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temporary.path());
    AppModel model; SettingsStore settings;
    const auto data = temporary.path()+"/data";
    EmulationManager session(nullptr,{},data);
    LibraryResources resources(model,settings,session,data);
    CHECK_RESOURCE(temporary.isValid());
    QDir().mkpath(temporary.path()+"/games");
    const auto path=temporary.path()+"/games/local-name.nro";
    CHECK_RESOURCE(fixture(path,"Authored metadata fixture"));
    model.addFolder(QUrl::fromLocalFile(temporary.path()+"/games")); settings.refresh();
    CHECK_RESOURCE(until([&] { return !model.games().isEmpty() && model.games().first().toMap()["title"]=="Authored metadata fixture"; }));
    CHECK_RESOURCE(until([&] { return !resources.busy(); }));
    CHECK_RESOURCE(!resources.profiles().isEmpty());
    const auto existingUser = resources.profiles().first().toMap().value("uuid").toString();
    CHECK_RESOURCE(!resources.selectProfile("00000000-0000-0000-0000-000000000000"));
    CHECK_RESOURCE(resources.selectProfile(existingUser));
    CHECK_RESOURCE(SettingsStore{}.values()["system/userUuid"].toString() == existingUser);
    const auto initialCount = resources.profiles().size();
    CHECK_RESOURCE(resources.createProfile("Shireia Test"));
    CHECK_RESOURCE(until([&] { return resources.profiles().size() == initialCount + 1; }));
    CHECK_RESOURCE(until([&] { return !resources.busy(); }));
    const auto createdUser = resources.profiles().last().toMap().value("uuid").toString();
    CHECK_RESOURCE(!createdUser.isEmpty() && createdUser != existingUser);
    CHECK_RESOURCE(resources.renameProfile(createdUser, "Renamed Test"));
    CHECK_RESOURCE(until([&] { for (const auto& item : resources.profiles()) if (item.toMap().value("uuid") == createdUser && item.toMap().value("name") == "Renamed Test") return true; return false; }));
    CHECK_RESOURCE(until([&] { return !resources.busy(); }));
    QImage avatar(48, 48, QImage::Format_RGB32); avatar.fill(Qt::magenta);
    const auto avatarSource = temporary.path() + "/avatar.png"; CHECK_RESOURCE(avatar.save(avatarSource));
    CHECK_RESOURCE(resources.setProfilePhoto(createdUser, QUrl::fromLocalFile(avatarSource)));
    CHECK_RESOURCE(until([&] { for (const auto& item : resources.profiles()) if (item.toMap().value("uuid") == createdUser && !item.toMap().value("photo").toString().isEmpty()) return true; return false; }));
    CHECK_RESOURCE(until([&] { return !resources.busy(); }));
    const auto avatarPath = resources.profiles().last().toMap().value("photo").toString();
    CHECK_RESOURCE(!QImage(avatarPath).isNull());
    CHECK_RESOURCE(SettingsStore{}.values()["system/userUuid"].toString() == existingUser);
    const auto cover=model.games().first().toMap()["cover"].toString();
    CHECK_RESOURCE(!QImage(QUrl(cover).toLocalFile()).isNull());
    AppModel restored;
    CHECK_RESOURCE(restored.games().first().toMap()["title"]=="Authored metadata fixture");
    QThread::msleep(20);
    CHECK_RESOURCE(fixture(path,"Updated authored metadata")); model.scan();
    CHECK_RESOURCE(until([&] { return model.games().first().toMap()["title"]=="Updated authored metadata"; }));
    QImage custom(16,16,QImage::Format_RGB32); custom.fill(Qt::blue);
    const auto customPath=temporary.path()+"/custom.png"; CHECK_RESOURCE(custom.save(customPath));
    model.setCover(path,QUrl::fromLocalFile(customPath)); resources.refresh();
    CHECK_RESOURCE(until([&] { return !resources.busy() && resources.status().startsWith("Metadados:"); }));
    CHECK_RESOURCE(model.games().first().toMap()["cover"]==QUrl::fromLocalFile(customPath).toString());
    // Invalid user-supplied resources must not overwrite an earlier installation.
    QDir().mkpath(data+"/keys");
    QFile oldKeys(data+"/keys/prod.keys"); CHECK_RESOURCE(oldKeys.open(QIODevice::WriteOnly)); oldKeys.write("non-key test sentinel"); oldKeys.close();
    QFile invalidKeys(temporary.path()+"/prod.keys"); CHECK_RESOURCE(invalidKeys.open(QIODevice::WriteOnly)); invalidKeys.write("invalid test placeholder, no keys"); invalidKeys.close();
    CHECK_RESOURCE(settings.selectProdKeys(QUrl::fromLocalFile(invalidKeys.fileName())).isEmpty()); CHECK_RESOURCE(settings.apply());
    CHECK_RESOURCE(until([&] { return !resources.busy() && resources.status().startsWith("Falha na análise:"); }));
    CHECK_RESOURCE(oldKeys.open(QIODevice::ReadOnly)); CHECK_RESOURCE(oldKeys.readAll()=="non-key test sentinel"); oldKeys.close();
    settings.setValue("system/prodKeys", "");
    QDir().mkpath(temporary.path()+"/firmware");
    QFile invalidNca(temporary.path()+"/firmware/test.nca"); CHECK_RESOURCE(invalidNca.open(QIODevice::WriteOnly)); invalidNca.write("not firmware"); invalidNca.close();
    QDir().mkpath(data+"/nand/system/Contents/registered");
    QFile previous(data+"/nand/system/Contents/registered/previous.txt"); CHECK_RESOURCE(previous.open(QIODevice::WriteOnly)); previous.write("previous test installation"); previous.close();
    CHECK_RESOURCE(settings.selectFirmware(QUrl::fromLocalFile(temporary.path()+"/firmware")).isEmpty()); CHECK_RESOURCE(settings.apply());
    CHECK_RESOURCE(until([&] { return !resources.busy() && resources.status().contains("Firmware não importado:"); }));
    CHECK_RESOURCE(previous.open(QIODevice::ReadOnly)); CHECK_RESOURCE(previous.readAll()=="previous test installation");
    CHECK_RESOURCE(!QFile::exists(data+"/nand/system/Contents/registered/test.nca"));
    qInfo() << "PASS: resource extraction, private account create/rename/photo with stable UUID and active user, invalid resources preserve installed data.";
    return 0;
}

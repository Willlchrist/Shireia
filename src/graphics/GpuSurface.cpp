#define NOMINMAX
#include "GpuSurface.h"
#include "FrameSize.h"
#include "../emulation/EmulationManager.h"
#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QSGTexture>
#include <QSGRendererInterface>
#include <QJsonArray>
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
namespace {
class TextureNode final : public QSGSimpleTextureNode {
public:
    ComPtr<ID3D11Texture2D> textures[3];
    ComPtr<IDXGIKeyedMutex> mutexes[3];
    QSGTexture* wrappers[3]{};
    QString generation;
    int held = -1;
    int heldSequence = 0;
    ~TextureNode() override {
        if (held >= 0) mutexes[held]->ReleaseSync(0);
        for (auto* wrapper : wrappers) delete wrapper;
    }
};
}
GpuSurface::GpuSurface(QQuickItem* parent) : QQuickItem(parent) { setFlag(ItemHasContents, true); }
QImage GpuSurface::grabPresentedImage() const {
    if (!window() || frames_ < 1 || !error_.isEmpty()) return {};
    const auto image = window()->grabWindow();
    if (image.isNull() || window()->width() <= 0) return {};
    const qreal scale = qreal(image.width()) / window()->width();
    const auto origin = mapToScene(QPointF(0, 0));
    const QRect bounds(qRound(origin.x() * scale), qRound(origin.y() * scale),
                       qRound(width() * scale), qRound(height() * scale));
    const auto crop = bounds.intersected(image.rect());
    return crop.isEmpty() ? QImage{} : image.copy(crop);
}
QObject* GpuSurface::manager() const { return manager_; }
void GpuSurface::setManager(QObject* object) {
    if (manager_) disconnect(manager_, nullptr, this, nullptr);
    manager_ = qobject_cast<EmulationManager*>(object);
    if (manager_) {
        auto refresh = [this] { descriptor_ = manager_->gpuDescriptor(); pending_ = {}; error_.clear(); emit errorChanged(); update(); };
        connect(manager_, &EmulationManager::gpuDescriptorChanged, this, refresh);
        connect(manager_, &EmulationManager::gpuFrame, this, [this](int slot, int sequence) { if (slot >= 0 && slot < 3) { pending_[slot] = sequence; update(); } });
        refresh();
    }
    emit managerChanged();
}
void GpuSurface::setError(const QString& message) {
    QMetaObject::invokeMethod(this, [this,message] { if (error_ != message) { error_ = message; emit errorChanged(); } }, Qt::QueuedConnection);
}
QSGNode* GpuSurface::updatePaintNode(QSGNode* old, UpdatePaintNodeData*) {
    auto* node = static_cast<TextureNode*>(old);
    if (descriptor_.isEmpty()) { delete node; return nullptr; }
    if (!window() || window()->rendererInterface()->graphicsApi() != QSGRendererInterface::Direct3D11) {
        setError(tr("O teste de texturas requer Qt Quick com Direct3D 11.")); delete node; return nullptr;
    }
    const auto generation = descriptor_["generation"].toString();
    if (node && node->generation != generation) { delete node; node = nullptr; }
    if (!node) {
        auto* device = static_cast<ID3D11Device*>(window()->rendererInterface()->getResource(window(), QSGRendererInterface::DeviceResource));
        if (!device) return nullptr;
        ComPtr<IDXGIDevice> dxgi; ComPtr<IDXGIAdapter> adapter; DXGI_ADAPTER_DESC desc{};
        if (FAILED(device->QueryInterface(IID_PPV_ARGS(&dxgi))) || FAILED(dxgi->GetAdapter(&adapter)) || FAILED(adapter->GetDesc(&desc))) { setError("Falha ao consultar a GPU do Qt."); return nullptr; }
        const auto luid = QString("%1%2").arg(quint32(desc.AdapterLuid.HighPart),8,16,QChar('0')).arg(desc.AdapterLuid.LowPart,8,16,QChar('0'));
        if (luid != descriptor_["luid"].toString()) { setError("As duas janelas de dispositivo usam GPUs diferentes."); return nullptr; }
        ComPtr<ID3D11Device1> device1;
        if (FAILED(device->QueryInterface(IID_PPV_ARGS(&device1)))) { setError("D3D11.1 indisponível."); return nullptr; }
        node = new TextureNode;
        node->generation = generation;
        const auto buffers = descriptor_["buffers"].toArray();
        for (int i = 0; i < 3; ++i) {
            const auto name = buffers[i].toString();
            if (FAILED(device1->OpenSharedResourceByName(reinterpret_cast<LPCWSTR>(name.utf16()), DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
                IID_PPV_ARGS(&node->textures[i]))) || FAILED(node->textures[i].As(&node->mutexes[i]))) {
                delete node; setError("Não foi possível importar a textura GPU compartilhada."); return nullptr;
            }
            node->wrappers[i] = QNativeInterface::QSGD3D11Texture::fromNative(node->textures[i].Get(), window(), QSize(descriptor_["width"].toInt(), descriptor_["height"].toInt()));
            if (!node->wrappers[i]) { delete node; setError("Qt não aceitou a textura nativa."); return nullptr; }
        }
    }
    int selected = -1;
    int selectedSequence = node->heldSequence;
    bool retry = false;
    {
        for (int i = 0; i < 3; ++i) {
            if (!pending_[i]) continue;
            const auto acquired = node->mutexes[i]->AcquireSync(1, 0);
            if (acquired == WAIT_TIMEOUT) { retry = true; continue; }
            if (acquired != S_OK) {
                if (selected >= 0) node->mutexes[selected]->ReleaseSync(0);
                setError("Textura invalidada pelo produtor."); delete node; return nullptr;
            }
            const int sequence = pending_[i];
            pending_[i] = 0;
            // Choose the newest READY frame, not the newest announcement.
            // A timed-out older slot must never replace a newer displayed frame.
            if (sequence <= selectedSequence) { node->mutexes[i]->ReleaseSync(0); continue; }
            if (selected >= 0) node->mutexes[selected]->ReleaseSync(0);
            selected = i;
            selectedSequence = sequence;
        }
        if (selected >= 0) {
            // Keep ownership across UI-only redraws of the same texture.
            const int skipped = node->heldSequence > 0 ? qMax(0, selectedSequence - node->heldSequence - 1) : 0;
            if (node->held >= 0) node->mutexes[node->held]->ReleaseSync(0);
            node->held = selected;
            node->heldSequence = selectedSequence;
            node->setTexture(node->wrappers[selected]);
            QMetaObject::invokeMethod(this, [this, skipped] {
                ++frames_; emit framesPresentedChanged();
                if (skipped) { framesSkipped_ += skipped; emit framesSkippedChanged(); }
            }, Qt::QueuedConnection);
        }
    }
    // Retry even if the producer pauses after its final notification. update()
    // schedules a scene-graph frame; it does not block on or spin on a GPU mutex.
    if (retry) QMetaObject::invokeMethod(this, [this] { update(); }, Qt::QueuedConnection);
    const QSizeF source(descriptor_["width"].toInt(),descriptor_["height"].toInt());
    const auto scale = qMin(width()/source.width(), height()/source.height());
    const QSizeF fitted = source * scale;
    node->setRect(QRectF(QPointF((width()-fitted.width())/2, (height()-fitted.height())/2), fitted));
    if (!node->texture()) { delete node; return nullptr; }
    return node;
}

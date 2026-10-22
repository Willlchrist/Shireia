#define NOMINMAX
#include "GpuProducer.h"
#include "FrameSize.h"
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <QJsonArray>
#include <QUuid>
#include <QElapsedTimer>
#include <stdexcept>
#include <array>
#include <cmath>
using Microsoft::WRL::ComPtr;
namespace {
void check(HRESULT value, const char* operation) {
    if (FAILED(value)) throw std::runtime_error(QString("%1: HRESULT 0x%2").arg(operation).arg(quint32(value), 8, 16, QChar('0')).toStdString());
}
QString luidString(LUID luid) { return QString("%1%2").arg(quint32(luid.HighPart),8,16,QChar('0')).arg(luid.LowPart,8,16,QChar('0')); }
}
struct GpuProducer::Impl {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    struct Slot {
        ComPtr<ID3D11Texture2D> texture;
        ComPtr<ID3D11RenderTargetView> target;
        ComPtr<IDXGIKeyedMutex> mutex;
        HANDLE handle = nullptr;
        ~Slot() { if (handle) CloseHandle(handle); }
    };
    std::array<Slot, 3> buffers;
    int sequence = 0;
    int next = 0;
    QString generation;
    bool emulation = false;
    int width = 1280, height = 720;
};
GpuProducer::GpuProducer() = default;
GpuProducer::~GpuProducer() = default;
void GpuProducer::stop() { impl_.reset(); }
QJsonObject GpuProducer::start(const QString& session, bool emulation, int width, int height) {
    if (!((width == 1280 && height == 720) || (width == 1920 && height == 1080)))
        throw std::runtime_error("Unsupported presentation size");
    stop();
    auto impl = std::make_unique<Impl>();
    impl->emulation = emulation;
    impl->width = width; impl->height = height;
    check(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        nullptr, 0, D3D11_SDK_VERSION, &impl->device, nullptr, &impl->context), "D3D11CreateDevice");
    ComPtr<IDXGIDevice> dxgi; check(impl->device.As(&dxgi), "IDXGIDevice");
    ComPtr<IDXGIAdapter> adapter; check(dxgi->GetAdapter(&adapter), "GetAdapter");
    DXGI_ADAPTER_DESC adapterDesc{}; check(adapter->GetDesc(&adapterDesc), "GetDesc");
    QJsonArray names;
    const auto generation = QUuid::createUuid().toString(QUuid::Id128);
    impl->generation = generation;
    for (int i = 0; i < 3; ++i) {
        auto& slot = impl->buffers[i];
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = width; desc.Height = height; desc.MipLevels = 1; desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT; desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
        check(impl->device->CreateTexture2D(&desc, nullptr, &slot.texture), "CreateTexture2D");
        check(impl->device->CreateRenderTargetView(slot.texture.Get(), nullptr, &slot.target), "CreateRenderTargetView");
        check(slot.texture.As(&slot.mutex), "IDXGIKeyedMutex");
        ComPtr<IDXGIResource1> resource; check(slot.texture.As(&resource), "IDXGIResource1");
        const auto name = "Local\\shireia-" + session + "-" + generation + "-" + QString::number(i);
        check(resource->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
            reinterpret_cast<LPCWSTR>(name.utf16()), &slot.handle), "CreateSharedHandle");
        names.append(name);
    }
    impl_ = std::move(impl);
    return {{"width",width},{"height",height},{"buffers",names},{"generation",generation},
        {"adapter",QString::fromWCharArray(adapterDesc.Description)},{"luid",luidString(adapterDesc.AdapterLuid)},
        {"format","rgba8"},{"content",emulation ? "emulation-headless-readback" : "diagnostic-not-emulation"}};
}
QJsonObject GpuProducer::tick(const QByteArray& pixels) {
    if (!impl_) return {};
    if (impl_->emulation && pixels.size() != impl_->width * impl_->height * 4) return {};
    for (int attempt = 0; attempt < 3; ++attempt) {
        const int index = (impl_->next + attempt) % 3;
        auto& slot = impl_->buffers[index];
        const auto acquired = slot.mutex->AcquireSync(0, 0);
        if (acquired == WAIT_TIMEOUT) continue;
        if (acquired != S_OK) throw std::runtime_error("Producer keyed mutex unavailable");
        const float wave = 0.5f + 0.5f * std::sin(float(impl_->sequence) * 0.07f);
        const float color[4]{0.15f + wave * 0.5f, 0.15f, 0.65f - wave * 0.3f, 1.0f};
        QElapsedTimer transfer; transfer.start();
        if (impl_->emulation) impl_->context->UpdateSubresource(slot.texture.Get(),0,nullptr,pixels.constData(),impl_->width*4,0);
        else impl_->context->ClearRenderTargetView(slot.target.Get(), color);
        impl_->context->Flush();
        const auto transferUs = transfer.nsecsElapsed() / 1000;
        check(slot.mutex->ReleaseSync(1), "Producer ReleaseSync");
        impl_->next = (index + 1) % 3;
        return {{"slot",index},{"sequence",++impl_->sequence},{"generation",impl_->generation},{"transferUs",int(transferUs)}};
    }
    return {}; // bounded backpressure: hidden UI cannot accumulate frames
}

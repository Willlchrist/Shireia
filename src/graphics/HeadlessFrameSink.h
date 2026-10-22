#pragma once

#include <cstdint>
#include <functional>
#include <span>

namespace VideoCore { class RendererBase; }
namespace Shireia {
// Register before GPU::Start. Called synchronously on the compositor thread after
// Finish + Invalidate. The span must not escape the callback. No Vulkan ABI changes.
using HeadlessFrameSink = std::function<void(std::span<const std::uint8_t>)>;
void SetHeadlessFrameSink(const VideoCore::RendererBase* renderer, HeadlessFrameSink sink);
}

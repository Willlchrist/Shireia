#pragma once

// Presentation size of the temporary CPU readback bridge. Keep both processes in sync.
namespace ShireiaFrame {
inline constexpr int Width = 1280;
inline constexpr int Height = 720;
inline constexpr int Bytes = Width * Height * 4;
}

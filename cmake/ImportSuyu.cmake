# Import the source-built Suyu core produced by scripts/build.ps1. Constrain
# both paths to this project so a stale cache cannot use an external checkout
# or a prebuilt archive from another tree.
set(_shireia_suyu_source_default "${CMAKE_SOURCE_DIR}/third_party/suyu")
set(_shireia_suyu_build_default "${CMAKE_BINARY_DIR}/suyu-core")
set(SHIREIA_SUYU_SOURCE "${_shireia_suyu_source_default}" CACHE PATH "Vendored Suyu source")
set(SHIREIA_SUYU_BUILD "${_shireia_suyu_build_default}" CACHE PATH "Local source build of Suyu core")
file(REAL_PATH "${SHIREIA_SUYU_SOURCE}" _shireia_suyu_source_real)
file(REAL_PATH "${_shireia_suyu_source_default}" _shireia_suyu_source_expected)
file(REAL_PATH "${SHIREIA_SUYU_BUILD}" _shireia_suyu_build_real)
file(REAL_PATH "${_shireia_suyu_build_default}" _shireia_suyu_build_expected)
if(NOT _shireia_suyu_source_real STREQUAL _shireia_suyu_source_expected)
    message(FATAL_ERROR "SHIREIA_SUYU_SOURCE must use the vendored source in third_party/suyu.")
endif()
if(NOT _shireia_suyu_build_real STREQUAL _shireia_suyu_build_expected)
    message(FATAL_ERROR "SHIREIA_SUYU_BUILD must use the source build under this CMake build directory.")
endif()
if(NOT MINGW OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "The integrated core profile requires the supported GNU/UCRT64 x86_64 toolchain.")
endif()
file(GLOB _compiler_metadata "${SHIREIA_SUYU_BUILD}/CMakeFiles/*/CMakeCXXCompiler.cmake")
list(LENGTH _compiler_metadata _metadata_count)
if(NOT _metadata_count EQUAL 1)
    message(FATAL_ERROR "Cannot uniquely verify the local Suyu compiler metadata")
endif()
file(READ "${_compiler_metadata}" _compiler_text)
if(NOT _compiler_text MATCHES "CMAKE_CXX_COMPILER_VERSION \"${CMAKE_CXX_COMPILER_VERSION}\"")
    message(FATAL_ERROR "Local Suyu compiler version differs from the Shireia toolchain.")
endif()
file(STRINGS "${SHIREIA_SUYU_BUILD}/compile_commands.json" _candidate_lines REGEX "\"command\".*core[.]cpp")
foreach(_line IN LISTS _candidate_lines)
    string(STRIP "${_line}" _line)
    string(REGEX REPLACE ",$" "" _line "${_line}")
    string(JSON _candidate GET "{${_line}}" command)
    if(_candidate MATCHES "-c .*[/\\\\]core[/\\\\]core[.]cpp$")
        set(_command "${_candidate}")
        break()
    endif()
endforeach()
if(NOT _command)
    message(FATAL_ERROR "Core compile metadata not found")
endif()
if(NOT _command MATCHES "ARCHITECTURE_x86_64=1")
    message(FATAL_ERROR "Vendored Suyu core build is not x86_64")
endif()
string(REGEX MATCHALL "-I[^ ]+|-isystem [^ ]+" _include_flags "${_command}")
set(_suyu_includes)
foreach(_flag IN LISTS _include_flags)
    string(REGEX REPLACE "^-I|^-isystem " "" _include "${_flag}")
    list(APPEND _suyu_includes "${_include}")
endforeach()
set(_suyu_archives
    src/core/libcore.a src/common/libcommon.a src/frontend_common/libfrontend_common.a src/audio_core/libaudio_core.a
    src/input_common/libinput_common.a
    src/hid_core/libhid_core.a src/network/libnetwork.a src/video_core/libvideo_core.a
    src/nintendo_library/libnintendo_library.a src/shader_recompiler/libshader_recompiler.a
    src/video_core/gpu_logging/libgpu_logging.a src/dynarmic/src/dynarmic/libdynarmic.a
    externals/libtz.a externals/libbc_decoder.a externals/libstb.a externals/libdemangle.a
    externals/glad/libglad.a _deps/enet-build/libenet.a _deps/fmt-build/libfmt.a
    _deps/sirit-build/src/libsirit.a _deps/cubeb-build/libcubeb.a _deps/sdl3-build/libSDL3.a)
set(_suyu_libraries)
foreach(_relative IN LISTS _suyu_archives)
    if(NOT EXISTS "${SHIREIA_SUYU_BUILD}/${_relative}")
        message(FATAL_ERROR "Missing core dependency: ${_relative}")
    endif()
    list(APPEND _suyu_libraries "${SHIREIA_SUYU_BUILD}/${_relative}")
endforeach()
foreach(_boost IN ITEMS date_time container filesystem context process)
    file(GLOB _library "${SHIREIA_SUYU_BUILD}/_deps/boost-build/libs/${_boost}/libboost_${_boost}-*.a")
    list(LENGTH _library _matches)
    if(NOT _matches EQUAL 1)
        message(FATAL_ERROR "Missing or ambiguous Boost ${_boost} dependency")
    endif()
    list(APPEND _suyu_libraries "${_library}")
endforeach()
add_library(ShireiaSuyuAdapter STATIC src/corehost/SuyuAdapter.cpp src/corehost/SuyuAdapter.h src/corehost/LibraryJob.cpp
    src/corehost/suyu_keyboard/qt_software_keyboard.cpp
    src/corehost/suyu_keyboard/qt_software_keyboard.h
    src/corehost/suyu_keyboard/qt_software_keyboard.ui
    src/corehost/suyu_keyboard/overlay.qrc)
target_include_directories(ShireiaSuyuAdapter SYSTEM PRIVATE ${_suyu_includes})
target_compile_definitions(ShireiaSuyuAdapter PRIVATE ARCHITECTURE_x86_64=1 HAS_OPENGL YUZU_ROOM
    _FILE_OFFSET_BITS=64 _WIN32_WINNT=0x0A00 WINVER=0x0A00 BOOST_CONTAINER_STATIC_LINK BOOST_FILESYSTEM_STATIC_LINK=1)
# Compile the original Vulkan renderer source locally so Shireia can instrument its
# headless readback wait without touching the read-only reference checkout or changing ABI.
add_library(ShireiaRendererOverlay OBJECT src/graphics/suyu_overlay/renderer_vulkan.cpp
    src/graphics/suyu_overlay/present_layer.cpp)
target_include_directories(ShireiaRendererOverlay SYSTEM PRIVATE ${_suyu_includes}
    "${SHIREIA_SUYU_BUILD}/src/video_core/host_shaders/include" "${SHIREIA_SUYU_BUILD}/src")
target_compile_definitions(ShireiaRendererOverlay PRIVATE ARCHITECTURE_x86_64=1 HAS_OPENGL YUZU_ROOM
    _FILE_OFFSET_BITS=64 _WIN32_WINNT=0x0A00 WINVER=0x0A00 BOOST_CONTAINER_NO_LIB BOOST_CONTAINER_STATIC_LINK
    BOOST_DATE_TIME_NO_LIB BOOST_DATE_TIME_STATIC_LINK BOOST_FILESYSTEM_NO_LIB BOOST_FILESYSTEM_STATIC_LINK=1
    BOOST_PROCESS_STATIC_LINK MINGW_HAS_SECURE_API)
target_compile_options(ShireiaRendererOverlay PRIVATE -O3 -DNDEBUG -fno-rtti -mcx16 -msse4.1
    -Wno-unknown-pragmas -Wno-unused-parameter -Wno-missing-field-initializers -Wno-shadow -Wno-conversion
    -Wno-sign-conversion -Wno-implicit-fallthrough -Wno-attributes -Wno-invalid-offsetof
    -Wno-array-bounds -Wno-stringop-overread -Wno-stringop-overflow)
target_compile_options(ShireiaSuyuAdapter PRIVATE -fno-rtti -mcx16 -msse4.1)
target_link_libraries(ShireiaSuyuAdapter PUBLIC Qt6::Core Qt6::Widgets "$<LINK_GROUP:RESCAN,${_suyu_libraries}>"
    archive avcodec avdevice avfilter avformat avutil swresample swscale z brotlicommon brotlienc brotlidec
    ssl crypto lz4 zstd opus ntdll bcrypt winmm iphlpapi ws2_32 wlanapi psapi imm32 version crypt32 rpcrt4
    gdi32 wldap32 mswsock setupapi dinput8 oleaut32 uuid shell32 avrt ole32 ksuser)
file(SHA256 "${SHIREIA_SUYU_BUILD}/src/core/libcore.a" _core_hash)
file(SHA256 "${SHIREIA_SUYU_BUILD}/src/core/CMakeFiles/core.dir/core.cpp.obj" _object_hash)
file(SHA256 "${SHIREIA_SUYU_SOURCE}/src/core/core.h" _header_hash)
file(WRITE "${CMAKE_BINARY_DIR}/core-provenance.txt"
    "source=${SHIREIA_SUYU_SOURCE}\nbuild=${SHIREIA_SUYU_BUILD}\ncompiler=${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}\narchive-sha256=${_core_hash}\ncore-object-sha256=${_object_hash}\ncore-header-sha256=${_header_hash}\nmode=vendored-source-build\n")

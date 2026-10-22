cmake_minimum_required(VERSION 3.24)
project(ShireiaNext VERSION 0.4.5 LANGUAGES CXX RC)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 6.5 REQUIRED COMPONENTS Quick QuickControls2 Network Widgets)
find_package(SDL2 2.0.14 REQUIRED CONFIG)
option(SHIREIA_BUILD_CORE "Build the real Suyu core host using verified compatible archives" ON)
set(SHIREIA_TEST_HOMEBREW "" CACHE FILEPATH "Optional authorized NRO for real integrated session tests; never downloaded by the build")
qt_standard_project_setup(REQUIRES 6.5)
add_library(ShireiaIpc STATIC src/ipc/JsonChannel.cpp src/ipc/JsonChannel.h)
target_link_libraries(ShireiaIpc PUBLIC Qt6::Network)
add_library(ShireiaSession STATIC src/emulation/EmulationManager.cpp src/emulation/EmulationManager.h)
target_link_libraries(ShireiaSession PUBLIC ShireiaIpc)
target_compile_definitions(ShireiaSession PRIVATE SHIREIA_CORE_ENABLED=$<BOOL:${SHIREIA_BUILD_CORE}>)
if(SHIREIA_BUILD_CORE)
    include(cmake/ImportSuyu.cmake)
    qt_add_executable(ShireiaCore src/corehost/main.cpp src/graphics/GpuProducer.cpp src/graphics/GpuProducer.h)
    target_link_libraries(ShireiaCore PRIVATE ShireiaSuyuAdapter ShireiaIpc d3d11 dxgi dxguid)
    target_sources(ShireiaCore PRIVATE $<TARGET_OBJECTS:ShireiaRendererOverlay>)
    get_filename_component(SHIREIA_COMPILER_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    find_file(SHIREIA_LIBARCHIVE_RUNTIME NAMES libarchive-13.dll HINTS "${SHIREIA_COMPILER_BIN_DIR}" REQUIRED)
    add_custom_command(TARGET ShireiaCore POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SHIREIA_LIBARCHIVE_RUNTIME}" "$<TARGET_FILE_DIR:ShireiaCore>"
        COMMENT "Deploying libarchive runtime required by ShireiaCore")
endif()
qt_add_executable(Shireia src/app/main.cpp src/app/AppModel.cpp src/app/AppModel.h
    src/app/GameContentManager.cpp src/app/GameContentManager.h
    src/app/CompatibilityStore.cpp src/app/CompatibilityStore.h
    src/app/SetupAssistant.cpp src/app/SetupAssistant.h
    assets/brand/shireia.rc
    src/app/LibraryResources.cpp src/app/LibraryResources.h
      src/app/QualityTools.cpp src/app/QualityTools.h
    src/graphics/GpuSurface.cpp src/graphics/GpuSurface.h
    tests/FrontendSmoke.cpp tests/FrontendSmoke.h
    src/settings/SettingsStore.cpp src/settings/SettingsStore.h
    src/input/ControllerInput.cpp src/input/ControllerInput.h src/input/ControllerGameInput.cpp)
set_target_properties(Shireia PROPERTIES OUTPUT_NAME "Shireia Next")
# CMake's resource compiler does not reliably discover files included by .rc
# scripts. Track the ICO explicitly so replacing the app icon relinks the EXE.
set_source_files_properties(assets/brand/shireia.rc PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/brand/shireia.ico")
qt_add_resources(Shireia frontend PREFIX "/" FILES
    src/frontend/Main.qml
    src/frontend/pages/SetupWizard.qml
    third_party/NOTICES.md
    third_party/licenses/Suyu-LICENSE.txt
    third_party/licenses/GPL-2.0-or-later.txt
    third_party/licenses/GPL-3.0-or-later.txt
    third_party/licenses/CC-BY-4.0.txt
    third_party/licenses/LGPL-2.1-or-later.txt
    third_party/licenses/LGPL-3.0-only.txt
    third_party/licenses/Qt-GPL-exception-1.0.txt
    third_party/licenses/SDL2-zlib-LICENSE.txt
    third_party/licenses/SDL3-zlib-LICENSE.txt
    third_party/licenses/libarchive-LICENSE.txt
    third_party/licenses/OpenSSL-LICENSE.txt
    third_party/licenses/zlib-LICENSE.txt
    third_party/licenses/BSL-1.0.txt
    third_party/licenses/Dynarmic-LICENSE.txt
    third_party/licenses/fmt-MIT-LICENSE.txt
    third_party/licenses/ENet-LICENSE.txt
    third_party/licenses/sirit-AGPL-3.0-LICENSE.txt
    third_party/licenses/cubeb-LICENSE.txt
    third_party/licenses/MPL-2.0.txt
    third_party/licenses/BSD-2-Clause.txt
    third_party/licenses/Apache-2.0-LLVM-exception.txt
    third_party/licenses/LLVM-exception.txt
    third_party/licenses/MIT.txt
    third_party/licenses/Lagoon-MIT-LICENSE.txt
    src/frontend/components/ActionButton.qml src/frontend/components/Field.qml
    src/frontend/components/Select.qml src/frontend/components/Toggle.qml
    src/frontend/components/Panel.qml src/frontend/components/Glyph.qml src/frontend/components/Brand.qml
    src/frontend/components/MainSidebar.qml src/frontend/components/QuickMenu.qml
    src/frontend/pages/LibraryPage.qml src/frontend/pages/SettingsPage.qml src/frontend/pages/FoundationPage.qml
    assets/brand/shireia-logo.png)
target_link_libraries(Shireia PRIVATE Qt6::Quick Qt6::QuickControls2 SDL2::SDL2 ShireiaSession d3d11 dxgi dxguid)
target_compile_definitions(Shireia PRIVATE SHIREIA_VERSION="${PROJECT_VERSION}")
enable_testing()
if(SHIREIA_BUILD_CORE)
    add_test(NAME core-initialize COMMAND ShireiaCore --self-test)
    set_tests_properties(core-initialize PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1")
    add_test(NAME core-execution COMMAND ShireiaCore --execution-self-test)
    add_test(NAME core-vulkan-execution COMMAND ShireiaCore --execution-self-test --vulkan-test)
    set_tests_properties(core-execution core-vulkan-execution PROPERTIES TIMEOUT 20 ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1")
    qt_add_executable(ShireiaCoreTests tests/CoreIpcTests.cpp)
    target_link_libraries(ShireiaCoreTests PRIVATE ShireiaSession)
    add_dependencies(ShireiaCoreTests ShireiaCore)
    add_test(NAME core-ipc COMMAND ShireiaCoreTests)
    set_tests_properties(core-ipc PROPERTIES TIMEOUT 60 ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1")
    qt_add_executable(ShireiaResourceTests tests/LibraryResourceTests.cpp
        src/app/LibraryResources.cpp src/app/LibraryResources.h src/app/AppModel.cpp src/app/AppModel.h
        src/settings/SettingsStore.cpp src/settings/SettingsStore.h)
    target_link_libraries(ShireiaResourceTests PRIVATE Qt6::Gui ShireiaSession)
    add_dependencies(ShireiaResourceTests ShireiaCore)
    add_test(NAME library-resources COMMAND ShireiaResourceTests)
    set_tests_properties(library-resources PROPERTIES TIMEOUT 60 ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1")
    add_test(NAME gpu-sharing COMMAND Shireia --gpu-smoke-test)
    set_tests_properties(gpu-sharing PROPERTIES TIMEOUT 60 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1;QT_QPA_PLATFORM=windows;QSG_RHI_BACKEND=d3d11;QT_QUICK_BACKEND=")
    if(SHIREIA_TEST_HOMEBREW)
        if(NOT EXISTS "${SHIREIA_TEST_HOMEBREW}")
            message(FATAL_ERROR "SHIREIA_TEST_HOMEBREW must point to an existing authorized NRO")
        endif()
        add_test(NAME integrated-homebrew COMMAND Shireia --program-smoke-test "${SHIREIA_TEST_HOMEBREW}")
        set_tests_properties(integrated-homebrew PROPERTIES TIMEOUT 90 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1;QT_QPA_PLATFORM=windows;QSG_RHI_BACKEND=d3d11;QT_QUICK_BACKEND=")
    endif()
endif()
qt_add_executable(ShireiaBackendTests tests/BackendTests.cpp
    src/input/ControllerInput.cpp src/input/ControllerInput.h src/input/ControllerGameInput.cpp
    src/settings/SettingsStore.cpp src/settings/SettingsStore.h
    src/app/AppModel.cpp src/app/AppModel.h)
target_link_libraries(ShireiaBackendTests PRIVATE Qt6::Gui SDL2::SDL2)
target_compile_options(ShireiaBackendTests PRIVATE -mssse3)
add_test(NAME backend-tests COMMAND ShireiaBackendTests)
set_tests_properties(backend-tests PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1")
add_test(NAME frontend-smoke COMMAND Shireia --smoke-test)
set_tests_properties(frontend-smoke PROPERTIES TIMEOUT 60 ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FORCE_STDERR_LOGGING=1")
add_test(NAME setup-wizard-smoke COMMAND Shireia --setup-smoke-test)
set_tests_properties(setup-wizard-smoke PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FORCE_STDERR_LOGGING=1")

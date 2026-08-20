# SPDX-FileCopyrightText: 2026 Emoji-cord contributors
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.24)

project(EmojiCord VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

include(CTest)

find_package(Qt6 6.6 REQUIRED COMPONENTS Core DBus Gui Quick QuickControls2 Qml WaylandClient)
find_package(KF6WindowSystem REQUIRED)
find_package(Qt6CorePrivate 6.6 REQUIRED)
find_package(Qt6GuiPrivate 6.6 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(Wayland REQUIRED IMPORTED_TARGET wayland-client)
pkg_check_modules(XkbCommon REQUIRED IMPORTED_TARGET xkbcommon)
pkg_check_modules(X11 REQUIRED IMPORTED_TARGET x11)
pkg_check_modules(XInput REQUIRED IMPORTED_TARGET xi)
pkg_check_modules(XTest REQUIRED IMPORTED_TARGET xtst)

set(INPUT_METHOD_PROTOCOL ${CMAKE_CURRENT_SOURCE_DIR}/protocols/input-method-unstable-v1.xml)
set(PROTOCOL_HEADER ${CMAKE_CURRENT_BINARY_DIR}/input-method-unstable-v1-client-protocol.h)
set(PROTOCOL_CODE ${CMAKE_CURRENT_BINARY_DIR}/input-method-unstable-v1-protocol.c)
set(LAYER_SHELL_PROTOCOL ${CMAKE_CURRENT_SOURCE_DIR}/protocols/wlr-layer-shell-unstable-v1.xml)
set(LAYER_SHELL_HEADER ${CMAKE_CURRENT_BINARY_DIR}/wlr-layer-shell-unstable-v1-client-protocol.h)
set(LAYER_SHELL_CODE ${CMAKE_CURRENT_BINARY_DIR}/wlr-layer-shell-unstable-v1-protocol.c)

add_custom_command(
    OUTPUT ${PROTOCOL_HEADER}
    COMMAND wayland-scanner client-header ${INPUT_METHOD_PROTOCOL} ${PROTOCOL_HEADER}
    DEPENDS ${INPUT_METHOD_PROTOCOL}
    VERBATIM
)

add_custom_command(
    OUTPUT ${LAYER_SHELL_HEADER}
    COMMAND wayland-scanner client-header ${LAYER_SHELL_PROTOCOL} ${LAYER_SHELL_HEADER}
    DEPENDS ${LAYER_SHELL_PROTOCOL}
    VERBATIM
)
add_custom_command(
    OUTPUT ${LAYER_SHELL_CODE}
    COMMAND wayland-scanner private-code ${LAYER_SHELL_PROTOCOL} ${LAYER_SHELL_CODE}
    DEPENDS ${LAYER_SHELL_PROTOCOL}
    VERBATIM
)
add_custom_command(
    OUTPUT ${PROTOCOL_CODE}
    COMMAND wayland-scanner private-code ${INPUT_METHOD_PROTOCOL} ${PROTOCOL_CODE}
    DEPENDS ${INPUT_METHOD_PROTOCOL}
    VERBATIM
)

add_library(emojicord_core STATIC
    src/appsettings.cpp
    src/appsettings.h
    src/candidatemodel.cpp
    src/candidatemodel.h
    src/completioncontroller.cpp
    src/completioncontroller.h
    src/contextrouter.cpp
    src/contextrouter.h
    src/emojicatalog.cpp
    src/emojicatalog.h
    src/emojimatcher.cpp
    src/emojimatcher.h
    src/querystate.cpp
    src/querystate.h
    src/usagestore.cpp
    src/usagestore.h
    src/waylandinputmethod.cpp
    src/waylandinputmethod.h
    src/xwaylandfallback.cpp
    src/xwaylandfallback.h
    ${PROTOCOL_HEADER}
    ${PROTOCOL_CODE}
)
target_link_libraries(emojicord_core PUBLIC
    Qt6::Core
    Qt6::DBus
    PkgConfig::Wayland
    PkgConfig::XkbCommon
    PkgConfig::X11
    PkgConfig::XInput
    PkgConfig::XTest
)
target_include_directories(emojicord_core PUBLIC src PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

add_executable(emoji-cord
    src/effectcapabilities.cpp
    src/effectcapabilities.h
    src/fallbacklayershell.cpp
    src/fallbacklayershell.h
    src/inputpanelshell.cpp
    src/inputpanelshell.h
    src/main.cpp
    src/pickerwindow.cpp
    src/pickerwindow.h
    ${LAYER_SHELL_HEADER}
    ${LAYER_SHELL_CODE}
)
target_link_libraries(emoji-cord PRIVATE
    emojicord_core
    Qt6::CorePrivate
    Qt6::Gui
    Qt6::GuiPrivate
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Qml
    Qt6::WaylandClient
    KF6::WindowSystem
)
target_include_directories(emoji-cord PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

if(NOT DEFINED EMOJICORD_QT_PRIVATE_INCLUDE_ROOT)
    get_target_property(_qt_core_include_dirs Qt6::Core INTERFACE_INCLUDE_DIRECTORIES)
    foreach(_include_dir IN LISTS _qt_core_include_dirs)
        if(_include_dir MATCHES "/QtCore$")
            get_filename_component(_qt_private_root "${_include_dir}" DIRECTORY)
            break()
        endif()
    endforeach()
    set(EMOJICORD_QT_PRIVATE_INCLUDE_ROOT "${_qt_private_root}" CACHE PATH
        "Qt include root containing versioned private Wayland headers")
endif()

if(NOT EXISTS "${EMOJICORD_QT_PRIVATE_INCLUDE_ROOT}/QtWaylandClient/${Qt6_VERSION}/QtWaylandClient/private/qwaylandwindow_p.h")
    message(FATAL_ERROR
        "QtWayland private headers were not found. Install qt6-qtwayland-devel "
        "or set EMOJICORD_QT_PRIVATE_INCLUDE_ROOT to the matching Qt include root.")
endif()
target_include_directories(emoji-cord PRIVATE
    ${EMOJICORD_QT_PRIVATE_INCLUDE_ROOT}/QtWaylandClient/${Qt6_VERSION}
    ${EMOJICORD_QT_PRIVATE_INCLUDE_ROOT}/QtWaylandClient/${Qt6_VERSION}/QtWaylandClient
    ${EMOJICORD_QT_PRIVATE_INCLUDE_ROOT}/QtWaylandGlobal/${Qt6_VERSION}
    ${EMOJICORD_QT_PRIVATE_INCLUDE_ROOT}/QtWaylandGlobal/${Qt6_VERSION}/QtWaylandGlobal
)

qt_add_resources(emoji-cord picker_resources
    PREFIX "/"
    FILES
        data/emoji-demo.json
        qml/Picker.qml
        qml/Settings.qml
)

if(BUILD_TESTING)
    find_package(Qt6 6.6 REQUIRED COMPONENTS Test)
    add_executable(emojicord_core_tests tests/coretest.cpp)
    target_link_libraries(emojicord_core_tests PRIVATE emojicord_core Qt6::Test)
    add_test(NAME emojicord_core_tests COMMAND emojicord_core_tests)
endif()

install(TARGETS emoji-cord RUNTIME DESTINATION bin)
install(FILES
    data/io.github.puzll.EmojiCord.desktop
    data/io.github.puzll.EmojiCord.Settings.desktop
    DESTINATION share/applications)
install(DIRECTORY kwin/emoji-cord-context DESTINATION share/kwin/scripts)
install(FILES README.md THIRD_PARTY.md DESTINATION share/doc/emoji-cord)
install(FILES LICENSES/MIT.txt DESTINATION share/licenses/emoji-cord)

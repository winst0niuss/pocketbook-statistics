# Host tests for the Qt half of the app.
#
# A project of its own, built against whatever Qt 6 the host has — never the
# cross-build. The device binary links the reader's own Qt 6.8.2 from /ebrmain
# and is built in Docker (`make qt`); nothing here goes near that toolchain, and
# a version difference does not matter, because what is under test is the
# app's own logic rather than Qt's.
#
# The one file that cannot be built here is inkview_bridge.cpp: it includes the
# firmware's inkview.h and resolves every call with dlsym out of the reader's
# libinkview.so. test/qt/inkview_stub.cpp stands in for it.
#
#   cmake -S test -B build-host && cmake --build build-host && ctest --test-dir build-host
#
cmake_minimum_required(VERSION 3.21)
project(pocketbook-statistics-host-tests CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Test QuickTest)
qt_standard_project_setup()

set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
file(READ "${ROOT}/VERSION" APP_VERSION)
string(STRIP "${APP_VERSION}" APP_VERSION)

enable_testing()

# Everything under test, plus the two bundled amalgamations the device build
# compiles in as well. The C core is here because the bridge calls straight
# into it — its own asserts live in test/test_tracker.c.
qt_add_library(pbstats STATIC
  ${ROOT}/qt/src/book_cover.cpp
  ${ROOT}/qt/src/device_paths.cpp
  ${ROOT}/qt/src/installer.cpp
  ${ROOT}/qt/src/shim.cpp
  ${ROOT}/qt/src/stats_bridge.cpp
  ${ROOT}/qt/src/update_log.cpp
  ${ROOT}/qt/src/updater.cpp
  ${ROOT}/src/daemon.c
  ${ROOT}/src/log.c
  ${ROOT}/src/stats_db.c
  ${ROOT}/src/tracker.c
  ${ROOT}/src/version.c
  ${ROOT}/qt/third_party/sqlite3.c
  ${ROOT}/qt/third_party/miniz.c
  ${CMAKE_CURRENT_SOURCE_DIR}/qt/inkview_stub.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/qt/fixtures.cpp
)
target_include_directories(pbstats PUBLIC
  ${ROOT}/src ${ROOT}/qt/src ${ROOT}/qt/third_party
  ${CMAKE_CURRENT_SOURCE_DIR}/qt)
target_compile_definitions(pbstats PUBLIC
  SQLITE_OMIT_LOAD_EXTENSION SQLITE_THREADSAFE=1
  APP_VERSION="${APP_VERSION}")
target_link_libraries(pbstats PUBLIC Qt6::Core Qt6::Gui Qt6::Qml Qt6::Test)
# The app's own warnings; the two amalgamations are third-party and noisy.
set_source_files_properties(
  ${ROOT}/qt/third_party/sqlite3.c ${ROOT}/qt/third_party/miniz.c
  PROPERTIES COMPILE_OPTIONS "-w")

# The shipped resource: the shim script the installer writes, the launcher
# icons, and the QML the screen tests instantiate.
qt_add_resources(pbstats "app"
  PREFIX "/"
  BASE ${ROOT}/qt/qml
  FILES
    ${ROOT}/qt/qml/qmldir
    ${ROOT}/qt/qml/Tr.qml
    ${ROOT}/qt/qml/main.qml
    ${ROOT}/qt/qml/NavIcon.qml
    ${ROOT}/qt/qml/PanelDialog.qml
    ${ROOT}/qt/qml/BookDialog.qml
    ${ROOT}/qt/qml/OverviewTab.qml
    ${ROOT}/qt/qml/CalendarTab.qml
    ${ROOT}/qt/qml/AboutTab.qml
    ${ROOT}/qt/qml/StreakTab.qml
    ${ROOT}/qt/qml/shim/open-book.sh
    ${ROOT}/qt/qml/pocketbook-statistics.bmp
    ${ROOT}/qt/qml/pocketbook-statistics_f.bmp
)
file(GLOB CATALOGS ${ROOT}/qt/qml/i18n/*.js)
qt_add_resources(pbstats "catalogs"
  PREFIX "/"
  BASE ${ROOT}/qt/qml
  FILES ${CATALOGS})

foreach(name book_cover installer shim stats_bridge updater)
  add_executable(tst_${name} qt/tst_${name}.cpp)
  target_link_libraries(tst_${name} PRIVATE pbstats)
  add_test(NAME ${name} COMMAND tst_${name})
endforeach()

# The screens, against the same bridge over a seeded database. They import the
# firmware's QML module, which exists only on the reader — test/qml/stubs is
# what stands in for it.
add_executable(tst_qml qml/main.cpp)
target_link_libraries(tst_qml PRIVATE pbstats Qt6::Quick Qt6::QuickTest)
target_compile_definitions(tst_qml PRIVATE
  QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml")
add_test(NAME qml COMMAND tst_qml
  -import ${CMAKE_CURRENT_SOURCE_DIR}/qml/stubs)

# No display on a CI runner, and the app draws on e-ink anyway.
set_tests_properties(book_cover installer shim stats_bridge updater qml
  PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

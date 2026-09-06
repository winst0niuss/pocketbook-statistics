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

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Test QuickTest)
# Brings AUTOMOC and the rest with it; the Q_OBJECT classes under test need it.
qt_standard_project_setup()

set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
file(READ "${ROOT}/VERSION" APP_VERSION)
string(STRIP "${APP_VERSION}" APP_VERSION)

enable_testing()

# Everything under test, and nothing else: no fixture and no stub is compiled
# into this, so what the tests link is the app's own code. The two bundled
# amalgamations come with it, as they do in the device build, and so does the C
# core — the bridge calls straight into it, and its own asserts live in
# test/test_tracker.c.
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

# The shipped resource, read out of the app's own .qrc rather than listed again
# here: a QML file added there and forgotten here would be a screen the tests
# quietly stop covering.
file(READ ${ROOT}/qt/qml/pocketbook-statistics.qrc QRC_XML)
string(REGEX MATCHALL "<file>[^<]+</file>" QRC_ENTRIES "${QRC_XML}")
set(QRC_FILES "")
foreach(entry ${QRC_ENTRIES})
  string(REGEX REPLACE "</?file>" "" rel "${entry}")
  list(APPEND QRC_FILES ${ROOT}/qt/qml/${rel})
endforeach()
list(LENGTH QRC_FILES QRC_COUNT)
if(QRC_COUNT LESS 10)
  message(FATAL_ERROR "read only ${QRC_COUNT} files out of the .qrc — the parse above is wrong")
endif()
qt_add_resources(pbstats "app"
  PREFIX "/"
  BASE ${ROOT}/qt/qml
  FILES ${QRC_FILES})

# The scaffolding: the stub that stands in for inkview_bridge.cpp, and the
# fixtures. An object library rather than a static one, because pbstats leaves
# the inkView* calls undefined and a static archive listed after it would come
# too late on the link line.
qt_add_library(pbfixtures OBJECT
  ${CMAKE_CURRENT_SOURCE_DIR}/qt/inkview_stub.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/qt/fixtures.cpp
)
target_link_libraries(pbfixtures PUBLIC pbstats)

foreach(name book_cover installer shim stats_bridge updater)
  add_executable(tst_${name} qt/tst_${name}.cpp)
  target_link_libraries(tst_${name} PRIVATE pbfixtures)
  add_test(NAME ${name} COMMAND tst_${name})
endforeach()

# The screens, against the same bridge over a seeded database. They import the
# firmware's QML module, which exists only on the reader — test/qml/stubs is
# what stands in for it.
add_executable(tst_qml qml/main.cpp)
target_link_libraries(tst_qml PRIVATE pbfixtures Qt6::Quick Qt6::QuickTest)
target_compile_definitions(tst_qml PRIVATE
  QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml")
add_test(NAME qml COMMAND tst_qml
  -import ${CMAKE_CURRENT_SOURCE_DIR}/qml/stubs)

# No display on a CI runner, and the app draws on e-ink anyway.
set_tests_properties(book_cover installer shim stats_bridge updater qml
  PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

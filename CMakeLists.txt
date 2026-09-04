cmake_minimum_required(VERSION 3.21)
project(pocketbook-statistics CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The version the update check compares against the latest GitHub release tag.
# A release tag must match this file — CI enforces that on tag builds.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" APP_VERSION)
string(STRIP "${APP_VERSION}" APP_VERSION)

add_subdirectory(third_party/pocketbook-sdk-qt6)

pocketbook_add_app(PocketBookStatistics
  SOURCES
    qt/src/main.cpp
    qt/src/inkview_bridge.cpp
    qt/src/stats_bridge.cpp
    qt/src/book_cover.cpp
    qt/src/installer.cpp
    qt/src/shim.cpp
    qt/src/updater.cpp
    qt/src/update_log.cpp
    src/tracker.c
    src/stats_db.c
    src/daemon.c
    src/log.c
    src/version.c
    qt/third_party/sqlite3.c
    qt/third_party/miniz.c
  QRC qt/qml/pocketbook-statistics.qrc
)

target_include_directories(PocketBookStatistics PRIVATE qt/third_party src qt/src)
target_compile_definitions(PocketBookStatistics PRIVATE
  SQLITE_OMIT_LOAD_EXTENSION SQLITE_THREADSAFE=1
  APP_VERSION="${APP_VERSION}")
target_link_libraries(PocketBookStatistics PRIVATE m pthread dl)

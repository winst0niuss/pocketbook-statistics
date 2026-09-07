CFLAGS   := -O2 -Wall -Wextra -std=gnu99

# Cross-compiler image with the on-device Qt 6.8.2 headers and InkView stubs.
QT_IMG   := ghcr.io/fstanis/pocketbook-sdk-qt6-builder
# Device mount point for the dev deploy target (adjust to your reader's volume).
DEVICE   := /Volumes/NO NAME

# ---- One-time setup: fetch the PocketBook Qt6 SDK (sparse, ~2 MB) ----
# Clones fstanis/pocketbook-sdk-qt6 and its SDK submodule, checking out only the
# four files the build needs (inkview.h, hwconfig.h and two stub libs).
sdk:
	@test -d third_party/pocketbook-sdk-qt6 || \
	  git clone --depth 1 https://github.com/fstanis/pocketbook-sdk-qt6 \
	    third_party/pocketbook-sdk-qt6
	cd third_party/pocketbook-sdk-qt6 && \
	  git submodule update --init --depth 1 --filter=blob:none sdk && \
	  git -C sdk sparse-checkout set \
	    SDK-B288/usr/arm-obreey-linux-gnueabi/sysroot/usr/local

# ---- Host tests: tracker, aggregates, daemon liveness (no device needed) ----
# daemon.c is in here for the liveness check: a pidfile whose number belongs to
# some other process is how the daemon silently stops being started, and that
# is worth a test rather than a device.
test:
	mkdir -p build
	cc $(CFLAGS) -o build/test_tracker test/test_tracker.c \
	  src/tracker.c src/stats_db.c src/version.c src/log.c src/daemon.c -lsqlite3
	./build/test_tracker
	@python3 tools/check_qml.py
	@$(MAKE) --no-print-directory i18ncheck

# ---- Host tests for the Qt half: bridge, covers, installer, shim, updater ----
# Needs cmake and a host Qt 6 (any 6.x — the device's own 6.8.2 is only for the
# cross-build). Not part of `test`, because a checkout with neither still has
# to be able to run the C tests. CI runs both.
qt-test:
	cmake -S test -B build-host
	cmake --build build-host -j8
	ctest --test-dir build-host --output-on-failure

# ---- Build the app (single ELF, links the device's Qt at runtime) ----
qt:
	docker run --rm -v "$(CURDIR):/src" -w /src $(QT_IMG) bash -c '\
	  cmake -B build-qt -DCMAKE_TOOLCHAIN_FILE=/src/third_party/pocketbook-sdk-qt6/cmake/pocketbook.toolchain.cmake \
	  && cmake --build build-qt -j8'
	@echo "Built build-qt/PocketBookStatistics.app"

# ---- Dev deploy to a USB-mounted reader ----
deploy:
	cp build-qt/PocketBookStatistics.app "$(DEVICE)/applications/PocketBookStatistics.app"
	rm -f "$(DEVICE)/applications/._PocketBookStatistics.app"
	sync

# ---- Regenerate the launcher icons (needs Pillow) ----
icons:
	python3 tools/make_icon.py qt/qml

clean:
	rm -rf build build-qt build-host

.PHONY: sdk test qmlcheck i18ncheck qt-test qt deploy icons clean

# ---- Catalogs: the one part of the app nothing else compiles or runs ----
# A missing key falls back to English on the device and says nothing; a renamed
# placeholder renders as "{version}" mid-sentence. Known gaps live in
# qt/qml/i18n/untranslated.json — `--update-baseline` rewrites it.
i18ncheck:
	@command -v node >/dev/null || { echo "i18n: node not found, skipping"; exit 0; }; \
	  node tools/check_i18n.mjs

# ---- QML sanity: things qmllint lets through but the engine refuses ----
# A duplicated function or id stops the component being created, and the app
# just doesn't open — with no message anywhere, since there is no console.
qmlcheck:
	@python3 tools/check_qml.py

.PHONY: help get analyze test \
        run-android run-ios run-macos run-windows run-linux run-web \
        build-android build-macos build-windows build-linux build-web

ROOT := ..

help:
	@echo "layrz_wifi example — available targets:"
	@echo ""
	@echo "  get            flutter pub get"
	@echo "  analyze        dart analyze"
	@echo "  test           flutter test"
	@echo ""
	@echo "  run-android    flutter run on connected Android device"
	@echo "  run-ios        flutter run on connected iOS device"
	@echo "  run-macos      flutter run -d macos"
	@echo "  run-windows    flutter run -d windows"
	@echo "  run-linux      flutter run -d linux"
	@echo "  run-web        flutter run -d chrome"
	@echo ""
	@echo "  build-android  flutter build apk"
	@echo "  build-macos    flutter build macos"
	@echo "  build-windows  flutter build windows"
	@echo "  build-linux    flutter build linux"
	@echo "  build-web      flutter build web"
	@echo ""
	@echo "  Plugin shortcuts (delegates to root Makefile):"
	@echo "  pigeon         regenerate Pigeon bindings"
	@echo "  plugin-analyze dart analyze on the plugin"
	@echo "  plugin-test    flutter test on the plugin"

# ── dependencies ─────────────────────────────────────────────────────────────

get:
	flutter pub get

# ── code quality ─────────────────────────────────────────────────────────────

analyze:
	dart analyze

test:
	flutter test

# ── run ──────────────────────────────────────────────────────────────────────

run-android:
	flutter run

run-ios:
	flutter run

run-macos:
	flutter run -d macos

run-windows:
	flutter run -d windows

run-linux:
	flutter run -d linux

run-web:
	flutter run -d chrome

# ── build ────────────────────────────────────────────────────────────────────

build-android:
	flutter build apk

build-macos:
	flutter build macos

build-windows:
	flutter build windows

build-linux:
	flutter build linux

build-web:
	flutter build web

# ── plugin shortcuts ─────────────────────────────────────────────────────────

pigeon:
	$(MAKE) -C $(ROOT) pigeon

plugin-analyze:
	$(MAKE) -C $(ROOT) analyze

plugin-test:
	$(MAKE) -C $(ROOT) test

.PHONY: clean
clean:
	flutter clean
	flutter pub get

.PHONY: pod
pod:
	$(MAKE) -C macos pod
	$(MAKE) -C ios pod
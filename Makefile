.PHONY: help get analyze test pigeon \
        run-android run-ios run-macos run-windows run-linux run-web \
        build-android build-macos build-windows build-linux build-web \
        publish-dry publish

help:
	@echo "layrz_wifi plugin — available targets:"
	@echo ""
	@echo "  Development"
	@echo "    get            flutter pub get (root + example)"
	@echo "    analyze        dart analyze"
	@echo "    test           flutter test"
	@echo "    pigeon         regenerate Pigeon bindings from pigeons/messages.dart"
	@echo ""
	@echo "  Run example app"
	@echo "    run-android    flutter run on connected Android device"
	@echo "    run-ios        flutter run on connected iOS device"
	@echo "    run-macos      flutter run -d macos"
	@echo "    run-windows    flutter run -d windows"
	@echo "    run-linux      flutter run -d linux"
	@echo "    run-web        flutter run -d chrome"
	@echo ""
	@echo "  Build example (no device needed)"
	@echo "    build-android  flutter build apk"
	@echo "    build-macos    flutter build macos"
	@echo "    build-windows  flutter build windows"
	@echo "    build-linux    flutter build linux"
	@echo "    build-web      flutter build web"
	@echo ""
	@echo "  Publish"
	@echo "    publish-dry    flutter pub publish --dry-run"
	@echo "    publish        flutter pub publish --force"

# ── dependencies ─────────────────────────────────────────────────────────────

get:
	flutter pub get
	cd example && flutter pub get

# ── code quality ─────────────────────────────────────────────────────────────

analyze:
	dart analyze

test:
	flutter test

pigeon:
	dart run pigeon --input pigeons/messages.dart

# ── run example ──────────────────────────────────────────────────────────────

run-android run-ios run-macos run-windows run-linux run-web:
	$(MAKE) -C example $@

# ── build example ────────────────────────────────────────────────────────────

build-android build-macos build-windows build-linux build-web:
	$(MAKE) -C example $@

# ── publish ──────────────────────────────────────────────────────────────────

publish-dry:
	flutter pub publish --dry-run

publish:
	flutter pub publish --force

.PHONY: clean
clean:
	$(MAKE) -C example clean
	flutter clean
	flutter pub get

.PHONY: tag
tag:
	@echo "Tagging the current commit with the version from pubspec.yaml..."
	@VERSION=$$(grep 'version:' pubspec.yaml | head -n 1 | awk '{print $$2}' | cut -d+ -f1) && \
	git tag -a "v$$VERSION" -m "Release v$$VERSION" && \
	echo "Tagged with v$$VERSION"

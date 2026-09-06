# Platform scope

User-confirmed policy, 2026-09-05: require a 64-bit OS, at least 4 GB RAM, Vulkan support and at least 1 GB graphics memory. Android, iOS and Switch 2 remain on the table. Vita and the original Switch are excluded. Do not use Windows being the currently validated build as a reason to delete other platform sources.

## Repository correction

- Restored Android Gradle/Java/SDL2 project files, resources, JNI, asset/file access, startup/data selection, keyboard, audio, display-density and renderer hooks.
- Restored iOS resources, data picker and lifecycle hooks. Changed the retained device capability from ARMv7 to ARM64; Android now selects only ARM64 and x86-64 ABIs.
- Restored shared dependency/AppImage scripts and Emscripten hooks that were removed without a platform-specific decision. Preservation does not waive the hardware baseline or validate those builds.
- Preserved legacy Switch controller, keyboard, filesystem and lifecycle adapters as reference material. Their `__SWITCH__` guard and old SDK assumptions do not constitute Switch 2 support. The original Switch packaging setup exits with an explanation instead of advertising a supported build.
- Vita-only resources and adapters remain removed. Shared hooks retain other platforms' branches. SDL2 controller handle safety changes and headless error reporting remain intact.

## Port gaps requiring implementation and device validation

| Target | Remaining work |
| --- | --- |
| Android | Connect the retained Gradle/native build to the current C++ sources and dependencies; validate Vulkan loading, RAM and graphics-memory checks on actual devices; test storage permissions, touch, keyboard, lifecycle and gamepad input. Re-audit Android upstream commits against SDL2 rather than omitting them by platform. |
| iOS | Establish the current 64-bit build and Vulkan-compatible graphics integration, including loading/initialization and device-memory accounting; validate app lifecycle, data picker and touch on devices. The current hardware probe assumes desktop loader conventions outside Windows and is not an iOS implementation. |
| Switch 2 | Establish an appropriate toolchain and graphics/runtime integration; assess reusable parts of the retained original Switch adapter. Do not rename the old target and claim it is a working Switch 2 port. |
| Other retained targets | Check current toolchains and the same hardware baseline before treating any as supported releases. No blanket platform removal is authorized. |

The current correction preserves these options. It does not claim mobile or console build/runtime test coverage, and no new mobile or console binaries are being released.

## Validation of the correction

- Windows Release x64 rebuilt successfully with the restored conditional platform hooks. SDL2 game-controller initialization and the existing controller handle safety changes remain in the build.
- No deleted files remain under Android, iOS, retained Switch or shared CI paths. Android manifest and iOS plist parse; Android ABI selection is ARM64/x86-64 only.
- `Consul.svv` loaded and rendered for 3,000 frames without warnings/errors. Direct `Citizen.sav` validation correctly failed on a logged stale-surface-binding repair; that initial import is not a clean-load pass.
- Separate test copies of `Citizen.sav` and `Clerk c3.sav` passed canonical write/load, 3,000-frame render soak and strict reload. Initial repairs remain visible in `out/platform-scope-roundtrip.stderr.log`; canonical loads and soaks emitted no warnings/errors. Original saves were untouched. Evidence: `out/platform-scope-soak.stdout.log` and `out/platform-scope-roundtrip.stdout.log`.
- These checks use the installed game data and validate the platform correction; they do not close the ongoing D08/editor/UI work or substitute for its final integration gate. No commit, ancestry marker or installation deployment was made for this correction.

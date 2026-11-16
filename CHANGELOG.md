# Changelog

## v1.1.0-compat.1 — 2026-11-16 (prerelease)

Experimental universal compatibility candidate, app build **12006**. Target range: iOS 5–27; this is not a claim that every device or system has been tested. Stable v1.0.0 is unchanged.

- One FAT32 IPA executable with armv7 / iOS 5 and arm64 / iOS 7 slices.
- OpenGL ES 2 renderer for legacy systems; retain native Metal and CAMetalDisplayLink on modern systems. CADisplayLink provides the fallback scheduler.
- Share gameplay, vertex layout, atlas decoding, interpolation and full-screen composition between renderers. Extend skyline and ground tiles horizontally on 3:2 phones and 3:4 iPads without moving gameplay objects.
- Preloaded AVAudioPlayer fallback before iOS 8, keeping playback work on the audio queue.
- AppDelegate window startup before iOS 13, guarded modern APIs, weak framework imports, legacy launch images/icons and an iOS 8+ launch storyboard.
- Pinned legacy SDK/C++ headers bootstrap with SHA-256 checks; compile ARC objects separately to avoid libarclite injection. Locally provide availability checks using iOS 5 APIs.
- Restore historical NSObject/C++ symbol provider libraries in the armv7 linker stubs; all 200 strong armv7 imports were checked against iOS 5.0 SDK exports. A supplementary iOS 7.1 SDK check covered all 155 strong arm64 imports (not an iOS 7.0 runtime test).
- Audit actual Mach-O entry points, minimum versions, imports, resources and absence of chained fixups. CI builds and audits the universal IPA.
- Host regressions passed. Desktop GL/Metal readback comparison: max channel difference 1/255, no channels differ by more than 2/255; ground occlusion passed. These desktop checks do not verify iPhone GLES drivers.
- **Pending:** installation, startup, gameplay, sound and resume on the owner's jailbroken iPhone 4S / iOS 6.1.3, plus modern-device regression testing. iOS 5 and iPad remain untested.

## v1.0.0 — 2026-11-16

First public release: an arm64-native, Metal-rendered revival targeting iOS 27. Game display version: **1.2**. Native application build: **12005**.

### Native runtime

- arm64 Objective-C++ application for iOS 17 and later.
- Metal rendering directly into a full-screen `CAMetalLayer`.
- `CAMetalDisplayLink` presentation scheduling with a preferred maximum of 120Hz.
- Fixed 60Hz gameplay simulation with interpolated render poses.
- Stable pipe identities preserve continuous motion during queue recycling and viewport entry.
- Three preallocated GPU vertex buffers, one render pass, and no GPU wait in the display callback.
- Predecoded stereo float PCM effects scheduled through three `AVAudioEngine` player nodes on a dedicated audio queue.

### Gameplay and adaptation

- Original atlas, sound effects, menus, scoring, local best score and medal thresholds.
- Full-screen sky and ground extensions preserve the core playfield's aspect ratio.
- Foreground ground fill hides lower pipes all the way to the screen bottom.
- Viewport-relative upper flight limit permits fully offscreen flight, finite ascent and natural recovery.
- Extended upper-pipe rendering and collision bounds prevent bypassing pipes above the screen.
- Actual presented-frame FPS and P99 frame-interval display, toggled with a two-finger double tap.

### Validation

- Host-side physics, state, layout, offscreen-flight, motion-continuity, telemetry and audio-decoding regression checks.
- Two 4000-frame pipe-motion traces at fixed and variable refresh rates.
- Desktop Metal offscreen previews and ground-occlusion pixel checks.
- Device build and IPA/resource-integrity checks.
- Project owner's iPhone 17 Pro Max feedback confirms launch/playability and a substantial smoothness improvement. No universal locked-120FPS claim is made.

### Known gaps

- Original developer's Game Center leaderboard and advertising services are unavailable in this independent build.
- Medal sparkle effects and pixel-for-pixel equivalence to the original multipass renderer remain incomplete.
- Broader device, thermal, low-power, audio-route and long-session validation is still needed.

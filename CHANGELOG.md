# Changelog

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

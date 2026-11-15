#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

// Translation of Flappy Bird 1.2 ARMv7 functions 0x108A8, 0x11194,
// 0x11234 and 0x1134C. This is a component, not a complete game.
// Coordinates are collision-box top-left, in the original 288x512 canvas.
// Keep binary32 arithmetic and integer truncation; do not integrate with dt.
namespace flappy12 {
struct OriginalBird {
    static constexpr int width = 20, height = 20;
    static constexpr int spriteOffsetX = 14, spriteOffsetY = 14;
    // The 48px sprite rotates around collision-top + 10. Its maximum bottom
    // extent is ceil(10 + 24*sqrt(2)) = 44, including the transparent padding.
    static constexpr int maxSpriteBottom = 44;
    static constexpr int upperFlightMargin = 64;
    static constexpr int floorY = 400;
    int x = 80, y = 246;
    float velocity = 0, gravity = 1;
    float angle = 0, angularVelocity = 0, angularAcceleration = 0.4f;
    bool hit = false, hovering = true;
    int hoverPhase = 0;
    float hoverOffset = 0;
    // Zero preserves the original game; the full-screen shell configures this
    // relative to the visible viewport. A reset intentionally keeps the policy.
    int flapCeiling = 0;

    // 0x108A8 does not write angularVelocity; the next flap sets it.
    // Bird colour and animation selection belong to the caller.
    void reset() {
        x = 80; y = 246; velocity = 0; gravity = 1;
        angle = 0; angularAcceleration = 0.4f;
        hit = false; hovering = true; hoverPhase = 0;
    }

    // Returns whether the caller should reset flap animation and queue event 4
    // with delay 5. Leaving hover happens before the rejection checks.
    bool flap() {
        hovering = false;
        if (y < flapCeiling || hit) return false;
        velocity = -5.0f;
        gravity = 0.3f;             // ARM immediate 0x3E99999A
        angularVelocity = -10.0f;  // ARM immediate 0xC1200000
        angularAcceleration = 0.4f;
        return true;
    }

    // Called once per original display tick (60 Hz on the original device).
    // The original updates wing animation immediately before this function.
    void tick() {
        if (hovering) {
            hoverPhase += 8;
            if (hoverPhase == 360) hoverPhase = 0;
            // Match 0xE920's table construction: double expression, then f32.
            float radians = static_cast<float>(hoverPhase * 3.1415926 / 180.0);
            hoverOffset = std::sin(radians) * 4.0f;
            return;
        }
        hoverOffset = 0;
        velocity = std::min(8.0f, velocity + gravity);
        y = static_cast<int>(static_cast<float>(y) + velocity);
        if (y > floorY - height) {
            y = floorY - height;
            velocity = 0;
            gravity = 0;
        }
        angle += angularVelocity;
        angularVelocity += angularAcceleration;
        angle = std::clamp(angle, -20.0f, 90.0f);
    }

    // Original rendering truncates angle and the combined hover offset.
    int spriteX() const { return x - spriteOffsetX; }
    int spriteY() const {
        return static_cast<int>(static_cast<float>(y - spriteOffsetY) + hoverOffset);
    }
    int spriteAngle() const { return static_cast<int>(angle); }

    // 0x10B18: axis-aligned, inclusive edges, unaffected by sprite rotation.
    bool overlapsPipe(int pipeX, int pipeTop, int pipeHeight = 320) const {
        return x + width >= pipeX && x <= pipeX + 52 &&
               y + height >= pipeTop && y <= pipeTop + pipeHeight;
    }
    bool overlapsPair(int pipeX, int lowerPipeTop) const {
        int upperBottom=lowerPipeTop-96;
        int upperTop=upperBottom-320;
        if(flapCeiling<0)upperTop=std::min(upperTop,flapCeiling-upperFlightMargin);
        return overlapsPipe(pipeX, lowerPipeTop) ||
               overlapsPipe(pipeX,upperTop,upperBottom-upperTop);
    }
};
} // namespace flappy12

#include "../src/Core/OriginalBird.hpp"
#include <cassert>
#include <iostream>

int main() {
    using flappy12::OriginalBird;
    OriginalBird b;
    assert(b.flap());
    // Short trace hand-checked against the ARM instructions: VADD.F32,
    // followed by VCVT.S32.F32 after adding the integer position.
    const int expected[] = {241,236,231,227,223};
    for (int y : expected) { b.tick(); assert(b.y == y); }
    assert(b.angle == -20.0f);
    b.y = -1;
    assert(!b.flap());
    b.reset(); b.hit = true;
    assert(!b.flap());
    b.reset(); b.hovering = false; b.y = -1; b.velocity = -0.3f;
    b.gravity = 0.1f; b.tick();
    assert(b.y == -1); // Truncate toward zero, not floor.
    b.y = 379; b.velocity = 8; b.gravity = 0.3f; b.tick();
    assert(b.y == 380 && b.velocity == 0 && b.gravity == 0);
    b.y = 380; b.velocity = 0; b.gravity = 0.3f; b.tick();
    assert(b.gravity == 0.3f); // Ground clamp uses >, not >=.
    b.reset();
    for (int i = 0; i < 45; ++i) b.tick();
    assert(b.hoverPhase == 0 && b.y == 246);
    b.x = 80; b.y = 200;
    assert(b.overlapsPipe(100,220)); // Exact edge contact collides.
    assert(!b.overlapsPipe(101,220));
    b.y = 179;
    assert(!b.overlapsPair(80,274)); // Gap is (178,274).
    b.y = 178;
    assert(b.overlapsPair(80,274));
    std::cout << "OriginalBird component checks passed\n";
}

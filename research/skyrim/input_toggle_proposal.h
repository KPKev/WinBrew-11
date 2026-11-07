/* Isolated proposal, not wired into the launcher. */
#ifndef SKYRIM_INPUT_TOGGLE_H
#define SKYRIM_INPUT_TOGGLE_H
#include <stdint.h>
/* Reserved everywhere: touchpad click + L3 + R3. Avoids Options+Create exit. */
#define ST_CHORD (0x100000u | 0x2u | 0x4u)
#define ST_CROSS 0x4000u
#define ST_TOGGLE_TICKS 60u

typedef struct SkyrimToggle {
    unsigned pointer_mode, chord_ticks, chord_latched;
    unsigned click_down, retry_release, cross_armed, transition_gate;
} SkyrimToggle;
typedef struct SkyrimDecision {
    unsigned pointer_mode, gamepad_neutral, click_press, click_release;
    uint32_t game_buttons; /* reserved controls are never sent */
} SkyrimDecision;
/* Call once per actual pad sample, including disconnect. Callback returns zero
 * only if the Wine queue accepted the mouse event. While release is pending,
 * remain neutral and retry; never assert a new click. */
static SkyrimDecision skyrim_toggle_step(SkyrimToggle *s, uint32_t buttons,
                                         int connected, int (*mouse)(int))
{
    SkyrimDecision d = {0};
    if (!connected) buttons = 0;
    if ((buttons & ST_CHORD) == ST_CHORD) {
        if (!s->chord_latched && ++s->chord_ticks >= ST_TOGGLE_TICKS) {
            s->chord_latched = 1;
            s->pointer_mode = !s->pointer_mode;
            s->cross_armed = 0; s->transition_gate = 1;
            if (s->click_down) s->retry_release = 1;
        }
    } else { s->chord_ticks = 0; s->chord_latched = 0; }
    if ((!connected || !s->pointer_mode || !(buttons & ST_CROSS)) && s->click_down)
        s->retry_release = 1;
    if (s->retry_release && mouse && mouse(0) == 0) {
        s->retry_release = 0; s->click_down = 0; d.click_release = 1;
    }
    if (!buttons && !s->retry_release) s->transition_gate = 0;
    if (!(buttons & ST_CROSS) && !s->transition_gate) s->cross_armed = 1;
    if (s->pointer_mode && connected && !s->retry_release && !s->transition_gate && s->cross_armed &&
        (buttons & ST_CROSS) && !s->click_down && mouse && mouse(1) == 0) {
        s->click_down = 1; d.click_press = 1;
    }
    d.pointer_mode = s->pointer_mode;
    d.gamepad_neutral = s->pointer_mode || s->retry_release || s->transition_gate;
    d.game_buttons = d.gamepad_neutral ? 0 : buttons & ~ST_CHORD;
    return d;
}
#endif

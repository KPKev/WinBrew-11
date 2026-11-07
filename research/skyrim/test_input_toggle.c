#include "input_toggle_proposal.h"
#include <assert.h>
#include <stdio.h>
static int fail_release, presses, releases;
static int mouse(int down) { if (!down && fail_release) return -1; if (down) presses++; else releases++; return 0; }
static SkyrimDecision hold(SkyrimToggle *s, uint32_t b) { SkyrimDecision d; for (int i=0;i<60;i++) d=skyrim_toggle_step(s,b,1,mouse); return d; }
int main(void) {
 SkyrimToggle s={0}; SkyrimDecision d;
 d=skyrim_toggle_step(&s,ST_CHORD & ~0x4u,1,mouse); assert(!d.game_buttons && !d.pointer_mode);
 d=hold(&s,ST_CHORD); assert(d.pointer_mode && d.gamepad_neutral);
 d=hold(&s,ST_CHORD); assert(d.pointer_mode); /* one toggle per hold */
 d=skyrim_toggle_step(&s,0,1,mouse); assert(d.pointer_mode);
 d=skyrim_toggle_step(&s,ST_CROSS,1,mouse); assert(d.click_press && !d.game_buttons);
 fail_release=1; d=hold(&s,ST_CHORD | ST_CROSS); assert(!d.pointer_mode && d.gamepad_neutral && s.retry_release);
 d=skyrim_toggle_step(&s,0,1,mouse); assert(d.gamepad_neutral && s.retry_release);
 fail_release=0; d=skyrim_toggle_step(&s,0,1,mouse); assert(d.click_release && !d.gamepad_neutral);
 assert(presses==1 && releases==1);
 d=hold(&s,ST_CHORD | ST_CROSS); assert(d.pointer_mode && !d.click_press);
 d=skyrim_toggle_step(&s,0,1,mouse); d=skyrim_toggle_step(&s,ST_CROSS,1,mouse); assert(d.click_press);
 d=skyrim_toggle_step(&s,0,0,mouse); assert(d.click_release && d.gamepad_neutral);
 puts("isolated toggle proposal tests passed"); return 0;
}

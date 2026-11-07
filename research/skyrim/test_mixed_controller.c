/* Host-only test against the preserved launcher source; no PS5 runtime. */
#include "../../build/return-wrapper/source/native/pw_wine_display.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    static const char profile[] = "[input]\nmode = xinput\nmouse = right_stick\ncross = mouse_left\n";
    PwGameInput input;
    PwPadPs5 pad = {0};
    PwWinePad state;
    PwWineInput event;
    PwWinePointer pointer = {0};
    pw_game_input_init(&input);
    assert(pw_game_input_parse((const uint8_t *)profile, strlen(profile), &input) == PW_OK);
    pw_game_input_default_mode(&input);
    assert(input.mode == PW_GAME_INPUT_XINPUT && input.mouse == PW_GAME_STICK_RIGHT);

    pad.core.connected = 1;
    pad.core.previous_buttons = 0x4000u; /* Cross */
    pad.left_stick.x = pad.left_stick.y = 0x80;
    pad.right_stick.x = 0xff; pad.right_stick.y = 0x80;
    assert(pw_wine_game_pad(&pad, &state) == 1);
    assert(state.connected && (state.buttons & 0x1000u) && state.thumb_rx == 32767);
    assert(pw_wine_game_inputs(&input, 0x4000u, 0, &event, 1) == 1);
    assert(event.type == PW_WINE_INPUT_MOUSE_BUTTON && event.code == 0 && event.down == 1);
    assert(pw_wine_game_inputs(&input, 0, 0x4000u, &event, 1) == 1);
    assert(event.type == PW_WINE_INPUT_MOUSE_BUTTON && event.code == 0 && event.down == 0);
    assert(pw_wine_pointer_step(&pointer, pad.right_stick.x, pad.right_stick.y,
                                input.mouse_speed, 1000000, &event) == 1);
    assert(event.type == PW_WINE_INPUT_MOUSE_MOVE && event.x > 0 && event.y == 0);
    puts("mixed XInput and pointer profile passed; simultaneous inputs are intentional");
    return 0;
}

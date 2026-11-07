#include "pw_blender_compat.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    assert(pw_blender_target(365670,"Z:\\data\\wine11\\wine11-games\\365670\\blender.exe"));
    assert(pw_blender_target(365670,"D:/Games/Art/BLENDER.EXE"));
    assert(!pw_blender_target(730,"D:/blender.exe"));
    assert(!pw_blender_target(365670,"D:/notblender.exe"));
    assert(!pw_blender_target(365670,NULL));
    const char *args[12]={"wine","D:/blender.exe","My Scene.blend","--debug-gpu",NULL};
    int n=pw_blender_default_backend(args,4,12);
    assert(n==6 && !strcmp(args[2],"My Scene.blend") && !strcmp(args[3],"--debug-gpu"));
    assert(!strcmp(args[4],"--gpu-backend") && !strcmp(args[5],"vulkan") && !args[6]);
    assert(pw_blender_default_backend(args,n,12)==n);
    const char *explicit_args[]={"wine","blender.exe","--gpu-backend","opengl",NULL};
    assert(pw_blender_default_backend(explicit_args,4,5)==4);
    const char *equals_args[]={"wine","blender.exe","--gpu-backend=opengl",NULL};
    assert(pw_blender_default_backend(equals_args,3,4)==3);
    const char *script[9]={"wine","blender.exe","--","--gpu-backend","custom-script-value",NULL};
    assert(pw_blender_default_backend(script,5,9)==7);
    assert(!strcmp(script[2],"--gpu-backend") && !strcmp(script[3],"vulkan") && !strcmp(script[4],"--"));
    assert(!strcmp(script[6],"custom-script-value") && !script[7]);
    const char *full[]={"wine","blender.exe","--debug-gpu",NULL};
    assert(pw_blender_default_backend(full,3,4)==-1 && !strcmp(full[2],"--debug-gpu") && !full[3]);
    puts("PASS: portable Blender identity, explicit backend, preserved arguments, script boundary and capacity");
}

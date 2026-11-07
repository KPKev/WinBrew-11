#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef uint32_t UINT32;typedef unsigned UINT;typedef int INT;
typedef struct{int x,y;}POINT;typedef struct{int left,top,right,bottom;}RECT;
static int vulkan_window;static RECT vulkan_client;static UINT32 pixel,*cursor_image;static UINT cursor_width,cursor_height;static POINT cursor_hotspot,cursor;
static unsigned gpu_calls,gdi_calls;static int seen_x,seen_y;static const UINT32*seen_image;
static int show(const UINT32*i,UINT w,UINT h,INT hx,INT hy,INT x,INT y,UINT cw,UINT ch){(void)w;(void)h;(void)hx;(void)hy;(void)cw;(void)ch;gpu_calls++;seen_x=x;seen_y=y;seen_image=i;return 0;}
static int(*vulkan_cursor_show)(const UINT32*,UINT,UINT,INT,INT,INT,INT,UINT,UINT);
static void present_screen_for_change(void){gdi_calls++;}
static void present_pointer_change(void)
{
#ifdef SONAME_LIBVULKAN
    if (vulkan_cursor_show && vulkan_window &&
        vulkan_client.right > vulkan_client.left && vulkan_client.bottom > vulkan_client.top)
    {
        vulkan_cursor_show( cursor_width && cursor_height ? cursor_image : NULL, cursor_width, cursor_height,
                            cursor_hotspot.x, cursor_hotspot.y,
                            cursor.x - vulkan_client.left, cursor.y - vulkan_client.top,
                            vulkan_client.right - vulkan_client.left, vulkan_client.bottom - vulkan_client.top );
        return;
    }
#endif
    present_screen_for_change();
}

int main(void){
vulkan_window=1;vulkan_client=(RECT){100,50,1380,770};cursor=(POINT){130,80};cursor_hotspot=(POINT){2,3};cursor_width=cursor_height=32;cursor_image=&pixel;vulkan_cursor_show=show;
present_pointer_change();assert(gpu_calls==1&&gdi_calls==0&&seen_x==30&&seen_y==30&&seen_image==&pixel);
/* No application present occurs between these mouse updates. */
cursor=(POINT){170,100};present_pointer_change();assert(gpu_calls==2&&seen_x==70&&seen_y==50&&gdi_calls==0);
cursor_image=0;cursor_width=cursor_height=0;present_pointer_change();assert(gpu_calls==3&&seen_image==0&&gdi_calls==0);
vulkan_window=0;present_pointer_change();assert(gdi_calls==1&&gpu_calls==3);
vulkan_window=1;vulkan_client.right=vulkan_client.left;present_pointer_change();assert(gdi_calls==2&&gpu_calls==3);
vulkan_client.right=1380;vulkan_cursor_show=0;present_pointer_change();assert(gdi_calls==3&&gpu_calls==3);
puts("6 independent cursor and fallback checks passed");return 0;}

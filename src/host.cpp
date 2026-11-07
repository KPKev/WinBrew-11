// SPDX-License-Identifier: GPL-3.0-or-later
#include "shell.hpp"
#include "owner_check_dispatch.hpp"
#include <cstdio>
#include <cstring>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
// shellapi requires the Win32 type declarations above.
#include <shellapi.h>
static ww::Shell shell;
static void paint(HWND hwnd) {
  shell.tick();
  shell.draw();
  HDC dc = GetDC(hwnd);
  RECT r;
  GetClientRect(hwnd, &r);
  // Win32 DIB is BGRA. The PS5 framebuffer is RGBA; convert only at the host
  // boundary.
  static uint32_t dib[ww::W * ww::H];
  for (int i = 0; i < ww::W * ww::H; i++) {
    auto c = shell.pixels[i];
    dib[i] = (c & 0xff00ff00) | ((c & 255) << 16) | ((c >> 16) & 255);
  }
  BITMAPINFO info{};
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = ww::W;
  info.bmiHeader.biHeight = -ww::H;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  StretchDIBits(dc, 0, 0, r.right, r.bottom, 0, 0, ww::W, ww::H, dib, &info,
                DIB_RGB_COLORS, SRCCOPY);
  ReleaseDC(hwnd, dc);
}
static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l) {
  if (m == WM_DESTROY) {
    PostQuitMessage(0);
    return 0;
  }
  if (m == WM_PAINT) {
    PAINTSTRUCT p;
    BeginPaint(h, &p);
    EndPaint(h, &p);
    paint(h);
    return 0;
  }
  if (m == WM_TIMER) {
    paint(h);
    return 0;
  }
  if (m == WM_KEYDOWN) {
    uint8_t modifiers=(GetKeyState(VK_SHIFT)<0?2:0)|(GetKeyState(VK_CONTROL)<0?1:0)|(GetKeyState(VK_MENU)<0?4:0);
    uint16_t usage=0;
    switch(w) {
    case VK_F1:usage=0x3a;break;case VK_F2:usage=0x3b;break;
    case VK_ESCAPE:usage=0x29;break;case VK_TAB:usage=0x2b;break;
    case VK_LEFT:usage=0x50;break;case VK_RIGHT:usage=0x4f;break;
    case VK_UP:usage=0x52;break;case VK_DOWN:usage=0x51;break;
    case VK_RETURN:usage=0x28;break;case VK_BACK:usage=0x2a;break;
    case VK_DELETE:usage=0x4c;break;case VK_HOME:usage=0x4a;break;case VK_END:usage=0x4d;break;
    case VK_PRIOR:usage=0x4b;break;case VK_NEXT:usage=0x4e;break;
    case 'L':if(modifiers&1) usage=0x0f;break;
    case 'A':if(modifiers&1) usage=4;break;
    case 'S':if(modifiers&1) usage=0x16;break;
    case 'W':if(modifiers&1) usage=0x1a;break;
    case 'Y':if(modifiers&1) usage=0x1c;break;
    case 'Z':if(modifiers&1) usage=0x1d;break;
    }
    if(usage) shell.physical_key(usage,modifiers,0);
  }
  if (m == WM_CHAR && w >= 32 && w < 127) {
    shell.input_device=ww::InputDevice::keyboard;
    shell.character(char(w));
  }
  if (m == WM_MOUSEMOVE || m == WM_LBUTTONDOWN || m == WM_LBUTTONUP ||
      m == WM_LBUTTONDBLCLK) {
    shell.input_device=ww::InputDevice::mouse;
    RECT r;
    GetClientRect(h, &r);
    shell.pointer(short(LOWORD(l)) * ww::W / r.right,
                  short(HIWORD(l)) * ww::H / r.bottom,
                  m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK,
                  (w & MK_LBUTTON) != 0, m == WM_LBUTTONDBLCLK);
    if (m == WM_LBUTTONDOWN)
      SetCapture(h);
    if (m == WM_LBUTTONUP)
      ReleaseCapture();
  }
  if (m == WM_RBUTTONDOWN) {shell.input_device=ww::InputDevice::mouse;shell.open_context();}
  ww::dispatch_owner_check(shell, false, nullptr);
  if (shell.pending == ww::Action::browser) {
    shell.pending = ww::Action::none;
    auto result = reinterpret_cast<INT_PTR>(
        ShellExecuteA(h, "open", shell.url, nullptr, nullptr, SW_SHOWNORMAL));
    if (result <= 32)
      shell.result(false, uint32_t(result), "browser");
    else
      shell.web_history.remember(shell.url);
  }
  if (shell.pending != ww::Action::none) {
    shell.pending = ww::Action::none;
    shell.dialog = true;
    std::snprintf(shell.message, sizeof(shell.message),
                  "This preview cannot launch PS5 apps.");
  }
  if (m == WM_LBUTTONDBLCLK || m == WM_KEYDOWN || m == WM_CHAR ||
      m == WM_MOUSEMOVE || m == WM_LBUTTONDOWN || m == WM_RBUTTONDOWN)
    paint(h);
  return DefWindowProc(h, m, w, l);
}
#endif
int main(int argc, char **argv) {
#ifndef _WIN32
  ww::Shell shell;
#endif
  if (!shell.init("assets", nullptr, nullptr, nullptr,
                  "build/desktop-settings.ini")) {
    std::fprintf(stderr, "Missing assets. Run from the WinBrew 11 folder.\n");
    return 1;
  }
  if (argc > 1) {
    if (!std::strcmp(argv[1], "start"))
      shell.start = true;
    if (!std::strcmp(argv[1], "library"))
      shell.activate(35);
    if (!std::strcmp(argv[1], "about")) {shell.window=2;shell.settings_page=4;}
    if (!std::strcmp(argv[1], "owner-check-pass")) {shell.window=2;shell.settings_page=4;shell.owner_check_status=8;}
    if (!std::strcmp(argv[1], "owner-check-busy")) {shell.window=2;shell.settings_page=4;shell.owner_check_status=3;}
    if (!std::strcmp(argv[1], "mouse")) {shell.window=2;shell.settings_page=3;}
    if (!std::strcmp(argv[1], "quick")) {shell.quick=true;shell.audio_available=true;shell.network_state=3;std::snprintf(shell.network_name,sizeof(shell.network_name),"Preview network");}
    if (!std::strcmp(argv[1], "tray")) {shell.tray=true;shell.window=2;}
    if (!std::strcmp(argv[1], "settings"))
      shell.window = 2;
    if (!std::strcmp(argv[1], "personalization")) {
      shell.window = 2;
      shell.settings_page = 1;
    }
    if (!std::strcmp(argv[1], "browser"))
      shell.window = 3;
    if (!std::strcmp(argv[1], "keyboard")) {
      shell.window = 3;
      shell.begin_text(ww::TextTarget::address);
    }
    if (!std::strcmp(argv[1], "search-keyboard")) shell.begin_text(ww::TextTarget::search);
    if (!std::strcmp(argv[1], "keyboard-dark")) {shell.window=3;shell.dark=true;shell.begin_text(ww::TextTarget::address);}
    if (!std::strcmp(argv[1], "input")) {shell.window=2;shell.settings_page=6;shell.keyboard_connected=shell.mouse_connected=true;}
    if (!std::strcmp(argv[1], "ps5-controller")) {shell.window=2;shell.settings_page=7;}
    if (!std::strcmp(argv[1], "controller-test")) {shell.window=2;shell.settings_page=8;shell.pad_connected=true;shell.pad_lx=174;shell.pad_ry=84;shell.pad_buttons=0x4000;}
    if (!std::strcmp(argv[1], "dark")) {
      shell.window = 2;
      shell.settings_page = 1;
      shell.dark = true;
    }
    if (!std::strcmp(argv[1], "calendar"))
      shell.calendar = true;
    shell.draw();
    FILE *f = std::fopen(argc > 2 ? argv[2] : "build/preview.ppm", "wb");
    if (!f)
      return 2;
    std::fprintf(f, "P6\n%d %d\n255\n", ww::W, ww::H);
    for (int i = 0; i < ww::W * ww::H; i++)
      std::fwrite(shell.pixels + i, 1, 3, f);
    std::fclose(f);
    return 0;
  }
#ifdef _WIN32
  shell.keyboard_connected=shell.mouse_connected=true;
  shell.input_device=ww::InputDevice::mouse;
  auto instance = GetModuleHandle(nullptr);
  WNDCLASS wc{};
  wc.style = CS_DBLCLKS;
  wc.lpfnWndProc = proc;
  wc.hInstance = instance;
  wc.lpszClassName = "WinBrew 11";
  RegisterClass(&wc);
  HWND window = CreateWindow("WinBrew 11", "WinBrew 11 - PC Preview",
                             WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT,
                             CW_USEDEFAULT, 1440, 850, nullptr, nullptr,
                             instance, nullptr);
  SetTimer(window, 1, 1000, nullptr);
  MSG msg;
  while (GetMessage(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  return 0;
#else
  std::puts(
      "Use a screenshot argument or build the interactive Windows preview.");
  return 0;
#endif
}

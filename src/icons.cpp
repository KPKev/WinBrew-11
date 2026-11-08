// SPDX-License-Identifier: GPL-3.0-or-later
#include "shell.hpp"
#include "image_codec.hpp"
#include <cstring>
#include <algorithm>
namespace ww {
bool Shell::vapor_game_cover(std::uint32_t app, Rect area) {
  if (!steam || !app || area.w <= 0 || area.h <= 0) return false;
  constexpr int cw = 184, ch = 86;
  const VaporGameCover* cached = nullptr;
  for (const auto& entry : vapor_game_covers) if (entry.app == app) { cached = &entry; break; }
  if (!cached) {
    steam->request_game_cover(app);
    std::vector<std::uint8_t> bytes;
    if (steam->take_game_cover(app, bytes)) {
      if (vapor_icon_decode.empty()) vapor_icon_decode.resize(image::MaxPixels);
      int width = 0, height = 0;
      if (image::read_memory(bytes.data(), bytes.size(), vapor_icon_decode.data(), width, height) && width > 0 && height > 0) {
        VaporGameCover entry; entry.app = app; entry.pixels.resize(cw * ch);
        for (int py = 0; py < ch; ++py) for (int px = 0; px < cw; ++px)
          entry.pixels[py * cw + px] = vapor_icon_decode[(py * height / ch) * width + px * width / cw];
        if (vapor_game_covers.size() >= 64) vapor_game_covers.erase(vapor_game_covers.begin());
        vapor_game_covers.push_back(std::move(entry)); cached = &vapor_game_covers.back();
      } else steam->reject_game_cover(app);
    }
  }
  if (!cached) return false;
  for (int py = 0; py < area.h; ++py) for (int px = 0; px < area.w; ++px) {
    const int xx = area.x + px, yy = area.y + py;
    if (xx >= 0 && yy >= 0 && xx < W && yy < H)
      pixels[yy * W + xx] = cached->pixels[(py * ch / area.h) * cw + px * cw / area.w] | 0xff000000u;
  }
  return true;
}
bool Shell::vapor_game_portrait(std::uint32_t app, Rect area) {
  if (!steam || !app || area.w <= 0 || area.h <= 0) return false;
  constexpr int cw = 128, ch = 192;
  const VaporGameCover* cached = nullptr;
  for (const auto& entry : vapor_game_portraits) if (entry.app == app) { cached = &entry; break; }
  if (!cached) {
    steam->request_game_portrait(app);
    std::vector<std::uint8_t> bytes;
    if (steam->take_game_portrait(app, bytes)) {
      if (vapor_icon_decode.empty()) vapor_icon_decode.resize(image::MaxPixels);
      int width = 0, height = 0;
      if (image::read_memory(bytes.data(), bytes.size(), vapor_icon_decode.data(), width, height) && width > 0 && height > 0) {
        VaporGameCover entry; entry.app = app; entry.pixels.assign(cw * ch, 0xff30251e);
        const int fit_w = width * ch > height * cw ? cw : std::max(1, width * ch / height);
        const int fit_h = width * ch > height * cw ? std::max(1, height * cw / width) : ch;
        const int ox = (cw - fit_w) / 2, oy = (ch - fit_h) / 2;
        for (int py = 0; py < fit_h; ++py) for (int px = 0; px < fit_w; ++px)
          entry.pixels[(oy + py) * cw + ox + px] = vapor_icon_decode[(py * height / fit_h) * width + px * width / fit_w];
        if (vapor_game_portraits.size() >= 64) vapor_game_portraits.erase(vapor_game_portraits.begin());
        vapor_game_portraits.push_back(std::move(entry)); cached = &vapor_game_portraits.back();
      } else steam->reject_game_portrait(app);
    }
  }
  if (!cached) return false;
  for (int py = 0; py < area.h; ++py) for (int px = 0; px < area.w; ++px) {
    const int xx = area.x + px, yy = area.y + py;
    if (xx >= 0 && yy >= 0 && xx < W && yy < H)
      pixels[yy * W + xx] = cached->pixels[(py * ch / area.h) * cw + px * cw / area.w] | 0xff000000u;
  }
  return true;
}
bool Shell::vapor_game_icon(std::uint32_t app, int x, int y, int size) {
  if (!steam || !app || size <= 0) return false;
  const VaporGameIcon* cached = nullptr;
  for (const auto& entry : vapor_game_icons) if (entry.app == app) { cached = &entry; break; }
  if (!cached) {
    steam->request_game_icon(app);
    std::vector<std::uint8_t> bytes;
    if (steam->take_game_icon(app, bytes)) {
      if (vapor_icon_decode.empty()) vapor_icon_decode.resize(image::MaxPixels);
      int width = 0, height = 0;
      if (image::read_memory(bytes.data(), bytes.size(), vapor_icon_decode.data(), width, height) && width > 0 && height > 0) {
        VaporGameIcon entry; entry.app = app;
        const int side = width < height ? width : height;
        const int offset_x = (width - side) / 2, offset_y = (height - side) / 2;
        for (int py = 0; py < 64; ++py) for (int px = 0; px < 64; ++px)
          entry.pixels[py * 64 + px] = vapor_icon_decode[(offset_y + py * side / 64) * width + offset_x + px * side / 64];
        if (vapor_game_icons.size() >= 128) vapor_game_icons.erase(vapor_game_icons.begin());
        vapor_game_icons.push_back(entry); cached = &vapor_game_icons.back();
      } else steam->reject_game_icon(app);
    }
  }
  if (!cached) return false;
  for (int py = 0; py < size; ++py) for (int px = 0; px < size; ++px) {
    const int xx = x + px, yy = y + py;
    if (xx < 0 || yy < 0 || xx >= W || yy >= H) continue;
    unsigned sums[3]{};
    for (int sy = 0; sy < 2; ++sy) for (int sx = 0; sx < 2; ++sx) {
      const auto c = cached->pixels[((py * 2 + sy) * 64 / (size * 2)) * 64 + (px * 2 + sx) * 64 / (size * 2)];
      for (int channel = 0; channel < 3; ++channel) sums[channel] += (c >> (channel * 8)) & 255;
    }
    pixels[yy * W + xx] = 0xff000000u | (sums[0] / 4) | (sums[1] / 4 << 8) | (sums[2] / 4 << 16);
  }
  return true;
}
void Shell::vapor_shortcut_icon(std::uint32_t app, int x, int y, int size, const char* title) {
  if (vapor_game_icon(app, x, y, size)) return;
  if (app == 1089130) { icon(x, y, size, 4); return; }
  round({x, y, size, size}, 4, 0xff594d3b);
  char initial[2] = {title && title[0] ? title[0] : '?', 0};
  center(x, y + (size - 22) / 2, size, initial, 0, 0xffffffff);
}
void Shell::vapor_icon(int x, int y, int size, bool backdrop) {
  if (size <= 0) return;
  if (vapor_badge_width > 0 && vapor_badge_height > 0 && !vapor_badge_pixels.empty()) {
    // Crop to the circular badge and supersample its transparent edge. The
    // generated source remains unchanged; stray pixels outside the disc are
    // never painted onto the desktop or taskbar.
    for (int py = 0; py < size; ++py) for (int px = 0; px < size; ++px) {
      const int xx = x + px, yy = y + py;
      if (xx < 0 || yy < 0 || xx >= W || yy >= H) continue;
      unsigned alpha = 0, components[3]{};
      for (int sy = 0; sy < 4; ++sy) for (int sx = 0; sx < 4; ++sx) {
        const int dx = 2 * (px * 4 + sx) + 1 - size * 4;
        const int dy = 2 * (py * 4 + sy) + 1 - size * 4;
        if (dx * dx + dy * dy > size * size * 16) continue;
        const int source_x = vapor_badge_width / 20 + (px * 4 + sx) * (vapor_badge_width * 9 / 10) / (size * 4);
        const int source_y = vapor_badge_height / 20 + (py * 4 + sy) * (vapor_badge_height * 9 / 10) / (size * 4);
        const auto sample = vapor_badge_pixels[source_y * vapor_badge_width + source_x];
        const unsigned a = sample >> 24; alpha += a;
        for (int channel = 0; channel < 3; ++channel) components[channel] += ((sample >> (channel * 8)) & 255) * a;
      }
      auto& dest = pixels[yy * W + xx];
      for (int shift = 0; shift < 24; shift += 8) {
        const auto channel = (components[shift / 8] + (dest >> shift & 255) * (4080 - alpha)) / 4080;
        dest = (dest & ~(255u << shift)) | (channel << shift);
      }
    }
    return;
  }
  if (backdrop) round({x, y, size, size}, size / 2, 0xff9a521b);
  if (vapor_mark_width != 744 || vapor_mark_height != 171 || vapor_mark_pixels.empty()) return;
  for (int py = 0; py < size; ++py) for (int px = 0; px < size; ++px) {
    const int xx = x + px, yy = y + py;
    if (xx < 0 || yy < 0 || xx >= W || yy >= H) continue;
    unsigned alpha = 0, components[3]{};
    for (int sy = 0; sy < 4; ++sy) for (int sx = 0; sx < 4; ++sx) {
      const int source_y = (py * 4 + sy) * 171 / (size * 4);
      const int source_x = 95 + (px * 4 + sx) * 168 / (size * 4);
      const auto sample = vapor_mark_pixels[source_y * 744 + source_x];
      const unsigned a = sample >> 24; alpha += a;
      for (int channel = 0; channel < 3; ++channel) components[channel] += ((sample >> (channel * 8)) & 255) * a;
    }
    auto& dest = pixels[yy * W + xx];
    for (int shift = 0; shift < 24; shift += 8) {
      const auto channel = (components[shift / 8] + (dest >> shift & 255) * (4080 - alpha)) / 4080;
      dest = (dest & ~(255u << shift)) | (channel << shift);
    }
  }
}
void Shell::icon(int x, int y, int size, int kind) {
  if(kind>=17&&kind<=19) {
    auto box=[&](int a,int b,int w,int h,uint32_t color){rect({x+a*size/48,y+b*size/48,std::max(1,w*size/48),std::max(1,h*size/48)},color);};
    if(kind==17){box(8,3,32,42,0xffe8b857);box(12,7,24,35,0xfffdfdfd);for(int i=0;i<5;++i)box(16,13+i*5,17,1,0xffb3a38e);box(8,3,5,42,0xffd69d42);}
    else if(kind==18){round({x+3*size/48,y+8*size/48,42*size/48,33*size/48},size/3,0xffcce5f2);const uint32_t colors[]={0xff4444db,0xff00b9f0,0xff62ad30,0xffdc9038};for(int i=0;i<4;++i)round({x+(9+i*8)*size/48,y+(14+(i%2)*14)*size/48,std::max(2,size/7),std::max(2,size/7)},std::max(1,size/14),colors[i]);box(34,26,5,18,0xff487dab);}
    else {box(15,4,29,40,0xff498c2b);box(19,8,21,32,0xffdef1e5);for(int i=0;i<4;++i)box(19,14+i*7,21,1,0xff83b792);for(int i=0;i<3;++i)box(23+i*7,8,1,32,0xff83b792);box(3,14,23,23,0xff3c6e1c);line(x+9*size/48,y+20*size/48,x+20*size/48,y+31*size/48,0xffffffff);line(x+20*size/48,y+20*size/48,x+9*size/48,y+31*size/48,0xffffffff);}
    return;
  }
  if (kind == 16 && !vapor_mark_pixels.empty()) {
    vapor_icon(x, y, size, true);
    return;
  }
  if (kind == 0) {
    brand_icon(x, y, size);
    return;
  }
  if (kind == 11)
    kind = network_state != 3                           ? 13
           : std::strcmp(network_name, "Ethernet") == 0 ? 15
                                                        : 11;
  if (kind == 12 && ui_muted)
    kind = 14;
  if (kind < 0 || kind >= icons::Count)
    return;
  int level = icons::Levels - 1;
  for (int i = 0; i < icons::Levels; ++i)
    if (icons::Sizes[i] >= size) {
      level = i;
      break;
    }
  int source = icons::Sizes[level];
  auto *data = font + icons::offset(kind, level);
  bool monochrome = kind == 7 || kind >= 11;
  for (int py = 0; py < size; ++py)
    for (int px = 0; px < size; ++px) {
      uint32_t c = 0;
      std::memcpy(
          &c, data + ((py * source / size) * source + px * source / size) * 4,
          4);
      if (monochrome)
        c = (c & 0xff000000u) | (ink() & 0xffffffu);
      if (c >> 24)
        rect({x + px, y + py, 1, 1}, c);
    }
}
} // namespace ww

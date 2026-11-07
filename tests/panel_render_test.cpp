#include "../src/shell.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <vector>
int main() {
  ww::Shell s; assert(s.init("assets"));
  std::vector<std::uint32_t> base(ww::W*ww::H), expected(base.size());
  for(std::size_t i=0;i<base.size();++i)base[i]=0xff000000u|std::uint32_t(i*2654435761u&0xffffffu);
  for(bool dark:{false,true}) for(ww::Rect r:{ww::Rect{180,90,1200,780},ww::Rect{0,0,ww::W,ww::H-54},ww::Rect{-70,-30,400,250},ww::Rect{1850,1000,250,200},ww::Rect{10,10,18,18}}) {
    s.dark=dark; std::copy(base.begin(),base.end(),s.pixels);
    for(int i=18;i>0;--i)s.round({r.x-i,r.y-i+8,r.w+2*i,r.h+2*i},12,0x03000000);
    s.round({r.x-1,r.y-1,r.w+2,r.h+2},11,dark?0xff5c514c:0xffe4dad2);
    s.round(r,10,s.surface()); std::copy(s.pixels,s.pixels+base.size(),expected.begin());
    std::copy(base.begin(),base.end(),s.pixels);s.panel(r);
    assert(std::equal(expected.begin(),expected.end(),s.pixels));
  }
  std::puts("PASS: optimized shadow matches original pixels for light/dark, maximized, clipped and small windows");
}

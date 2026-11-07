#include "../src/shell.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <vector>
static unsigned char glyph_char(const char*& s) {return static_cast<unsigned char>(*s++);}
static std::uint32_t blend(std::uint32_t a,std::uint32_t b,unsigned t){std::uint32_t o=0xff000000;for(int shift=0;shift<24;shift+=8)o|=(((a>>shift&255)*(255-t)+(b>>shift&255)*t)/255)<<shift;return o;}
static void reference(ww::Shell& s,int x,int y,const char* value,int size,std::uint32_t color){while(*value){unsigned ch=glyph_char(value);const auto* g=s.font+(size*96+ch-32)*48*56;for(int gy=1;gy<56;++gy)for(int gx=0;gx<48;++gx){int px=x+gx,py=y+gy-1;if(px>=0&&px<ww::W&&py>=0&&py<ww::H&&g[gy*48+gx])s.pixels[py*ww::W+px]=blend(s.pixels[py*ww::W+px],color,g[gy*48+gx]);}x+=g[0];}}
int main(){ww::Shell s;assert(s.init("assets"));std::string chars;for(int ch=32;ch<127;++ch)chars+=char(ch);std::vector<std::uint32_t> base(ww::W*ww::H),expected(base.size());for(std::size_t i=0;i<base.size();++i)base[i]=0xff000000u|std::uint32_t(i*2654435761u&0xffffffu);
for(std::size_t first=0;first<chars.size();first+=32){const auto chunk=chars.substr(first,32);
for(int size=0;size<3;++size)for(auto xy:{ww::Rect{20,20,0,0},ww::Rect{-200,-4,0,0},ww::Rect{ww::W-80,ww::H-15,0,0}})for(auto color:{0xff123456u,0x00332211u}){std::copy(base.begin(),base.end(),s.pixels);reference(s,xy.x,xy.y,chunk.c_str(),size,color);std::copy(s.pixels,s.pixels+base.size(),expected.begin());std::copy(base.begin(),base.end(),s.pixels);s.text(xy.x,xy.y,chunk.c_str(),size,color);assert(std::equal(expected.begin(),expected.end(),s.pixels));}
}
std::puts("PASS: bounded glyph rendering matches original pixels for every atlas character, size, clipping and color");}

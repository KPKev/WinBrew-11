#include "../src/image_codec.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
#include <vector>

int main() {
  std::ifstream file("tests/fixtures/paint-photo.jpg", std::ios::binary);
  std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)), {});
  assert(!bytes.empty());
  std::vector<std::uint32_t> pixels(ww::image::MaxPixels);
  int width = 0, height = 0;
  assert(ww::image::read_memory(bytes.data(), bytes.size(), pixels.data(), width, height));
  assert(width > 0 && height > 0 && width <= ww::image::MaxWidth && height <= ww::image::MaxHeight);
  const unsigned char bad[] = {1, 2, 3};
  int before_width = width, before_height = height;
  assert(!ww::image::read_memory(bad, sizeof(bad), pixels.data(), width, height));
  assert(width == before_width && height == before_height);
}

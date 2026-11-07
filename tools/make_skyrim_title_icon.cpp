// Generate a PS5 title icon using the same Playden converter as Vapor.
// Input is the public Steam image selected for app 489830.
#include "homebrew/title_icon.hpp"
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <cstdio>
#include <vector>

int main(int argc, char** argv) {
  if (argc != 3) return 2;
  int width = 0, height = 0, channels = 0;
  unsigned char* pixels = stbi_load(argv[1], &width, &height, &channels, 4);
  if (!pixels || width <= 0 || height <= 0 || width > 1920 || height > 1920) return 3;
  playden::homebrew::ArtworkImage image;
  image.width = static_cast<unsigned>(width); image.height = static_cast<unsigned>(height);
  image.rgba.assign(pixels, pixels + static_cast<std::size_t>(width) * height * 4);
  stbi_image_free(pixels);
  const auto png = playden::homebrew::MakeTitleIcon(image);
  if (!png) return 4;
  FILE* file = std::fopen(argv[2], "wbx");
  if (!file) return 5;
  const auto count = std::fwrite(png->data(), 1, png->size(), file);
  return std::fclose(file) == 0 && count == png->size() ? 0 : 6;
}

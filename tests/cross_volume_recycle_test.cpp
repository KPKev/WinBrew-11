#include "../src/explorer_fileops.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
using namespace ww::fileops;

static void put(const fs::path &path, const std::string &data) {
  std::ofstream stream(path, std::ios::binary); stream << data; assert(stream.good());
}
static std::string get(const fs::path &path) {
  std::ifstream stream(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(stream), {}};
}
static void finish(CrossVolumeRecycle &operation) {
  for (int steps=0; operation.busy() && steps<100000; ++steps) operation.step();
  assert(!operation.busy());
}

int main() {
  const auto root=fs::absolute(fs::path("build")/("cross-recycle-"+
      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())));
  const auto source=root/"source", bin=root/"target"/".winbrew-trash";
  fs::create_directories(source);fs::create_directories(bin);
  put(source/"note.txt","recoverable data");
  Clipboard clip;assert(clip.copy((source/"note.txt").string().c_str()));
  CrossVolumeRecycle op;assert(op.begin(clip,bin.string().c_str()));finish(op);
  assert(op.state==State::complete && !fs::exists(source/"note.txt"));
  assert(get(bin/"note.txt")=="recoverable data");

  fs::create_directories(source/"folder"/"nested");put(source/"folder"/"nested"/"item.txt","nested");
  assert(clip.copy((source/"folder").string().c_str()));assert(op.begin(clip,bin.string().c_str()));finish(op);
  assert(op.state==State::complete && !fs::exists(source/"folder"));
  assert(get(bin/"folder"/"nested"/"item.txt")=="nested");

  // Cancel or change the source before copying completes: never remove it.
  put(source/"cancel.txt",std::string(150000,'c'));
  assert(clip.copy((source/"cancel.txt").string().c_str()));assert(op.begin(clip,bin.string().c_str()));
  op.step();op.cancel();assert(fs::exists(source/"cancel.txt"));
  put(source/"changed.txt",std::string(150000,'x'));
  assert(clip.copy((source/"changed.txt").string().c_str()));assert(op.begin(clip,bin.string().c_str()));
  op.step();put(source/"changed.txt","changed while copying");finish(op);
  assert(op.state==State::failed && get(source/"changed.txt")=="changed while copying");

  // A copied item changed before original removal must leave the original.
  put(source/"race.txt","original");assert(clip.copy((source/"race.txt").string().c_str()));
  assert(op.begin(clip,bin.string().c_str()));
  for(int steps=0;op.busy()&&!op.removing&&steps<100000;++steps)op.step();
  assert(op.busy()&&op.removing);put(bin/"race.txt","changed copy");finish(op);
  assert(op.state==State::failed && get(source/"race.txt")=="original");

  // A destination that is not the designated bin is never accepted.
  put(source/"wrong-target.txt","keep");assert(clip.copy((source/"wrong-target.txt").string().c_str()));
  assert(!op.begin(clip,(root/"target").string().c_str()));assert(get(source/"wrong-target.txt")=="keep");
#ifndef _WIN32
  fs::create_symlink(source/"wrong-target.txt",source/"link.txt");
  assert(!clip.copy((source/"link.txt").string().c_str()));
#endif

  // Replacing the bin itself must not turn a verified copy into data loss.
  put(source/"swapped.txt","keep original");assert(clip.copy((source/"swapped.txt").string().c_str()));
  assert(op.begin(clip,bin.string().c_str()));
  for(int steps=0;op.busy()&&!op.removing&&steps<100000;++steps)op.step();
  assert(op.busy()&&op.removing);fs::rename(bin,root/"target"/"held-bin");fs::create_directory(bin);
  finish(op);assert(op.state==State::failed&&get(source/"swapped.txt")=="keep original");
  assert(get(root/"target"/"held-bin"/"swapped.txt")=="keep original");

  assert(root.parent_path()==fs::absolute("build"));fs::remove_all(root);
  std::cout<<"PASS: verified cross-volume recycling, directory data, cancellation and source/copy race retention\n";
}

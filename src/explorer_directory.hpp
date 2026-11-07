// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace ww::directory {
// The payload SDK's FreeBSD 11 directory ABI. Native compilation also checks
// these offsets against <sys/dirent.h>; this parser never casts unaligned data.
struct Record {
  uint32_t inode;
  uint16_t length;
  uint8_t type, name_length;
  char name[256];
};
static_assert(offsetof(Record, length) == 4 && offsetof(Record, type) == 6 &&
              offsetof(Record, name_length) == 7 &&
              offsetof(Record, name) == 8);
enum class Read { entry, end, invalid };
inline Read next(const unsigned char *bytes, size_t count, size_t &offset,
                 Record &record) {
  if (offset == count)
    return Read::end;
  if (!bytes || offset > count || count - offset < 8)
    return Read::invalid;
  std::memcpy(&record, bytes + offset, 8);
  if (record.length < 8 || record.length > count - offset)
    return Read::invalid;
  if (!record.inode) {
    record.name[0] = 0;
    offset += record.length;
    return Read::entry;
  }
  if (record.name_length == 0 || size_t(record.name_length) + 9 > record.length)
    return Read::invalid;
  const auto *name = bytes + offset + 8;
  if (name[record.name_length] != 0 ||
      std::memchr(name, 0, record.name_length) ||
      std::memchr(name, '/', record.name_length))
    return Read::invalid;
  std::memcpy(record.name, name, size_t(record.name_length) + 1);
  offset += record.length;
  return Read::entry;
}
inline int native_error(int code) {
  const auto value = uint32_t(code);
  if ((value & 0xffff0000u) == 0x80020000u && (value & 0xffffu) > 0 &&
      (value & 0xffffu) < 4096)
    return int(value & 0xffffu);
  return EIO;
}
inline int error_or_io(int code) { return code > 0 ? code : EIO; }
} // namespace ww::directory

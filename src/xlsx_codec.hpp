// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Bounded, in-memory SpreadsheetML package editing. No ZIP entries are
// extracted to disk. Unedited package parts and unedited cell XML are retained
// verbatim.
#include "stb_image.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>
namespace ww::xlsx {
constexpr size_t MaxArchive = 32 * 1024 * 1024, MaxPart = 8 * 1024 * 1024,
                 MaxExpanded = 16 * 1024 * 1024;
using Grid = std::array<std::string, 2600>;
struct Part {
  std::string name, bytes;
};
inline uint32_t crc(const std::string &bytes) {
  uint32_t value = ~0u;
  for (unsigned char c : bytes) {
    value ^= c;
    for (int i = 0; i < 8; ++i)
      value = (value >> 1) ^ (0xedb88320u & uint32_t(-int(value & 1)));
  }
  return ~value;
}
inline uint32_t word(const std::string &s, size_t at, int n) {
  uint32_t v = 0;
  for (int i = 0; i < n; ++i)
    v |= uint32_t((unsigned char)s[at + i]) << (8 * i);
  return v;
}
inline void put(std::string &s, uint32_t v, int n) {
  for (int i = 0; i < n; ++i)
    s += char(v >> (8 * i));
}
inline bool fail(std::string &error, const char *message) {
  error = message;
  return false;
}
inline bool safe_name(const std::string &name) {
  return !name.empty() && name[0] != '/' && name.find('\\') == name.npos &&
         name.find(':') == name.npos && name.find('\0') == name.npos &&
         name != ".." && name.find("../") == name.npos &&
         name.find("/..") == name.npos;
}
inline bool unzip(const std::string &bytes, std::vector<Part> &out,
                  std::string &error) {
  if (bytes.size() < 22 || bytes.size() > MaxArchive)
    return fail(error, "Invalid or oversized XLSX package (32 MB limit).");
  size_t e = bytes.size() - 22,
         minimum = bytes.size() > 65557 ? bytes.size() - 65557 : 0;
  for (;;) {
    if (word(bytes, e, 4) == 0x06054b50 &&
        e + 22 + word(bytes, e + 20, 2) == bytes.size())
      break;
    if (e == minimum)
      return fail(error, "Not an unencrypted XLSX ZIP package.");
    --e;
  }
  const size_t count = word(bytes, e + 10, 2),
               directory = word(bytes, e + 16, 4),
               size = word(bytes, e + 12, 4);
  if (word(bytes, e + 4, 2) || word(bytes, e + 6, 2) ||
      word(bytes, e + 8, 2) != count || count > 1024 || directory > e ||
      size != e - directory)
    return fail(error, "Unsupported split, ZIP64 or oversized XLSX package.");
  size_t at = directory, total = 0;
  std::map<std::string, bool> seen;
  for (size_t i = 0; i < count; ++i) {
    if (at > e || e - at < 46 || word(bytes, at, 4) != 0x02014b50)
      return fail(error, "Damaged XLSX ZIP directory.");
    size_t flags = word(bytes, at + 8, 2), method = word(bytes, at + 10, 2),
           packed = word(bytes, at + 20, 4), length = word(bytes, at + 24, 4),
           nl = word(bytes, at + 28, 2), xl = word(bytes, at + 30, 2),
           cl = word(bytes, at + 32, 2), local = word(bytes, at + 42, 4);
    if (nl + xl + cl > e - at - 46 || length > MaxPart ||
        total + length > MaxExpanded || word(bytes, at + 34, 2) ||
        flags & 0x2041 || (method != 0 && method != 8))
      return fail(error, "Encrypted or unsupported XLSX ZIP entry, or "
                         "expansion limit exceeded.");
    std::string name = bytes.substr(at + 46, nl);
    if (!safe_name(name) || seen[name])
      return fail(error, "Duplicate or invalid XLSX package path.");
    seen[name] = true;
    if (local > directory || directory - local < 30 ||
        word(bytes, local, 4) != 0x04034b50 ||
        word(bytes, local + 8, 2) != method ||
        word(bytes, local + 6, 2) != flags)
      return fail(error, "Damaged XLSX ZIP entry.");
    size_t lnl = word(bytes, local + 26, 2), lxl = word(bytes, local + 28, 2),
           offset = local + 30 + lnl + lxl;
    if (offset > directory || packed > directory - offset || lnl != nl ||
        bytes.compare(local + 30, lnl, name))
      return fail(error, "Truncated XLSX ZIP entry.");
    std::string data(length, '\0');
    if (method == 0) {
      if (packed != length)
        return fail(error, "Invalid stored XLSX entry.");
      data.assign(bytes, offset, length);
    } else if (stbi_zlib_decode_noheader_buffer(data.data(), int(length),
                                                bytes.data() + offset,
                                                int(packed)) != int(length))
      return fail(error, "Damaged compressed XLSX entry.");
    if (crc(data) != word(bytes, at + 16, 4))
      return fail(error, "XLSX checksum mismatch.");
    total += length;
    out.push_back({std::move(name), std::move(data)});
    at += 46 + nl + xl + cl;
  }
  if (at != e)
    return fail(error, "Invalid XLSX directory length.");
  return true;
}
inline bool zip(const std::vector<Part> &parts, std::string &out,
                std::string &error) {
  std::string central;
  out.clear();
  for (const auto &p : parts) {
    if (out.size() + p.bytes.size() + p.name.size() + 30 > MaxArchive)
      return fail(error, "Workbook exceeds the XLSX save limit.");
    uint32_t offset = uint32_t(out.size()), checksum = crc(p.bytes);
    put(out, 0x04034b50, 4);
    put(out, 20, 2);
    put(out, 0x800, 2);
    put(out, 0, 2);
    put(out, 0, 2);
    put(out, 33, 2);
    put(out, checksum, 4);
    put(out, uint32_t(p.bytes.size()), 4);
    put(out, uint32_t(p.bytes.size()), 4);
    put(out, uint32_t(p.name.size()), 2);
    put(out, 0, 2);
    out += p.name;
    out += p.bytes;
    put(central, 0x02014b50, 4);
    put(central, 20, 2);
    put(central, 20, 2);
    put(central, 0x800, 2);
    put(central, 0, 2);
    put(central, 0, 2);
    put(central, 33, 2);
    put(central, checksum, 4);
    put(central, uint32_t(p.bytes.size()), 4);
    put(central, uint32_t(p.bytes.size()), 4);
    put(central, uint32_t(p.name.size()), 2);
    put(central, 0, 2);
    put(central, 0, 2);
    put(central, 0, 2);
    put(central, 0, 2);
    put(central, 0, 4);
    put(central, offset, 4);
    central += p.name;
  }
  uint32_t offset = uint32_t(out.size());
  out += central;
  put(out, 0x06054b50, 4);
  put(out, 0, 2);
  put(out, 0, 2);
  put(out, uint32_t(parts.size()), 2);
  put(out, uint32_t(parts.size()), 2);
  put(out, uint32_t(central.size()), 4);
  put(out, offset, 4);
  put(out, 0, 2);
  return out.size() <= MaxArchive;
}
inline std::string local_name(std::string name) {
  auto colon = name.find(':');
  return colon == name.npos ? name : name.substr(colon + 1);
}
inline std::string prefix(const std::string &name) {
  auto colon = name.find(':');
  return colon == name.npos ? "" : name.substr(0, colon + 1);
}
inline void utf8(std::string &out, uint32_t c) {
  if (c < 128)
    out += char(c);
  else if (c < 2048) {
    out += char(0xc0 | (c >> 6));
    out += char(0x80 | (c & 63));
  } else if (c < 65536) {
    out += char(0xe0 | (c >> 12));
    out += char(0x80 | ((c >> 6) & 63));
    out += char(0x80 | (c & 63));
  } else {
    out += char(0xf0 | (c >> 18));
    out += char(0x80 | ((c >> 12) & 63));
    out += char(0x80 | ((c >> 6) & 63));
    out += char(0x80 | (c & 63));
  }
}
inline bool decode(const std::string &in, std::string &out) {
  for (size_t i = 0; i < in.size(); ++i) {
    unsigned char c = in[i];
    if (c != '&') {
      if (!c || (c < 32 && c != 9 && c != 10 && c != 13))
        return false;
      out += char(c);
      continue;
    }
    size_t end = in.find(';', i + 1);
    if (end == in.npos || end - i > 16)
      return false;
    std::string entity = in.substr(i + 1, end - i - 1);
    if (entity == "amp")
      out += '&';
    else if (entity == "lt")
      out += '<';
    else if (entity == "gt")
      out += '>';
    else if (entity == "quot")
      out += '"';
    else if (entity == "apos")
      out += '\'';
    else if (!entity.empty() && entity[0] == '#') {
      char *tail = nullptr;
      const bool hex =
          entity.size() > 1 && (entity[1] == 'x' || entity[1] == 'X');
      unsigned long value =
          std::strtoul(entity.c_str() + (hex ? 2 : 1), &tail, hex ? 16 : 10);
      if (*tail || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff) ||
          (!value) || (value < 32 && value != 9 && value != 10 && value != 13))
        return false;
      utf8(out, uint32_t(value));
    } else
      return false;
    i = end;
  }
  return true;
}
inline int hex(char c) {
  return c >= '0' && c <= '9'   ? c - '0'
         : c >= 'a' && c <= 'f' ? c - 'a' + 10
         : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                : -1;
}
inline int excel_code(const std::string &s, size_t i) {
  if (i + 7 > s.size() || s[i] != '_' || (s[i + 1] != 'x' && s[i + 1] != 'X') ||
      s[i + 6] != '_')
    return -1;
  int v = 0;
  for (int n = 2; n < 6; ++n) {
    int h = hex(s[i + n]);
    if (h < 0)
      return -1;
    v = v * 16 + h;
  }
  return v;
}
inline std::string excel_text(const std::string &s) {
  std::string out;
  for (size_t i = 0; i < s.size(); ++i) {
    int c = excel_code(s, i);
    if (c < 0) {
      out += s[i];
      continue;
    }
    i += 6;
    if (c >= 0xd800 && c <= 0xdbff) {
      int low = excel_code(s, i + 1);
      if (low >= 0xdc00 && low <= 0xdfff) {
        c = 0x10000 + ((c - 0xd800) << 10) + (low - 0xdc00);
        i += 7;
      } else
        c = 0xfffd;
    } else if (c >= 0xdc00 && c <= 0xdfff)
      c = 0xfffd;
    utf8(out, uint32_t(c));
  }
  return out;
}
inline std::string escaped_excel_text(const std::string &s) {
  std::string out;
  for (size_t i = 0; i < s.size(); ++i) {
    if (excel_code(s, i) >= 0)
      out += "_x005F_";
    else
      out += s[i];
  }
  return out;
}
inline std::string escape(const std::string &in) {
  std::string out;
  for (char c : in) {
    if (c == '&')
      out += "&amp;";
    else if (c == '<')
      out += "&lt;";
    else if (c == '>')
      out += "&gt;";
    else if (c == '"')
      out += "&quot;";
    else if (c == '\r')
      out += "&#13;";
    else
      out += c;
  }
  return out;
}
struct Node {
  std::string name;
  std::map<std::string, std::string> attrs;
  std::vector<int> children;
  size_t start = 0, open = 0, close = 0, end = 0;
  bool empty = false;
  std::string attr(const char *key) const {
    for (const auto &a : attrs)
      if (local_name(a.first) == key)
        return a.second;
    return "";
  }
};
struct Xml {
  const std::string &text;
  std::vector<Node> nodes;
  explicit Xml(const std::string &s) : text(s) {}
  bool parse(std::string &error) {
    std::vector<int> stack;
    size_t at = 0;
    int roots = 0;
    while (at < text.size()) {
      if (text[at] != '<') {
        size_t next = text.find('<', at);
        if (next == text.npos)
          next = text.size();
        std::string decoded;
        if (!decode(text.substr(at, next - at), decoded))
          return fail(error, "Invalid XLSX XML text.");
        at = next;
        continue;
      }
      if (text.compare(at, 4, "<!--") == 0) {
        size_t end = text.find("-->", at + 4);
        if (end == text.npos)
          return fail(error, "Invalid XML comment.");
        at = end + 3;
        continue;
      }
      if (text.compare(at, 2, "<?") == 0) {
        size_t end = text.find("?>", at + 2);
        if (end == text.npos)
          return false;
        at = end + 2;
        continue;
      }
      if (text.compare(at, 9, "<![CDATA[") == 0) {
        size_t end = text.find("]]>", at + 9);
        if (end == text.npos)
          return false;
        at = end + 3;
        continue;
      }
      if (text.compare(at, 2, "<!") == 0)
        return fail(error, "XLSX XML declarations/entities are unsupported.");
      size_t start = at++;
      if (at >= text.size())
        return false;
      bool closing = text[at] == '/';
      if (closing)
        ++at;
      size_t n = at;
      while (at < text.size() && !std::isspace((unsigned char)text[at]) &&
             text[at] != '/' && text[at] != '>')
        ++at;
      std::string name = text.substr(n, at - n);
      if (name.empty())
        return false;
      if (closing) {
        while (at < text.size() && std::isspace((unsigned char)text[at]))
          ++at;
        if (at >= text.size() || text[at++] != '>' || stack.empty() ||
            nodes[stack.back()].name != name)
          return fail(error, "Unbalanced XLSX XML.");
        auto &node = nodes[stack.back()];
        node.close = start;
        node.end = at;
        stack.pop_back();
        continue;
      }
      Node node;
      node.name = name;
      node.start = start;
      for (;;) {
        while (at < text.size() && std::isspace((unsigned char)text[at]))
          ++at;
        if (at >= text.size())
          return false;
        if (text[at] == '/' || text[at] == '>')
          break;
        size_t a = at;
        while (at < text.size() && text[at] != '=' &&
               !std::isspace((unsigned char)text[at]))
          ++at;
        std::string key = text.substr(a, at - a);
        while (at < text.size() && std::isspace((unsigned char)text[at]))
          ++at;
        if (key.empty() || at >= text.size() || text[at++] != '=')
          return false;
        while (at < text.size() && std::isspace((unsigned char)text[at]))
          ++at;
        if (at >= text.size() || (text[at] != '"' && text[at] != '\''))
          return false;
        char q = text[at++];
        size_t value = at;
        at = text.find(q, at);
        if (at == text.npos)
          return false;
        std::string decoded;
        if (!decode(text.substr(value, at - value), decoded) ||
            node.attrs.count(key))
          return false;
        node.attrs[key] = std::move(decoded);
        ++at;
      }
      node.empty = text[at] == '/';
      if (node.empty && ++at >= text.size())
        return false;
      if (text[at++] != '>')
        return false;
      node.open = at;
      if (node.empty)
        node.close = node.end = at;
      if (nodes.size() >= 100000 || stack.size() >= 128)
        return fail(error, "XLSX XML complexity limit exceeded.");
      int index = int(nodes.size());
      nodes.push_back(std::move(node));
      if (stack.empty())
        ++roots;
      else
        nodes[stack.back()].children.push_back(index);
      if (!nodes.back().empty)
        stack.push_back(index);
    }
    return stack.empty() && roots == 1 && !nodes.empty();
  }
  int child(int parent, const char *name) const {
    for (int i : nodes[parent].children)
      if (local_name(nodes[i].name) == name)
        return i;
    return -1;
  }
  bool value(int index, std::string &out) const {
    if (index < 0)
      return true;
    const auto &n = nodes[index];
    if (n.empty)
      return true;
    if (!n.children.empty())
      return false;
    std::string raw = text.substr(n.open, n.close - n.open);
    if (raw.compare(0, 9, "<![CDATA[") == 0 && raw.size() >= 12) {
      out = raw.substr(9, raw.size() - 12);
      return true;
    }
    return decode(raw, out);
  }
  bool strings(int index, std::string &out) const {
    if (index < 0)
      return true;
    const auto &n = nodes[index];
    if (local_name(n.name) == "rPh")
      return true;
    if (local_name(n.name) == "t")
      return value(index, out);
    for (int i : n.children)
      if (!strings(i, out))
        return false;
    return true;
  }
};
inline bool positive(const std::string &s, int &out, int max) {
  if (s.empty())
    return false;
  out = 0;
  for (char c : s) {
    if (c < '0' || c > '9' || out > max / 10)
      return false;
    out = out * 10 + c - '0';
    if (out > max)
      return false;
  }
  return out > 0;
}
inline bool coordinate(const std::string &s, int &r, int &c) {
  size_t i = 0;
  c = 0;
  while (i < s.size() && s[i] >= 'A' && s[i] <= 'Z') {
    c = c * 26 + s[i++] - 'A' + 1;
    if (c > 16384)
      return false;
  }
  if (!i || !positive(s.substr(i), r, 1048576))
    return false;
  --r;
  --c;
  return true;
}
inline std::string address(int r, int c) {
  return std::string(1, char('A' + c)) + std::to_string(r + 1);
}
struct Edit {
  size_t start, end;
  std::string replacement;
};
inline std::string patched(const std::string &source, size_t start, size_t end,
                           std::vector<Edit> edits) {
  std::stable_sort(
      edits.begin(), edits.end(),
      [](const Edit &a, const Edit &b) { return a.start < b.start; });
  std::string out;
  size_t at = start;
  for (const auto &e : edits) {
    out.append(source, at, e.start - at);
    out += e.replacement;
    at = e.end;
  }
  out.append(source, at, end - at);
  return out;
}
inline std::string expanded(const Xml &xml, int index,
                            const std::string &contents) {
  const auto &n = xml.nodes[index];
  if (!n.empty)
    return xml.text.substr(n.start, n.open - n.start) + contents +
           xml.text.substr(n.close, n.end - n.close);
  std::string opening = xml.text.substr(n.start, n.open - n.start);
  size_t slash = opening.rfind('/');
  opening.erase(slash, 1);
  return opening + contents + "</" + n.name + ">";
}
struct Page {
  std::string name, path;
  Grid cells{}, original{};
  std::array<bool, 2600> formulas{};
  std::map<std::string, std::string> cached_values;
};
struct Workbook {
  std::vector<Part> parts;
  std::vector<Page> pages;
  std::string original_bytes, workbook_path, rels_path;
  bool changed = false;
  static constexpr const char *Main =
      "http://schemas.openxmlformats.org/spreadsheetml/2006/main";
  int part(const std::string &name) const {
    for (size_t i = 0; i < parts.size(); ++i)
      if (parts[i].name == name)
        return int(i);
    return -1;
  }
  static std::string resolve(const std::string &base,
                             const std::string &target) {
    if (target.empty() || target.find(':') != target.npos ||
        target.find('\\') != target.npos)
      return "";
    std::string path = target[0] == '/'
                           ? target.substr(1)
                           : base.substr(0, base.rfind('/') == base.npos
                                                ? 0
                                                : base.rfind('/') + 1) +
                                 target;
    std::vector<std::string> segments;
    size_t at = 0;
    while (at <= path.size()) {
      size_t end = path.find('/', at);
      if (end == path.npos)
        end = path.size();
      auto segment = path.substr(at, end - at);
      if (segment == "..") {
        if (segments.empty())
          return "";
        segments.pop_back();
      } else if (!segment.empty() && segment != ".")
        segments.push_back(segment);
      at = end + 1;
    }
    std::string out;
    for (const auto &s : segments) {
      if (!out.empty())
        out += '/';
      out += s;
    }
    return safe_name(out) ? out : "";
  }
  static std::string relationship_path(const std::string &path) {
    size_t slash = path.rfind('/');
    return (slash == path.npos ? "" : path.substr(0, slash + 1)) + "_rels/" +
           (slash == path.npos ? path : path.substr(slash + 1)) + ".rels";
  }
  bool load(const std::string &bytes, std::string &error) {
    if (!unzip(bytes, parts, error))
      return false;
    int root = part("_rels/.rels");
    if (root < 0)
      return fail(error, "Workbook relationship is missing.");
    Xml rootxml(parts[root].bytes);
    if (!rootxml.parse(error))
      return false;
    for (int i : rootxml.nodes[0].children) {
      auto &n = rootxml.nodes[i];
      if (n.attr("Type").ends_with("/officeDocument") &&
          n.attr("TargetMode") != "External")
        workbook_path = resolve("", n.attr("Target"));
    }
    int wi = part(workbook_path);
    rels_path = relationship_path(workbook_path);
    int ri = part(rels_path);
    if (wi < 0 || ri < 0)
      return fail(error, "Workbook parts are missing.");
    Xml book(parts[wi].bytes), rels(parts[ri].bytes);
    if (!book.parse(error) || !rels.parse(error))
      return false;
    std::map<std::string, std::string> relationships;
    std::string shared_path;
    for (int i : rels.nodes[0].children) {
      auto &n = rels.nodes[i];
      if (n.attr("TargetMode") == "External")
        continue;
      const auto target = resolve(workbook_path, n.attr("Target"));
      relationships[n.attr("Id")] = target;
      if (n.attr("Type").find("/sharedStrings") != n.name.npos)
        shared_path = target;
    }
    std::vector<std::string> shared;
    int si = part(shared_path);
    if (si >= 0) {
      Xml strings(parts[si].bytes);
      if (!strings.parse(error))
        return false;
      for (int i : strings.nodes[0].children)
        if (local_name(strings.nodes[i].name) == "si") {
          std::string v;
          if (!strings.strings(i, v))
            return fail(error, "Invalid shared string.");
          shared.push_back(std::move(v));
        }
    }
    int sheets = book.child(0, "sheets");
    if (sheets < 0)
      return fail(error, "Workbook has no worksheets.");
    for (int i : book.nodes[sheets].children) {
      auto &n = book.nodes[i];
      if (local_name(n.name) != "sheet")
        continue;
      if (pages.size() >= 16)
        return fail(error, "This workbook exceeds the 16-sheet limit.");
      Page page;
      page.name = n.attr("name");
      page.path = relationships[n.attr("id")];
      int pi = part(page.path);
      if (pi < 0)
        return fail(error, "Worksheet relationship is missing.");
      Xml xml(parts[pi].bytes);
      if (!xml.parse(error))
        return false;
      int data = xml.child(0, "sheetData");
      if (data < 0)
        return fail(error, "This sheet type is unsupported.");
      std::map<std::string, bool> seen;
      for (int row : xml.nodes[data].children)
        if (local_name(xml.nodes[row].name) == "row")
          for (int cell : xml.nodes[row].children) {
            const auto &cn = xml.nodes[cell];
            if (local_name(cn.name) != "c")
              continue;
            int r = 0, c = 0;
            auto reference = cn.attr("r");
            if (!coordinate(reference, r, c) || seen[reference])
              return fail(error, "Invalid or duplicate worksheet cell.");
            seen[reference] = true;
            if (r >= 100 || c >= 26)
              continue;
            std::string value, formula;
            int fi = xml.child(cell, "f"), vi = xml.child(cell, "v");
            if (!xml.value(vi, value) || !xml.value(fi, formula))
              return fail(error, "Invalid worksheet cell contents.");
            auto type = cn.attr("t");
            if (fi >= 0) {
              if (formula.empty()) {
                value = "[shared/array formula]";
              } else {
                page.cached_values[reference] = value;
                value = "=" + formula;
                page.formulas[r * 26 + c] = true;
              }
            } else if (type == "s") {
              char *end = nullptr;
              unsigned long index = std::strtoul(value.c_str(), &end, 10);
              if (value.empty() || *end || index >= shared.size())
                return fail(error, "Invalid shared string index.");
              value = excel_text(shared[index]);
            } else if (type == "inlineStr") {
              value.clear();
              if (!xml.strings(xml.child(cell, "is"), value))
                return false;
              value = excel_text(value);
            } else if (type == "b")
              value = value == "1" ? "TRUE" : "FALSE";
            page.cells[r * 26 + c] = std::move(value);
          }
      page.original = page.cells;
      pages.push_back(std::move(page));
    }
    if (pages.empty())
      return fail(error, "Workbook has no editable worksheets.");
    original_bytes = bytes;
    changed = false;
    return true;
  }
  static std::string cell_xml(const std::string &ref, const std::string &value,
                              const Node *old, const std::string &pre,
                              const std::string &extra = "") {
    std::string attrs = " r=\"" + ref + "\"";
    if (old)
      for (const auto &a : old->attrs)
        if (local_name(a.first) != "r" && local_name(a.first) != "t")
          attrs += " " + a.first + "=\"" + escape(a.second) + "\"";
    std::string contents, type;
    if (!value.empty() && value[0] == '=')
      contents = "<" + pre + "f>" + escape(value.substr(1)) + "</" + pre + "f>";
    else {
      char *end = nullptr;
      std::strtod(value.c_str(), &end);
      if (!value.empty() && end != value.c_str() && !*end &&
          std::isfinite(std::strtod(value.c_str(), nullptr)))
        contents = "<" + pre + "v>" + escape(value) + "</" + pre + "v>";
      else {
        type = " t=\"inlineStr\"";
        contents = "<" + pre + "is><" + pre + "t xml:space=\"preserve\">" +
                   escape(escaped_excel_text(value)) + "</" + pre + "t></" +
                   pre + "is>";
      }
    }
    return "<" + pre + "c" + attrs + type + ">" + contents + extra + "</" +
           pre + "c>";
  }
  bool update_page(const Page &page, std::string &out,
                   std::string &error) const {
    int pi = part(page.path);
    if (pi < 0)
      return false;
    const auto &source = parts[pi].bytes;
    Xml xml(source);
    if (!xml.parse(error))
      return false;
    int di = xml.child(0, "sheetData");
    if (di < 0)
      return false;
    const auto &data = xml.nodes[di];
    std::string pre = prefix(data.name);
    std::vector<Edit> rows;
    std::map<int, int> existing;
    for (int i : data.children)
      if (local_name(xml.nodes[i].name) == "row") {
        int row = 0;
        if (!positive(xml.nodes[i].attr("r"), row, 1048576) ||
            existing.count(row - 1))
          return fail(error, "Invalid worksheet row.");
        existing[row - 1] = i;
      }
    for (int r = 0; r < 100; ++r) {
      bool changed_row = false;
      for (int c = 0; c < 26; ++c)
        if (page.cells[r * 26 + c] != page.original[r * 26 + c])
          changed_row = true;
      if (!changed_row)
        continue;
      auto found = existing.find(r);
      if (found == existing.end()) {
        std::string contents;
        for (int c = 0; c < 26; ++c)
          if (!page.cells[r * 26 + c].empty())
            contents +=
                cell_xml(address(r, c), page.cells[r * 26 + c], nullptr, pre);
        size_t position = data.empty ? data.open : data.close;
        for (const auto &item : existing)
          if (item.first > r) {
            position = xml.nodes[item.second].start;
            break;
          }
        rows.push_back({position, position,
                        "<" + pre + "row r=\"" + std::to_string(r + 1) + "\">" +
                            contents + "</" + pre + "row>"});
        continue;
      }
      int row = found->second;
      const auto &rn = xml.nodes[row];
      std::map<int, int> cells;
      for (int i : rn.children)
        if (local_name(xml.nodes[i].name) == "c") {
          int rr = 0, cc = 0;
          if (!coordinate(xml.nodes[i].attr("r"), rr, cc) || rr != r ||
              cells.count(cc))
            return fail(error, "Invalid cell position.");
          cells[cc] = i;
        }
      std::vector<Edit> edits;
      for (int c = 0; c < 26; ++c) {
        const auto &value = page.cells[r * 26 + c];
        if (value == page.original[r * 26 + c])
          continue;
        auto old = cells.find(c);
        if (old != cells.end()) {
          const auto &cn = xml.nodes[old->second];
          int fi = xml.child(old->second, "f");
          if (fi >= 0 && !xml.nodes[fi].attr("t").empty())
            return fail(error, "Shared, array and table formula cells are "
                               "preserved but cannot be edited here.");
          std::string extra;
          for (int child : cn.children) {
            auto name = local_name(xml.nodes[child].name);
            if (name != "f" && name != "v" && name != "is")
              extra +=
                  source.substr(xml.nodes[child].start,
                                xml.nodes[child].end - xml.nodes[child].start);
          }
          edits.push_back(
              {cn.start, cn.end,
               cell_xml(address(r, c), value, &cn, prefix(cn.name), extra)});
        } else {
          size_t position = rn.empty ? rn.open : rn.close;
          for (const auto &cell : cells)
            if (cell.first > c) {
              position = xml.nodes[cell.second].start;
              break;
            }
          edits.push_back({position, position,
                           cell_xml(address(r, c), value, nullptr, pre)});
        }
      }
      std::string contents = rn.empty
                                 ? patched(source, rn.open, rn.open, edits)
                                 : patched(source, rn.open, rn.close, edits);
      rows.push_back({rn.start, rn.end, expanded(xml, row, contents)});
    }
    if (rows.empty()) {
      out = source;
      return true;
    }
    std::string contents = data.empty
                               ? patched(source, data.open, data.open, rows)
                               : patched(source, data.open, data.close, rows);
    out = source.substr(0, data.start) + expanded(xml, di, contents) +
          source.substr(data.end);
    return true;
  }
  bool encode(std::string &bytes, std::vector<Part> &candidate,
              std::string &error) const {
    candidate = parts;
    for (const auto &page : pages) {
      std::string xml;
      if (!update_page(page, xml, error))
        return false; // Expand existing used-range metadata so readers in
                      // streaming mode see new cells.
      Xml result(xml);
      if (!result.parse(error))
        return false;
      int dimension = result.child(0, "dimension");
      if (dimension >= 0 && xml != parts[part(page.path)].bytes) {
        int maxr = 0, maxc = 0;
        const auto old_ref = result.nodes[dimension].attr("ref");
        size_t colon = old_ref.find(':');
        int rr = 0, cc = 0;
        if (coordinate(colon == old_ref.npos ? old_ref
                                             : old_ref.substr(colon + 1),
                       rr, cc)) {
          maxr = rr;
          maxc = cc;
        }
        for (const auto &n : result.nodes)
          if (local_name(n.name) == "c" && coordinate(n.attr("r"), rr, cc)) {
            maxr = std::max(maxr, rr);
            maxc = std::max(maxc, cc);
          }
        int column = maxc + 1;
        std::string letters;
        while (column) {
          --column;
          letters.insert(letters.begin(), char('A' + column % 26));
          column /= 26;
        }
        const auto &d = result.nodes[dimension];
        xml = patched(xml, 0, xml.size(),
                      {{d.start, d.end,
                        "<" + prefix(d.name) + "dimension ref=\"A1:" + letters +
                            std::to_string(maxr + 1) + "\"/>"}});
      }
      candidate[part(page.path)].bytes = std::move(xml);
    }
    // Excel recalculates formulas on opening. Preserve unsupported workbook
    // XML.
    int wi = part(workbook_path);
    Xml book(candidate[wi].bytes);
    if (!book.parse(error))
      return false;
    int calc = book.child(0, "calcPr");
    const auto &root = book.nodes[0];
    std::string
        pre = prefix(root.name),
        setting =
            "<" + pre +
            "calcPr calcId=\"0\" fullCalcOnLoad=\"1\" forceFullCalc=\"1\"/>";
    std::vector<Edit> edits;
    if (calc >= 0) {
      const auto &n = book.nodes[calc];
      setting = "<" + n.name;
      for (const auto &a : n.attrs)
        if (a.first != "calcId" && a.first != "fullCalcOnLoad" &&
            a.first != "forceFullCalc")
          setting += " " + a.first + "=\"" + escape(a.second) + "\"";
      setting += " calcId=\"0\" fullCalcOnLoad=\"1\" forceFullCalc=\"1\"/>";
      edits.push_back({n.start, n.end, setting});
    } else {
      size_t position = root.close;
      for (int child : root.children)
        if (local_name(book.nodes[child].name) == "extLst")
          position = book.nodes[child].start;
      edits.push_back({position, position, setting});
    }
    candidate[wi].bytes = patched(book.text, 0, book.text.size(), edits);
    // Calculation chains are disposable caches. Remove them after edits so
    // Excel rebuilds dependencies instead of using references to old formulas.
    int relationships_index = part(rels_path),
        types_index = part("[Content_Types].xml");
    if (relationships_index >= 0 && types_index >= 0) {
      Xml relationships(candidate[relationships_index].bytes);
      if (!relationships.parse(error))
        return false;
      std::vector<Edit> remove;
      std::vector<std::string> obsolete;
      for (int child : relationships.nodes[0].children) {
        const auto &n = relationships.nodes[child];
        if (n.attr("Type").ends_with("/calcChain")) {
          const auto path = resolve(workbook_path, n.attr("Target"));
          int index = part(path);
          if (index < 0)
            return fail(error, "Calculation-chain part is missing.");
          Xml chain(candidate[index].bytes);
          if (!chain.parse(error) ||
              local_name(chain.nodes[0].name) != "calcChain")
            return fail(error, "Invalid calculation-chain part.");
          obsolete.push_back(path);
          remove.push_back({n.start, n.end, ""});
        }
      }
      if (!obsolete.empty()) {
        std::string next_relationships =
            patched(relationships.text, 0, relationships.text.size(), remove);
        Xml types(candidate[types_index].bytes);
        if (!types.parse(error))
          return false;
        remove.clear();
        for (int child : types.nodes[0].children) {
          const auto &n = types.nodes[child];
          for (const auto &path : obsolete)
            if (n.attr("PartName") == "/" + path)
              remove.push_back({n.start, n.end, ""});
        }
        candidate[types_index].bytes =
            patched(types.text, 0, types.text.size(), remove);
        candidate[relationships_index].bytes = std::move(next_relationships);
        candidate.erase(
            std::remove_if(candidate.begin(), candidate.end(),
                           [&](const Part &p) {
                             return std::find(obsolete.begin(), obsolete.end(),
                                              p.name) != obsolete.end();
                           }),
            candidate.end());
      }
    }
    return zip(candidate, bytes, error);
  }

  bool add_sheet(std::string &error) {
    if (pages.size() >= 16)
      return fail(error, "The 16-sheet workbook limit has been reached.");
    int wi = part(workbook_path), ri = part(rels_path),
        ti = part("[Content_Types].xml");
    if (wi < 0 || ri < 0 || ti < 0)
      return fail(error, "Required workbook parts are missing.");
    Xml book(parts[wi].bytes), rels(parts[ri].bytes), types(parts[ti].bytes);
    if (!book.parse(error) || !rels.parse(error) || !types.parse(error))
      return false;
    int si = book.child(0, "sheets");
    if (si < 0)
      return false;
    int sheet_id = 0;
    for (int child : book.nodes[si].children) {
      int id = 0;
      if (positive(book.nodes[child].attr("sheetId"), id, 1000000000))
        sheet_id = std::max(sheet_id, id);
    }
    std::string name;
    for (int i = 1; i < 1000; ++i) {
      name = "Sheet" + std::to_string(i);
      bool found = false;
      for (const auto &page : pages) {
        std::string candidate = page.name;
        for (char &c : candidate)
          c = char(std::tolower((unsigned char)c));
        std::string wanted = name;
        for (char &c : wanted)
          c = char(std::tolower((unsigned char)c));
        if (candidate == wanted)
          found = true;
      }
      if (!found)
        break;
    }
    std::string id, target, path;
    for (int i = 1; i < 1000; ++i) {
      id = "winbrew" + std::to_string(i);
      target = "worksheets/winbrew_sheet" + std::to_string(i) + ".xml";
      path = resolve(workbook_path, target);
      bool taken = part(path) >= 0;
      for (int child : rels.nodes[0].children)
        if (rels.nodes[child].attr("Id") == id)
          taken = true;
      if (!taken)
        break;
      if (i == 999)
        return false;
    }
    const auto &sheets = book.nodes[si];
    std::string pre = prefix(sheets.name),
                entry = "<" + pre + "sheet name=\"" + name + "\" sheetId=\"" +
                        std::to_string(sheet_id + 1) + "\"";
    std::string rel_attribute = "r:id";
    bool namespace_found = false;
    for (const auto &a : book.nodes[0].attrs)
      if (a.second == "http://schemas.openxmlformats.org/officeDocument/2006/"
                      "relationships" &&
          a.first.find("xmlns:") == 0) {
        rel_attribute = a.first.substr(6) + ":id";
        namespace_found = true;
      }
    entry += " " + rel_attribute + "=\"" + id + "\"";
    if (!namespace_found)
      entry += " xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/"
               "2006/relationships\"";
    entry += "/>";
    std::string contents =
        sheets.empty
            ? ""
            : book.text.substr(sheets.open, sheets.close - sheets.open);
    contents += entry;
    std::string next_book =
        patched(book.text, 0, book.text.size(),
                {{sheets.start, sheets.end, expanded(book, si, contents)}});
    const auto &rr = rels.nodes[0];
    pre = prefix(rr.name);
    entry = "<" + pre + "Relationship Id=\"" + id +
            "\" "
            "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/"
            "relationships/worksheet\" Target=\"" +
            target + "\"/>";
    contents =
        (rr.empty ? "" : rels.text.substr(rr.open, rr.close - rr.open)) + entry;
    std::string next_rels = expanded(rels, 0, contents);
    const auto &tr = types.nodes[0];
    pre = prefix(tr.name);
    entry = "<" + pre + "Override PartName=\"/" + path +
            "\" "
            "ContentType=\"application/"
            "vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>";
    contents =
        (tr.empty ? "" : types.text.substr(tr.open, tr.close - tr.open)) +
        entry;
    std::string next_types = expanded(types, 0, contents);
    parts[wi].bytes = std::move(next_book);
    parts[ri].bytes = std::move(next_rels);
    parts[ti].bytes = std::move(next_types);
    parts.push_back({path, "<?xml version=\"1.0\"?><worksheet xmlns=\"" +
                               std::string(Main) +
                               "\"><sheetData/></worksheet>"});
    Page page;
    page.name = name;
    page.path = path;
    pages.push_back(std::move(page));
    changed = true;
    return true;
  }
  void committed(std::vector<Part> next, std::string bytes) {
    parts = std::move(next);
    original_bytes = std::move(bytes);
    for (auto &p : pages)
      p.original = p.cells;
    changed = false;
  }
  static Workbook blank() {
    Workbook b;
    b.workbook_path = "xl/workbook.xml";
    b.rels_path = "xl/_rels/workbook.xml.rels";
    b.parts = {
        {"[Content_Types].xml",
         "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Types "
         "xmlns=\"http://schemas.openxmlformats.org/package/2006/"
         "content-types\"><Default Extension=\"rels\" "
         "ContentType=\"application/"
         "vnd.openxmlformats-package.relationships+xml\"/><Default "
         "Extension=\"xml\" ContentType=\"application/xml\"/><Override "
         "PartName=\"/xl/workbook.xml\" "
         "ContentType=\"application/"
         "vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/"
         "><Override PartName=\"/xl/worksheets/sheet1.xml\" "
         "ContentType=\"application/"
         "vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/></"
         "Types>"},
        {"_rels/.rels", "<?xml version=\"1.0\"?><Relationships "
                        "xmlns=\"http://schemas.openxmlformats.org/package/"
                        "2006/relationships\"><Relationship Id=\"rId1\" "
                        "Type=\"http://schemas.openxmlformats.org/"
                        "officeDocument/2006/relationships/officeDocument\" "
                        "Target=\"xl/workbook.xml\"/></Relationships>"},
        {b.workbook_path,
         "<?xml version=\"1.0\"?><workbook xmlns=\"" + std::string(Main) +
             "\" "
             "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/"
             "relationships\"><sheets><sheet name=\"Sheet1\" sheetId=\"1\" "
             "r:id=\"rId1\"/></sheets></workbook>"},
        {b.rels_path, "<?xml version=\"1.0\"?><Relationships "
                      "xmlns=\"http://schemas.openxmlformats.org/package/2006/"
                      "relationships\"><Relationship Id=\"rId1\" "
                      "Type=\"http://schemas.openxmlformats.org/officeDocument/"
                      "2006/relationships/worksheet\" "
                      "Target=\"worksheets/sheet1.xml\"/></Relationships>"},
        {"xl/worksheets/sheet1.xml",
         "<?xml version=\"1.0\"?><worksheet xmlns=\"" + std::string(Main) +
             "\"><sheetData/></worksheet>"}};
    Page page;
    page.name = "Sheet1";
    page.path = "xl/worksheets/sheet1.xml";
    b.pages.push_back(std::move(page));
    return b;
  }
};
} // namespace ww::xlsx

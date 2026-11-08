// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/notepad_document.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
static std::string read(const char *p) {
  std::ifstream f(p, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(f), {});
}
static void write(const char *p, const std::string &s) {
  std::ofstream f(p, std::ios::binary);
  f.write(s.data(), s.size());
  assert(f.good());
}
int main() {
  namespace fs = std::filesystem;
  fs::create_directories("build/office-host/xlsx");
  auto m = std::make_unique<ww::SheetDocument>();
  if (!m->load("tests/fixtures/office-interop.xlsx")) {
    std::fprintf(stderr, "XLSX load: %s\n", m->status);
    return 2;
  }
  assert(m->xlsx_format && m->workbook.pages.size() == 4);
  assert(m->cells[26] == "Tea & <cake>");
  assert(m->cells[2] == "=literal text");
  assert(m->display(0, 2) == "=literal text");
  assert(m->cells[28] == "_x0041_ literal");
  assert(m->cells[3] == "Rich text");
  assert(m->display(3, 1) == "20");
  assert(m->display(4, 1) == "42 (cached)");
  assert(!m->dirty());
  m->row=4;m->column=2;assert(m->cells[4*26+2].find('\0')!=std::string::npos);assert(!m->editable());
  m->row = 3;
  m->column = 2;
  assert(!m->editable());
  assert(!m->set(3, 2, "shortened"));
  assert(m->select_sheet(2));
  m->row = 1;
  m->column = 1;
  assert(!m->editable());
  assert(!m->set(1, 1, "changed"));
  assert(m->select_sheet(3));
  assert(!m->set(0, 0, "changed"));
  // Work on an independent copy and preserve the synthetic source fixture.
  write("build/office-host/xlsx/edited.xlsx",
        read("tests/fixtures/office-interop.xlsx"));
  assert(m->load("build/office-host/xlsx/edited.xlsx"));
  assert(m->set(1, 1, "25"));
  assert(m->display(3, 1) == "32.5");
  assert(m->set(99, 25, "New final cell"));
  assert(m->set(10, 0, "_x0041_ literal & <xml>"));
  assert(m->set(11, 0, "  spaced text  "));
  assert(m->select_sheet(1));
  assert(m->set(0, 1, "Second-sheet edit"));
  assert(m->dirty());
  assert(m->save());
  assert(!m->dirty());
  fs::remove("build/office-host/xlsx/full-copy.xlsx");
  assert(m->export_xlsx("build/office-host/xlsx/full-copy.xlsx"));
  assert(m->workbook.pages.size() == 4 && m->selected_sheet == 1);
  auto reloaded = std::make_unique<ww::SheetDocument>();
  assert(reloaded->load("build/office-host/xlsx/edited.xlsx"));
  assert(reloaded->cells[99 * 26 + 25] == "New final cell");
  assert(reloaded->cells[10 * 26] == "_x0041_ literal & <xml>");
  assert(reloaded->cells[11 * 26] == "  spaced text  ");
  assert(reloaded->select_sheet(1));
  assert(reloaded->cells[1] == "Second-sheet edit");
  // Every precommit failure leaves original bytes and dirty edits available.
  for (int stage = 0; stage < 8; ++stage) {
    assert(reloaded->set(0, 1, ("Failure " + std::to_string(stage)).c_str()));
    std::string before = read("build/office-host/xlsx/edited.xlsx");
    auto io = ww::document_io();
    if (stage == 0)
      io.create = [](void *, const char *, unsigned) { return -5; };
    if (stage == 1)
      io.write = [](void *, int, const char *, size_t) -> int64_t {
        return -5;
      };
    if (stage == 2)
      io.permissions = [](void *, int, unsigned) { return -5; };
    if (stage == 3)
      io.sync = [](void *, int) { return -5; };
    if (stage == 4)
      io.close = [](void *, int fd) {
        ww::document_io().close(nullptr, fd);
        return -5;
      };
    if (stage == 5)
      io.replace = [](void *, const char *, const char *) { return -5; };
    if(stage==6)io.write=[](void*,int,const char*,size_t)->int64_t{return 0;};
    if(stage==7)io.write=[](void*,int fd,const char*,size_t count)->int64_t{ww::document_io().write(nullptr,fd,"!",1);return int64_t(count);};
    assert(!reloaded->save(&io));
    assert(reloaded->dirty());
    assert(read("build/office-host/xlsx/edited.xlsx") == before);
  }
  assert(reloaded->save());
  assert(reloaded->set(0, 1, "Conflict"));
  write("build/office-host/xlsx/edited.xlsx", "external change");
  assert(!reloaded->save());
  assert(reloaded->storage.result == ww::DocumentResult::conflict);
  assert(read("build/office-host/xlsx/edited.xlsx") == "external change");
  // Restore a successful copy for independent OpenPyXL verification.
  write("build/office-host/xlsx/edited.xlsx", m->workbook.original_bytes);
  std::string source = read("tests/fixtures/office-interop.xlsx"), error;
  ww::xlsx::Workbook good;
  assert(good.load(source, error));
  // A saved calculation chain is discarded, preserving the formula cells.
  auto chain=good.parts;
  int rel=good.part("xl/_rels/workbook.xml.rels"),types=good.part("[Content_Types].xml");
  auto insert=[](std::string&xml,const char*end,const char*node){xml.insert(xml.rfind(end),node);};
  insert(chain[rel].bytes,"</Relationships>","<Relationship Id=\"calc\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/calcChain\" Target=\"calcChain.xml\"/>");
  insert(chain[types].bytes,"</Types>","<Override PartName=\"/xl/calcChain.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.calcChain+xml\"/>");
  chain.push_back({"xl/calcChain.xml","<calcChain xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\"><c r=\"B4\" i=\"1\"/></calcChain>"});
  std::string encoded;assert(ww::xlsx::zip(chain,encoded,error));write("build/office-host/xlsx/chain.xlsx",encoded);
  auto chained=std::make_unique<ww::SheetDocument>();assert(chained->load("build/office-host/xlsx/chain.xlsx"));assert(chained->set(1,1,"9"));assert(chained->save());assert(chained->workbook.part("xl/calcChain.xml")<0);
  // Malformed archives cannot replace the already-open sheet.
  std::vector<std::string> broken = {
      "not a workbook", source.substr(0, source.size() / 2), source};
  broken.back()[0] = '!';
  for (size_t i = 0; i < broken.size(); ++i) {
    write("build/office-host/xlsx/broken.xlsx", broken[i]);
    auto old = m->cells;
    assert(!m->load("build/office-host/xlsx/broken.xlsx"));
    assert(m->cells == old);
  }
  std::vector<ww::xlsx::Part> bad = good.parts;
  bad.push_back(bad[0]);
  std::string bytes;
  assert(ww::xlsx::zip(bad, bytes, error));
  write("build/office-host/xlsx/duplicate.xlsx", bytes);
  assert(!m->load("build/office-host/xlsx/duplicate.xlsx"));
  bad = good.parts;
  bad[0].name = "../outside";
  assert(ww::xlsx::zip(bad, bytes, error));
  write("build/office-host/xlsx/path.xlsx", bytes);
  assert(!m->load("build/office-host/xlsx/path.xlsx"));
  bad = good.parts;
  bad[good.part("xl/worksheets/sheet1.xml")].bytes =
      "<!DOCTYPE worksheet [<!ENTITY e SYSTEM "
      "'file:///x'>]><worksheet>&e;</worksheet>";
  assert(ww::xlsx::zip(bad, bytes, error));
  write("build/office-host/xlsx/entity.xlsx", bytes);
  assert(!m->load("build/office-host/xlsx/entity.xlsx"));
  const char *csv = "build/office-host/xlsx/source.csv";
  write(csv, "Label,Value\r\nA,2\r\nB,=B2*3\r\n");
  auto converted = std::make_unique<ww::SheetDocument>();
  assert(converted->load(csv));
  fs::remove("build/office-host/xlsx/converted.xlsx");
  assert(converted->export_xlsx("build/office-host/xlsx/converted.xlsx"));
  assert(converted->add_sheet());
  assert(converted->set(0, 0, "Added worksheet"));
  assert(converted->save());
  assert(read(csv) == "Label,Value\r\nA,2\r\nB,=B2*3\r\n");
  std::puts("PASS: compressed XLSX, shared/inline text, formulas, four "
            "worksheets, protected/array cells, atomic failure retention, "
            "conflicts, malformed ZIP/XML rejection and CSV export.");
}

#include "../src/notepad_document.hpp"
#include <filesystem>
#include <fstream>
#include <cassert>
#include <chrono>
#include <iostream>
namespace fs=std::filesystem;void put(const fs::path&p,const std::string&s){std::ofstream(p,std::ios::binary)<<s;}std::string get(const fs::path&p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(){auto root=fs::absolute(fs::path("build")/("saveas-model-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())));fs::create_directories(root);const std::string bom="\xef\xbb\xbf";put(root/"original.txt",bom+"A\r\n");auto d=std::make_unique<ww::TextDocument>();auto target=std::make_unique<ww::TextDocument>();assert(d->load((root/"original.txt").string().c_str()));d->end(true);assert(d->insert('B'));assert(d->save_as((root/"new.txt").string().c_str()));assert(get(root/"new.txt")==bom+"A\r\nB"&&get(root/"original.txt")==bom+"A\r\n");assert(d->undo()&&std::string(d->text)=="A\r\n"&&d->dirty());assert(d->redo()&&std::string(d->text)=="A\r\nB"&&!d->dirty());assert(d->insert('C')&&d->save());assert(get(root/"new.txt")==bom+"A\r\nBC");
 put(root/"replace.txt","BOM-free target");assert(target->load((root/"replace.txt").string().c_str()));assert(!d->save_as((root/"replace.txt").string().c_str()));assert(get(root/"replace.txt")=="BOM-free target");assert(d->save_as((root/"replace.txt").string().c_str(),target.get()));assert(get(root/"replace.txt")==bom+"A\r\nBC");
 assert(d->insert('D'));assert(target->load((root/"replace.txt").string().c_str()));auto io=ww::document_io();io.write=[](void*,int,const char*,size_t)->int64_t{return -EIO;};assert(!d->save_as((root/"replace.txt").string().c_str(),target.get(),&io));assert(d->dirty()&&get(root/"replace.txt")==bom+"A\r\nBC");
 put(root/"empty.txt","");assert(d->load((root/"empty.txt").string().c_str(),true));assert(d->save_as((root/"empty-copy.txt").string().c_str())&&get(root/"empty-copy.txt").empty());
 assert(root.parent_path()==fs::absolute("build"));fs::remove_all(root);std::cout<<"PASS: Save As preserves BOM/CRLF, reserves new names, requires target confirmation, retains original on failure and saves empty documents\n";}

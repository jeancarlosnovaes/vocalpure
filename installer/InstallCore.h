#pragma once
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace vocalpure::install {
namespace fs = std::filesystem;
struct Entry { fs::path relative; std::string hash; };
struct Payload { fs::path bundle; std::vector<Entry> entries; };
struct Locations { fs::path target, backups; };
struct Result { fs::path backup; std::wstring warning; };
std::string sha256(const fs::path&);
std::vector<Entry> inventory(const fs::path&);
void writeInventory(const fs::path& manifest,const std::vector<Entry>&);
Payload loadPayload(const fs::path& bundle);
void verify(const Payload&);
std::wstring uniqueName();
// beforeCommit is empty in production; tests inject a failed final rename.
Result install(const Payload&,const Locations&,const std::function<void()>& beforeCommit = {});
Result restorePrevious(const Locations&);
}

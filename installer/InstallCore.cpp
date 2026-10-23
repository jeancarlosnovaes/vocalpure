#include "InstallCore.h"
#include <windows.h>
#include <bcrypt.h>
#include <objbase.h>
#include <algorithm>
#include <array>
#include <fstream>
#include <map>
#include <stdexcept>
#include <cwctype>
#include <exception>

namespace vocalpure::install {
static void demand(bool ok,const char* text) { if(!ok) throw std::runtime_error(text); }
static bool present(const fs::path& p) { return fs::exists(p); }
class TransactionLock
{
public:
    TransactionLock()
    {
        handle=CreateMutexW(nullptr,FALSE,L"Local\\VocalPureInstallerTransaction");
        demand(handle!=nullptr,"Cannot create installation lock");
        const auto wait=WaitForSingleObject(handle,0);
        if(wait!=WAIT_OBJECT_0 && wait!=WAIT_ABANDONED)
        { CloseHandle(handle); handle=nullptr; throw std::runtime_error("Outra instalacao do VocalPure esta em andamento."); }
    }
    ~TransactionLock() { if(handle) { ReleaseMutex(handle); CloseHandle(handle); } }
    TransactionLock(const TransactionLock&)=delete;
    TransactionLock& operator=(const TransactionLock&)=delete;
private:
    HANDLE handle=nullptr;
};

// Never traverse junctions/symlinks, including in the parents of a destination.
static void plainPath(const fs::path& p)
{
    for(auto at=fs::absolute(p).lexically_normal(); !at.empty();)
    {
        const auto attrs=GetFileAttributesW(at.c_str());
        if(attrs!=INVALID_FILE_ATTRIBUTES) demand((attrs&FILE_ATTRIBUTE_REPARSE_POINT)==0,"Links/junctions are not allowed in installer paths");
        else demand(GetLastError()==ERROR_FILE_NOT_FOUND || GetLastError()==ERROR_PATH_NOT_FOUND,"Cannot inspect installer path");
        const auto parent=at.parent_path(); if(parent==at) break; at=parent;
    }
}
static std::wstring key(const fs::path& p)
{
    auto s=fs::absolute(p).lexically_normal().generic_wstring();
    std::transform(s.begin(),s.end(),s.begin(),[](wchar_t c){return wchar_t(std::towlower(c));});
    return s;
}
static bool within(const fs::path& p,const fs::path& parent)
{
    auto prefix=key(parent); if(prefix.back()!=L'/') prefix+=L'/';
    return key(p).starts_with(prefix);
}
static void validRelative(const fs::path& p)
{
    demand(!p.empty() && !p.has_root_path(),"Payload path must be relative");
    for(const auto& part:p) demand(part!=L".." && part!=L"." && part.wstring().find_first_of(L":\r\n") == std::wstring::npos,"Invalid payload path");
}
std::wstring uniqueName()
{
    GUID value{}; demand(SUCCEEDED(CoCreateGuid(&value)),"Cannot generate transaction ID");
    wchar_t buffer[40]{}; demand(StringFromGUID2(value,buffer,40)>0,"Cannot format transaction ID");
    std::wstring text(buffer); return text.substr(1,text.size()-2);
}
std::string sha256(const fs::path& file)
{
    plainPath(file);
    std::ifstream in(file,std::ios::binary); demand(in.good(),"Cannot read file for checksum");
    BCRYPT_ALG_HANDLE algorithm=nullptr; BCRYPT_HASH_HANDLE hash=nullptr;
    demand(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0,"Cannot initialise SHA256");
    try
    {
        demand(BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0,"Cannot create SHA256 state");
        std::array<char,65536> bytes{};
        while(in)
        {
            in.read(bytes.data(),bytes.size()); const auto n=in.gcount();
            if(n>0) demand(BCryptHashData(hash,reinterpret_cast<PUCHAR>(bytes.data()),ULONG(n),0)>=0,"SHA256 update failed");
        }
        demand(in.eof(),"File read failed during checksum");
        std::array<unsigned char,32> digest{};
        demand(BCryptFinishHash(hash,digest.data(),ULONG(digest.size()),0)>=0,"SHA256 finish failed");
        constexpr char hex[]="0123456789abcdef"; std::string result;
        for(auto byte:digest) { result+=hex[byte>>4]; result+=hex[byte&15]; }
        BCryptDestroyHash(hash); BCryptCloseAlgorithmProvider(algorithm,0); return result;
    }
    catch(...) { if(hash) BCryptDestroyHash(hash); BCryptCloseAlgorithmProvider(algorithm,0); throw; }
}
std::vector<Entry> inventory(const fs::path& bundle)
{
    plainPath(bundle); demand(fs::is_directory(bundle),"Missing VST3 bundle directory");
    std::vector<Entry> result;
    for(const auto& item:fs::recursive_directory_iterator(bundle))
    {
        plainPath(item.path());
        if(item.is_directory()) continue;
        demand(item.is_regular_file(),"Unsupported file type in VST3 bundle");
        const auto relative=item.path().lexically_relative(bundle); validRelative(relative);
        result.push_back({relative,sha256(item.path())});
    }
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.relative<b.relative;});
    demand(!result.empty(),"VST3 bundle is empty"); return result;
}
void writeInventory(const fs::path& manifest,const std::vector<Entry>& entries)
{
    plainPath(manifest); demand(!present(manifest),"Checksum manifest already exists");
    std::ofstream out(manifest,std::ios::binary); demand(out.good(),"Cannot create checksum manifest");
    out<<"# VocalPure VST3 SHA256\n";
    for(const auto& e:entries) { const auto path=e.relative.generic_u8string(); out<<e.hash<<"  "<<std::string(path.begin(),path.end())<<'\n'; }
    out.close(); demand(out.good(),"Cannot finish checksum manifest");
}
void verify(const Payload& payload)
{
    demand(!payload.entries.empty(),"Missing payload checksums");
    std::map<std::wstring,std::string> expected;
    for(const auto& e:payload.entries)
    {
        validRelative(e.relative);
        demand(e.hash.size()==64 && std::all_of(e.hash.begin(),e.hash.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');}),"Invalid SHA256 entry");
        demand(expected.emplace(key(payload.bundle/e.relative),e.hash).second,"Duplicate checksum path");
    }
    const auto actual=inventory(payload.bundle);
    demand(actual.size()==expected.size(),"Payload contains missing or unexpected files");
    for(const auto& e:actual)
    {
        const auto found=expected.find(key(payload.bundle/e.relative));
        demand(found!=expected.end() && found->second==e.hash,"Payload checksum mismatch");
    }
    demand(present(payload.bundle/L"Contents/x86_64-win/VocalPure.vst3"),"Missing Windows x64 plugin binary");
}
Payload loadPayload(const fs::path& bundle)
{
    const auto manifest=bundle.parent_path()/L"payload.sha256"; plainPath(manifest);
    std::ifstream in(manifest,std::ios::binary); demand(in.good(),"Arquivo payload.sha256 ausente. Extraia todo o ZIP antes de instalar.");
    Payload result{bundle,{}}; std::string line;
    while(std::getline(in,line))
    {
        if(!line.empty() && line.back()=='\r') line.pop_back();
        if(line.empty() || line[0]=='#') continue;
        demand(line.size()>66 && line.substr(64,2)=="  ","Malformed checksum manifest");
        const auto bytes=line.substr(66);
        result.entries.push_back({fs::path(std::u8string(bytes.begin(),bytes.end())),line.substr(0,64)});
    }
    demand(in.eof(),"Cannot read checksum manifest"); verify(result); return result;
}
static void locations(const Locations& paths)
{
    demand(paths.target.is_absolute() && paths.backups.is_absolute(),"Installer destinations must be absolute");
    demand(paths.target.filename()==L"VocalPure.vst3","Installer only replaces VocalPure.vst3");
    demand(!within(paths.backups,paths.target) && !within(paths.target,paths.backups) && key(paths.target)!=key(paths.backups),"Backup and plugin paths overlap");
    plainPath(paths.target); plainPath(paths.backups);
}
static void removeTransaction(const fs::path& path,const fs::path& parent,const std::wstring& prefix)
{
    // Every recursive removal has an exact, resolved direct-parent/prefix check.
    demand(key(path.parent_path())==key(parent) && path.filename().wstring().starts_with(prefix),"Unsafe transaction cleanup path");
    plainPath(path);
    if(present(path)) { (void)inventory(path); fs::remove_all(path); }
}
static void unlocked(const fs::path& target)
{
    if(!present(target)) return;
    // A loaded plugin image cannot grant this exclusive DELETE-capable handle.
    const auto binary=target/L"Contents/x86_64-win/VocalPure.vst3";
    demand(present(binary),"Existing destination is not a VocalPure Windows bundle");
    HANDLE handle=CreateFileW(binary.c_str(),GENERIC_READ|DELETE,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    demand(handle!=INVALID_HANDLE_VALUE,"O plugin esta em uso ou sem acesso. Feche o editor de audio antes de instalar.");
    CloseHandle(handle);
}
static void copyChecked(const Payload& source,const fs::path& destination)
{
    demand(!present(destination),"Transaction destination already exists"); plainPath(destination);
    fs::create_directories(destination);
    for(const auto& e:source.entries)
    {
        const auto out=destination/e.relative; plainPath(out); fs::create_directories(out.parent_path());
        fs::copy_file(source.bundle/e.relative,out,fs::copy_options::none);
    }
    verify({destination,source.entries});
}
Result install(const Payload& source,const Locations& paths,const std::function<void()>& beforeCommit)
{
    const TransactionLock transactionLock;
    locations(paths); verify(source); unlocked(paths.target);
    demand(!within(source.bundle,paths.target) && !within(paths.target,source.bundle) && key(source.bundle)!=key(paths.target),"Source and installed plugin paths overlap");
    fs::create_directories(paths.target.parent_path()); fs::create_directories(paths.backups);
    const auto token=uniqueName(); const auto parent=paths.target.parent_path();
    const auto stage=parent/(L"VocalPure.staging-"+token);
    const auto holding=parent/(L"VocalPure.previous-"+token);
    demand(!present(stage) && !present(holding),"Transaction path already exists");
    Result result; bool movedOriginal=false;
    try
    {
        copyChecked(source,stage);
        if(present(paths.target))
        {
            const Payload old{paths.target,inventory(paths.target)};
            SYSTEMTIME now{}; GetSystemTime(&now); wchar_t stamp[40]{};
            swprintf_s(stamp,L"backup-%04u%02u%02u-%02u%02u%02u%03u-",now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,now.wMilliseconds);
            result.backup=paths.backups/(std::wstring(stamp)+token);
            demand(!present(result.backup),"Backup already exists"); fs::create_directory(result.backup);
            copyChecked(old,result.backup/L"VocalPure.vst3");
            writeInventory(result.backup/L"payload.sha256",old.entries);
            // Verify exact paths before each directory move. Both are siblings.
            plainPath(paths.target); plainPath(holding);
            demand(key(holding.parent_path())==key(parent),"Unsafe rollback directory");
            fs::rename(paths.target,holding); movedOriginal=true;
        }
        if(beforeCommit) beforeCommit();
        plainPath(stage); plainPath(paths.target);
        demand(key(stage.parent_path())==key(parent) && !present(paths.target),"Unsafe install commit destination");
        fs::rename(stage,paths.target);
    }
    catch(...)
    {
        const auto originalError=std::current_exception();
        if(movedOriginal)
        {
            plainPath(holding); plainPath(paths.target);
            demand(key(holding.parent_path())==key(parent) && !present(paths.target),"Cannot restore automatically; original retained in transaction backup");
            fs::rename(holding,paths.target);
        }
        // An interrupted staging directory can be retained safely if cleanup fails.
        try { if(present(stage)) removeTransaction(stage,parent,L"VocalPure.staging-"); } catch(...) {}
        std::rethrow_exception(originalError);
    }
    if(!result.backup.empty())
    {
        std::ofstream marker(result.backup/L"ready.txt"); marker<<"Previous version verified before replacement.\n"; marker.close();
        if(!marker.good()) result.warning=L"O backup foi salvo, mas nao foi possivel registrar a restauracao automatica.";
    }
    if(movedOriginal)
    {
        try { removeTransaction(holding,parent,L"VocalPure.previous-"); }
        catch(...) { result.warning+=L" Uma pasta temporaria foi preservada em "+holding.wstring(); }
    }
    return result;
}
Result restorePrevious(const Locations& paths)
{
    locations(paths); demand(present(paths.backups),"No previous version backup exists");
    fs::path latest;
    for(const auto& entry:fs::directory_iterator(paths.backups))
    {
        plainPath(entry.path());
        if(entry.is_directory() && entry.path().filename().wstring().starts_with(L"backup-") && present(entry.path()/L"ready.txt"))
            if(latest.empty() || entry.path().filename()>latest.filename()) latest=entry.path();
    }
    demand(!latest.empty(),"No completed previous-version backup exists");
    return install(loadPayload(latest/L"VocalPure.vst3"),paths);
}
}

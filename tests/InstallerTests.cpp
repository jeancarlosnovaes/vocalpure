#include "InstallCore.h"
#include <windows.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace vocalpure::install;
static void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
static void write(const fs::path& file,const std::string& text)
{ fs::create_directories(file.parent_path()); std::ofstream out(file,std::ios::binary); out<<text; out.close(); require(out.good(),"Cannot write fixture"); }
template<class F> static void fails(F&& task)
{ bool failed=false; try { task(); } catch(const std::exception&) { failed=true; } require(failed,"Unsafe operation unexpectedly succeeded"); }
static Payload payload(const fs::path& parent,const char* version)
{
    const auto bundle=parent/L"VocalPure.vst3";
    write(bundle/L"Contents/x86_64-win/VocalPure.vst3",version);
    write(bundle/L"Contents/Resources/moduleinfo.json","{\"fixture\":true}");
    writeInventory(parent/L"payload.sha256",inventory(bundle)); return loadPayload(bundle);
}
int wmain(int argc,wchar_t** argv)
{
    try
    {
        if(argc==3 && std::wstring(argv[1])==L"--verify-payload")
        {
            const auto verified=loadPayload(fs::absolute(fs::path(argv[2])));
            std::cout<<"PASS: packaged VST3 verified by native installer core ("<<verified.entries.size()<<" files). Read-only.\n";
            return 0;
        }
        require(argc==2,"Pass the isolated installer test root");
        const auto root=fs::absolute(fs::path(argv[1]))/(L"run-"+uniqueName());
        require(!fs::exists(root),"Test root must be new"); fs::create_directories(root);
        const auto first=payload(root/L"payload one","first");
        const auto next=payload(root/L"payload two","second");
        const Locations where{root/L"Common Files/VST3/VocalPure.vst3",root/L"Program Data/Backups"};
        const auto binary=where.target/L"Contents/x86_64-win/VocalPure.vst3";
        const auto firstHash=sha256(first.bundle/L"Contents/x86_64-win/VocalPure.vst3");
        const auto nextHash=sha256(next.bundle/L"Contents/x86_64-win/VocalPure.vst3");
        require(install(first,where).backup.empty(),"First install should not invent a backup");
        require(sha256(binary)==firstHash,"First install mismatch");
        write(where.target/L"old-version-only.txt","stale file must not survive update");
        const auto update=install(next,where);
        require(!update.backup.empty() && fs::exists(update.backup/L"ready.txt"),"Verified backup not recorded");
        require(sha256(update.backup/L"VocalPure.vst3/Contents/x86_64-win/VocalPure.vst3")==firstHash,"Backup mismatch");
        require(!fs::exists(where.target/L"old-version-only.txt") && sha256(binary)==nextHash,"Update merged stale files");
        restorePrevious(where);
        require(sha256(binary)==firstHash && fs::exists(where.target/L"old-version-only.txt"),"Rollback did not restore exact prior bundle");
        // Fail after the original has moved, as if the final directory rename failed.
        fails([&]{install(next,where,[]{throw std::runtime_error("Injected commit failure");});});
        require(sha256(binary)==firstHash,"Failed commit damaged the installed plugin");
        // Corruption, missing files, extra files, duplicate paths and path traversal.
        const auto bad=payload(root/L"corrupt","original");
        write(bad.bundle/L"Contents/x86_64-win/VocalPure.vst3","modified");
        fails([&]{install(bad,where);}); require(sha256(binary)==firstHash,"Corrupt payload changed destination");
        auto duplicate=next; duplicate.entries.push_back(duplicate.entries.front()); fails([&]{verify(duplicate);});
        auto traversal=next; traversal.entries.front().relative=L"../outside.txt"; fails([&]{verify(traversal);});
        auto missing=next; missing.entries.pop_back(); fails([&]{verify(missing);});
        const auto extra=payload(root/L"extra","extra"); write(extra.bundle/L"unexpected.txt","extra"); fails([&]{verify(extra);});
        // Refuse an actively locked plugin without moving or replacing it.
        HANDLE locked=CreateFileW(binary.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        require(locked!=INVALID_HANDLE_VALUE,"Cannot create locked-file test");
        try { fails([&]{install(next,where);}); } catch(...) { CloseHandle(locked); throw; }
        CloseHandle(locked); require(sha256(binary)==firstHash,"Locked plugin was replaced");
        HANDLE guard=CreateMutexW(nullptr,FALSE,L"Local\\VocalPureInstallerTransaction");
        require(guard!=nullptr && WaitForSingleObject(guard,0)==WAIT_OBJECT_0,"Cannot acquire concurrency fixture");
        bool busyRejected=false;
        std::thread contender([&] {
            try { install(next,where); }
            catch(const std::exception& e) { busyRejected=std::string(e.what()).find("andamento")!=std::string::npos; }
        });
        contender.join(); ReleaseMutex(guard); CloseHandle(guard);
        require(busyRejected && sha256(binary)==firstHash,"Concurrent install was not refused");
        // Refuse arbitrary folder names and nested backup/source destinations.
        fails([&]{install(next,{root/L"other",where.backups});});
        fails([&]{install(next,{where.target,where.target/L"backups"});});
        fails([&]{install(next,{next.bundle,where.backups});});
        // SHA256 standard known vector and Unicode/space destination support.
        write(root/L"abc.txt","abc");
        require(sha256(root/L"abc.txt")=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","SHA256 known vector failed");
        const Locations unicode{root/fs::path(L"voz \u00e7\u00e3o")/L"VocalPure.vst3",root/L"Unicode backups"};
        install(first,unicode); require(sha256(unicode.target/L"Contents/x86_64-win/VocalPure.vst3")==firstHash,"Unicode installation failed");
        std::cout<<"PASS: isolated first install, exact backup/update/rollback, injected commit failure recovery, SHA256, corrupt/missing/extra/duplicate/traversal rejection, locked file, concurrent operation, destination boundaries, spaces and Unicode. No system installation or host interaction.\n";
        return 0;
    }
    catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n'; return 1; }
}

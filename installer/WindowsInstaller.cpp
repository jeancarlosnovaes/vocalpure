#include "InstallCore.h"
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <memory>
#include <stdexcept>

using namespace vocalpure::install;
static fs::path knownFolder(REFKNOWNFOLDERID id)
{
    PWSTR value=nullptr;
    if(FAILED(SHGetKnownFolderPath(id,0,nullptr,&value))) throw std::runtime_error("Cannot locate Windows folder");
    fs::path result(value); CoTaskMemFree(value); return result;
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int)
{
    try
    {
        wchar_t path[32768]{};
        const auto count=GetModuleFileNameW(nullptr,path,32768);
        if(count==0 || count>=32768) throw std::runtime_error("Cannot locate installer package");
        const auto package=fs::path(path).parent_path();
        const Locations paths{knownFolder(FOLDERID_ProgramFilesCommon)/L"VST3/VocalPure.vst3",
                              knownFolder(FOLDERID_ProgramData)/L"VocalPure/Backups"};
        INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_STANDARD_CLASSES}; InitCommonControlsEx(&controls);
        TASKDIALOG_BUTTON buttons[]={{100,L"Instalar / atualizar"},{101,L"Restaurar versao anterior"},{IDCANCEL,L"Fechar"}};
        TASKDIALOGCONFIG dialog{}; dialog.cbSize=sizeof(dialog); dialog.dwFlags=TDF_SIZE_TO_CONTENT;
        dialog.pszWindowTitle=L"VocalPure - instalador interno";
        dialog.pszMainInstruction=L"Instalar o plugin VocalPure";
        const std::wstring content=L"Feche o Resolve e outros editores de audio antes de continuar.\n\nDestino: "+paths.target.wstring()+
            L"\n\nA atualizacao guarda uma copia verificada da versao anterior. O aplicativo independente permanece na pasta Standalone do pacote.\n\nPacote de teste interno, sem assinatura digital.";
        dialog.pszContent=content.c_str(); dialog.cButtons=3; dialog.pButtons=buttons; dialog.nDefaultButton=IDCANCEL;
        int button=IDCANCEL;
        if(FAILED(TaskDialogIndirect(&dialog,&button,nullptr,nullptr))) throw std::runtime_error("Cannot open installer dialog");
        if(button!=100 && button!=101) return 0;
        const auto result=button==100?install(loadPayload(package/L"VST3/VocalPure.vst3"),paths):restorePrevious(paths);
        std::wstring message=button==100?L"VocalPure instalado. Reabra o Resolve e confira o plugin na lista de efeitos.":L"A versao anterior foi restaurada. Reabra o editor de audio.";
        if(!result.backup.empty()) message+=L"\n\nCopia de seguranca: "+result.backup.wstring();
        if(!result.warning.empty()) message+=L"\n\n"+result.warning;
        MessageBoxW(nullptr,message.c_str(),L"VocalPure",MB_OK|MB_ICONINFORMATION); return 0;
    }
    catch(const std::exception& e)
    {
        const auto n=MultiByteToWideChar(CP_UTF8,0,e.what(),-1,nullptr,0);
        std::wstring error(size_t(n),L'\0'); if(n>0) MultiByteToWideChar(CP_UTF8,0,e.what(),-1,error.data(),n);
        const auto message=L"Nao foi possivel concluir.\n\n"+error+L"\n\nNenhum aplicativo foi fechado automaticamente. Se o plugin estiver em uso, feche o editor e tente novamente.";
        MessageBoxW(nullptr,message.c_str(),L"VocalPure - instalacao",MB_OK|MB_ICONERROR); return 1;
    }
}

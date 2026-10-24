#include "../Installer/Setup.h"
#include <windows.h>
#include <fstream>
#include <filesystem>
#include <iostream>
int main(){
 wchar_t temp[MAX_PATH]{};if(!GetTempPathW(MAX_PATH,temp))return 1;
 auto directory=std::filesystem::path(temp)/(L"TitanTracePackageTests-"+std::to_wstring(GetCurrentProcessId()));
 std::filesystem::create_directory(directory);
 auto file=directory/L"untrusted.cat";{std::ofstream stream(file,std::ios::binary);stream<<"not a trusted catalog";}
 bool passed=!Setup::CatalogTrusted(file.wstring())&&!Setup::PackageMatches(file.wstring());
 auto inf=directory/L"unrelated.inf";{std::ofstream stream(inf);stream<<"[Version]\nSignature=\"$Windows NT$\"\nClass=System\nClassGUID={4D36E97D-E325-11CE-BFC1-08002BE10318}\nProvider=Other\nCatalogFile=Other.cat\n";}
 passed=passed&&!Setup::PackageMatches(inf.wstring());std::filesystem::remove(file);std::filesystem::remove(inf);std::filesystem::remove(directory);
 std::cout<<"Unsigned catalog and unrelated package rejected. No install or removal operation was run.\n";return passed?0:1;
}

// SPDX-License-Identifier: MIT
// Bounded offline LZMA streams only. Never loads or executes Setup/DAC bytes.
// ABI: ip7z/7zip 25.01 CPP/7zip/{ICoder.h,IStream.h}; decoder is the
// is7zxr-x64.dll already covered by the Inno 7.1.0 full inventory lock.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <unknwn.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>
#include <filesystem>
#include <stdexcept>
using U64 = unsigned long long;
struct InStream : IUnknown { virtual HRESULT STDMETHODCALLTYPE Read(void*, ULONG, ULONG*) = 0; };
struct OutStream : IUnknown { virtual HRESULT STDMETHODCALLTYPE Write(const void*, ULONG, ULONG*) = 0; };
struct Coder : IUnknown { virtual HRESULT STDMETHODCALLTYPE Code(InStream*, OutStream*, const U64*, const U64*, IUnknown*) = 0; };
struct Properties : IUnknown { virtual HRESULT STDMETHODCALLTYPE SetDecoderProperties2(const BYTE*, ULONG) = 0; };
struct Finish : IUnknown { virtual HRESULT STDMETHODCALLTYPE SetFinishMode(ULONG) = 0; };
struct Processed : IUnknown { virtual HRESULT STDMETHODCALLTYPE GetInStreamProcessedSize(U64*) = 0; };
static GUID iid(unsigned group, unsigned id) {
    GUID g{0x23170f69,0x40c1,0x278a,{0,0,0,0,0,0,0,0}};
    g.Data4[3] = static_cast<BYTE>(group); g.Data4[5] = static_cast<BYTE>(id); return g;
}
class Input final : public InStream {
public:
    std::vector<BYTE> bytes; size_t at{};
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** p) override {
        if (riid != IID_IUnknown && riid != iid(3,1)) return E_NOINTERFACE;
        *p = static_cast<InStream*>(this); return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
    ULONG STDMETHODCALLTYPE Release() override { return 1; }
    HRESULT STDMETHODCALLTYPE Read(void* p, ULONG n, ULONG* done) override {
        const auto count = static_cast<ULONG>(std::min<size_t>(n, bytes.size()-at));
        if (count) std::memcpy(p,bytes.data()+at,count);
        at += count; if (done) *done=count; return S_OK;
    }
};
class Output final : public OutStream {
public:
    std::vector<BYTE> bytes;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** p) override {
        if (riid != IID_IUnknown && riid != iid(3,2)) return E_NOINTERFACE;
        *p = static_cast<OutStream*>(this); return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
    ULONG STDMETHODCALLTYPE Release() override { return 1; }
    HRESULT STDMETHODCALLTYPE Write(const void* p, ULONG n, ULONG* done) override {
        if (n > 64*1024*1024 - bytes.size()) return E_OUTOFMEMORY;
        const auto b=static_cast<const BYTE*>(p); bytes.insert(bytes.end(),b,b+n);
        if (done) *done=n; return S_OK;
    }
};
static void require(bool ok) { if (!ok) throw std::runtime_error("Offline decoder rejected input."); }
int wmain(int argc, wchar_t** argv) {
    Coder* coder=nullptr; Properties* props=nullptr; Finish* finish=nullptr; Processed* processed=nullptr; HMODULE module=nullptr;
    try {
        require(argc==5 && (wcscmp(argv[4],L"1")==0 || wcscmp(argv[4],L"2")==0));
        const bool lzma2=wcscmp(argv[4],L"2")==0;
        const std::filesystem::path inputPath(argv[2]), outputPath(argv[3]);
        require(inputPath.is_absolute() && outputPath.is_absolute() && std::filesystem::path(argv[1]).is_absolute());
        const auto size=std::filesystem::file_size(inputPath); require(size>5 && size<=32*1024*1024);
        Input input; input.bytes.resize(static_cast<size_t>(size));
        std::ifstream file(inputPath,std::ios::binary); require(static_cast<bool>(file.read(reinterpret_cast<char*>(input.bytes.data()),static_cast<std::streamsize>(size))));
        const ULONG propertySize=lzma2 ? 1u : 5u;
        const BYTE property=input.bytes[0];
        if (lzma2) require(property<=26); // dictionary at most 64 MiB
        else {
            uint32_t dictionary{}; std::memcpy(&dictionary,input.bytes.data()+1,4);
            require(property<225 && dictionary<=64*1024*1024);
        }
        // Search only the pinned DLL's directory and Windows system directory.
        module=LoadLibraryExW(argv[1],nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32); require(module!=nullptr);
        using Create = HRESULT (WINAPI*)(const GUID*, const GUID*, void**);
        const auto create=reinterpret_cast<Create>(GetProcAddress(module,"CreateObject"));
        if (!create) throw std::runtime_error("Pinned decoder entry point is absent.");
        GUID clsid{0x23170f69,0x40c1,0x2790,{0,0,0,0,0,0,0,0}};
        if (lzma2) clsid.Data4[0]=0x21; else { clsid.Data4[0]=1; clsid.Data4[1]=1; clsid.Data4[2]=3; }
        const auto coderIid=iid(4,5), propsIid=iid(4,0x22), finishIid=iid(4,0x26), processedIid=iid(4,0x24);
        require(create(&clsid,&coderIid,reinterpret_cast<void**>(&coder))==S_OK);
        require(coder->QueryInterface(propsIid,reinterpret_cast<void**>(&props))==S_OK);
        require(props->SetDecoderProperties2(input.bytes.data(),propertySize)==S_OK);
        require(coder->QueryInterface(finishIid,reinterpret_cast<void**>(&finish))==S_OK && finish->SetFinishMode(1)==S_OK);
        require(coder->QueryInterface(processedIid,reinterpret_cast<void**>(&processed))==S_OK);
        input.at=propertySize; const U64 compressed=size-propertySize; Output output;
        require(coder->Code(&input,&output,&compressed,nullptr,nullptr)==S_OK && input.at==input.bytes.size());
        // Reading into a decoder buffer does not prove every byte was consumed.
        U64 consumed{}; require(processed->GetInStreamProcessedSize(&consumed)==S_OK && consumed==compressed);
        const HANDLE destination=CreateFileW(argv[3],GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        require(destination!=INVALID_HANDLE_VALUE);
        DWORD written{}; const bool ok=WriteFile(destination,output.bytes.data(),static_cast<DWORD>(output.bytes.size()),&written,nullptr)!=0;
        CloseHandle(destination); require(ok && written==output.bytes.size());
        processed->Release(); finish->Release(); props->Release(); coder->Release(); FreeLibrary(module); return 0;
    } catch (...) {
        if (processed) processed->Release(); if (finish) finish->Release(); if (props) props->Release(); if (coder) coder->Release(); if (module) FreeLibrary(module);
        return 1; // No input strings, local paths, or OS error text on stdout/stderr.
    }
}

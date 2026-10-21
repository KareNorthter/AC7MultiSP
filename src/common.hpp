#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include "json.hpp"
using Json=nlohmann::json;
inline const std::filesystem::path& modRoot() {
    static const auto path=[] {
        HMODULE module{};
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&modRoot),&module);
        wchar_t filename[32768]{};GetModuleFileNameW(module,filename,32768);
        return std::filesystem::path(filename).parent_path().parent_path();
    }();
    return path;
}
inline void nativeLog(const std::string& line) noexcept {
    using Submit=bool(*)(unsigned,const char*,size_t);
    static std::atomic<Submit> sink{nullptr};
    auto submit=sink.load(std::memory_order_acquire);
    if(!submit) {
        const auto core=GetModuleHandleW(L"DualSP.dll");
        if(!core)return;
        submit=reinterpret_cast<Submit>(GetProcAddress(core,"dualsp_log_submit"));
        if(!submit)return;
        sink.store(submit,std::memory_order_release);
    }
    static const unsigned source=[] {
        HMODULE module{};
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&nativeLog),&module);
        return GetProcAddress(module,"dualsp_log_submit")?1u:2u;
    }();
    submit(source,line.data(),line.size()); // 核心和UI只投递，不在调用线程访问磁盘。
}
inline void atomicJson(const std::filesystem::path& file,const Json& value,bool durable=true) {
    auto pending=file;pending+=L".tmp";
    {std::ofstream out(pending,std::ios::binary|std::ios::trunc);out<<value.dump();if(!out)throw std::runtime_error("Cannot write mod data");}
    if(!MoveFileExW(pending.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|(durable?MOVEFILE_WRITE_THROUGH:0)))throw std::runtime_error("Cannot publish mod data");
}

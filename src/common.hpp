#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
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
inline void nativeLog(const std::string& line) {
    static std::mutex lock;std::lock_guard<std::mutex> guard(lock);
    std::ofstream out(modRoot()/"native.log",std::ios::app);out<<GetTickCount64()<<" "<<line<<'\n';
}
inline void atomicJson(const std::filesystem::path& file,const Json& value,bool durable=true) {
    auto pending=file;pending+=L".tmp";
    {std::ofstream out(pending,std::ios::binary|std::ios::trunc);out<<value.dump();if(!out)throw std::runtime_error("Cannot write mod data");}
    if(!MoveFileExW(pending.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|(durable?MOVEFILE_WRITE_THROUGH:0)))throw std::runtime_error("Cannot publish mod data");
}

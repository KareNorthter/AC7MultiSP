#pragma once
#include <Windows.h>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace dualsp {
// 本机 AC7 专用 UE4SS 将 Lua C API 静态链接，未直接导出 lua_*。
// 从已核对的 LuaMadeSimple 导出转发中取真实 C API，不构造其 C++ 对象。
// 前缀、相对调用和目标范围必须一致；其他加载器明确拒绝，不猜测布局。
struct LuaApi {
    // Lua 5.4 的标准栈接口；跨 DLL 只传指针、整数、浮点和字节串。
    using Top = int(*)(void*);
    using Integer = int64_t(*)(void*,int,int*);
    using Number = double(*)(void*,int,int*);
    using Boolean = void(*)(void*,int);
    using String = const char*(*)(void*,const char*,size_t);
    Top top{};
    Integer integer{};
    Number number{};
    Boolean boolean{};
    String string{};

    // 只解析指定导出的一个已知转发指令，禁止全模块模糊扫描。
    static void* target(HMODULE module,const char* name,const unsigned char* prefix,size_t length,unsigned char opcode) {
        const auto entry=reinterpret_cast<const unsigned char*>(GetProcAddress(module,name));
        if(!entry || std::memcmp(entry,prefix,length)!=0 || entry[length]!=opcode)
            throw std::runtime_error(std::string("UE4SS Lua 转发接口不匹配：")+name);
        // 相对分支目标必须仍处于同一模块映像。
        int32_t displacement{};
        std::memcpy(&displacement,entry+length+1,sizeof(displacement));
        const auto destination=entry+length+5+displacement;
        const auto base=reinterpret_cast<const unsigned char*>(module);
        const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        const auto pe=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
        if(destination<base || destination>=base+pe->OptionalHeader.SizeOfImage)
            throw std::runtime_error("UE4SS Lua 转发目标超出模块");
        return const_cast<unsigned char*>(destination);
    }

    // 固定前缀来自本机专用加载器的离线导出反汇编，末尾是 call/jmp rel32。
    LuaApi() {
        const auto module=GetModuleHandleW(L"UE4SS.dll");
        if(!module)throw std::runtime_error("未加载 AC7 专用 UE4SS");
        constexpr unsigned char forward[]={0x48,0x8b,0x09};
        constexpr unsigned char pushBoolean[]={0x48,0x8b,0x09,0x0f,0xb6,0xd2};
        constexpr unsigned char getInteger[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0xf1,0x45,0x33,0xc0,0x48,0x8b,0x09,0x8b,0xfa};
        constexpr unsigned char getNumber[]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x30,0x48,0x8b,0xf9,0x0f,0x29,0x74,0x24,0x20,0x48,0x8b,0x09,0x45,0x33,0xc0,0x8b,0xda};
        top=reinterpret_cast<Top>(target(module,"?get_stack_size@Lua@LuaMadeSimple@RC@@QEBAHXZ",forward,sizeof(forward),0xe9));
        integer=reinterpret_cast<Integer>(target(module,"?get_integer@Lua@LuaMadeSimple@RC@@QEBA_JH@Z",getInteger,sizeof(getInteger),0xe8));
        number=reinterpret_cast<Number>(target(module,"?get_number@Lua@LuaMadeSimple@RC@@QEBANH@Z",getNumber,sizeof(getNumber),0xe8));
        boolean=reinterpret_cast<Boolean>(target(module,"?set_bool@Lua@LuaMadeSimple@RC@@QEBAX_N@Z",pushBoolean,sizeof(pushBoolean),0xe9));
        string=reinterpret_cast<String>(target(module,"?set_string@Lua@LuaMadeSimple@RC@@QEBAXPEBD_K@Z",forward,sizeof(forward),0xe9));
    }

    // 只读参数，不调用 LuaMadeSimple 会弹栈的 getter。
    int64_t readInteger(void* state,int index)const {
        int valid{};
        const auto value=integer(state,index,&valid);
        if(!valid)throw std::runtime_error("HUD 内存接口需要整数参数");
        return value;
    }
    double readNumber(void* state,int index)const {
        int valid{};
        const auto value=number(state,index,&valid);
        if(!valid)throw std::runtime_error("HUD 内存接口需要游戏时间参数");
        return value;
    }
};
}

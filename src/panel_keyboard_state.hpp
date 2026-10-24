#pragma once
// 消息隔离不能代替按键状态隔离。仅屏蔽游戏EXE读取的退格/空格，面板和其他DLL仍读真实状态。
using PanelKeyStateFn=SHORT(WINAPI*)(int);
using PanelKeyboardStateFn=BOOL(WINAPI*)(PBYTE);
PanelKeyStateFn originalPanelAsyncKeyState{},originalPanelKeyState{};
PanelKeyboardStateFn originalPanelKeyboardState{};
std::atomic<unsigned> panelKeyboardTail{0};
std::atomic<unsigned> panelKeyboardMessageHeld{0};
std::atomic<std::uintptr_t> panelGameImageBegin{0},panelGameImageEnd{0};

unsigned panelKeyboardBit(int key) {return key==VK_BACK?1u:key==VK_SPACE?2u:0u;}
bool panelKeyboardGameCaller(void* caller) {
    const auto address=reinterpret_cast<std::uintptr_t>(caller);
    return address>=panelGameImageBegin.load() && address<panelGameImageEnd.load();
}
bool panelKeyboardMask(int key) {
    const auto bit=panelKeyboardBit(key);
    if(!bit)return false;
    const bool active=cursorOwned.load();
    const bool tail=(panelKeyboardTail.load()&bit)!=0;
    if(!active && !tail)return false;
    if(!runtimeEnabled.load() || !gameFocused()) {panelKeyboardTail.fetch_and(~bit);return false;}
    // 只在面板接管或等待尾沿时读取物理状态，不修改系统按键状态，也不模拟松键。
    const bool held=(originalPanelAsyncKeyState(key)&0x8000)!=0;
    if(held)panelKeyboardTail.fetch_or(bit);
    else if(!(panelKeyboardMessageHeld.load()&bit))panelKeyboardTail.fetch_and(~bit);
    return active || tail;
}
SHORT WINAPI panelAsyncKeyState(int key) {
    const auto value=originalPanelAsyncKeyState(key);
    return panelKeyboardGameCaller(__builtin_return_address(0))&&panelKeyboardMask(key)?0:value;
}
SHORT WINAPI panelKeyState(int key) {
    const auto value=originalPanelKeyState(key);
    return panelKeyboardGameCaller(__builtin_return_address(0))&&panelKeyboardMask(key)?0:value;
}
BOOL WINAPI panelKeyboardState(PBYTE keys) {
    const auto result=originalPanelKeyboardState(keys);
    if(result && keys && panelKeyboardGameCaller(__builtin_return_address(0))) {
        for(int key:{VK_BACK,VK_SPACE})if(panelKeyboardMask(key))keys[key]=0;
    }
    return result;
}
bool installPanelKeyboardState(HMODULE user32) {
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0)return false;
    const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS*>(image+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE || !nt->OptionalHeader.SizeOfImage)return false;
    panelGameImageBegin=image;panelGameImageEnd=image+nt->OptionalHeader.SizeOfImage;
    struct Hook {const char* name;void* callback;void** original;void* address{};};
    std::array<Hook,3> hooks={{
        {"GetAsyncKeyState",reinterpret_cast<void*>(&panelAsyncKeyState),reinterpret_cast<void**>(&originalPanelAsyncKeyState)},
        {"GetKeyState",reinterpret_cast<void*>(&panelKeyState),reinterpret_cast<void**>(&originalPanelKeyState)},
        {"GetKeyboardState",reinterpret_cast<void*>(&panelKeyboardState),reinterpret_cast<void**>(&originalPanelKeyboardState)}
    }};
    size_t created=0,enabled=0;
    for(auto& hook:hooks) {
        hook.address=reinterpret_cast<void*>(GetProcAddress(user32,hook.name));
        if(!hook.address || MH_CreateHook(hook.address,hook.callback,hook.original)!=MH_OK)break;
        ++created;
    }
    if(created==hooks.size())for(auto& hook:hooks) {
        if(MH_EnableHook(hook.address)!=MH_OK)break;
        ++enabled;
    }
    if(enabled==hooks.size())return true;
    // 仅撤回本函数刚创建的钩子；不移除其他模块已有的同名钩子。
    for(size_t i=0;i<enabled;++i)MH_DisableHook(hooks[i].address);
    for(size_t i=0;i<created;++i)MH_RemoveHook(hooks[i].address);
    return false;
}

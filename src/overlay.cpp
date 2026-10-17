// DX11 overlay: the render thread only handles UI and a command queue. All game
// objects and weapon mutations stay in the UE4SS game-thread controller.
#include "common.hpp"
#include <d3d11.h>
#include <Xinput.h>
#include <atomic>
#include <chrono>
#include <vector>
#include <array>
#include <algorithm>
#include <iterator>
#include <cmath>
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "MinHook.h"
#include "panel_pointer.hpp"
#include "keyboard_shortcuts.hpp"
#include "ue4ss_lua_api.hpp"
#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"xinput.lib")
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
namespace {
using Present=HRESULT(__stdcall*)(IDXGISwapChain*,UINT,UINT);
using Resize=HRESULT(__stdcall*)(IDXGISwapChain*,UINT,UINT,UINT,DXGI_FORMAT,UINT);
Present originalPresent{};Resize originalResize{};
using SetCursorFn=HCURSOR(WINAPI*)(HCURSOR);
using SetCursorPosFn=BOOL(WINAPI*)(int,int);
using ClipCursorFn=BOOL(WINAPI*)(const RECT*);
SetCursorFn originalSetCursor{};SetCursorPosFn originalSetCursorPos{};ClipCursorFn originalClipCursor{};
ID3D11Device* device{};ID3D11DeviceContext* context{};ID3D11RenderTargetView* target{};
IDXGISwapChain* mainSwap{};HWND gameWindow{};WNDPROC originalWindowProc{};
std::recursive_mutex renderMutex;
std::mutex queueMutex;
std::mutex publishedMutex;
Json publishedState=Json::object();
std::atomic<uint64_t> publishedGeneration{0};
std::vector<Json> actions;
ImGuiContext* imguiContext{};
Json state=Json::object();
ULONGLONG lastControllerUpdate{};
int lastSequence=-1;
bool panel=false,initialized=false;
std::atomic<bool> cursorOwned{false};
std::mutex cursorMutex;
RECT restoreClip{};
bool restoreClipValid=false;
dualsp::PanelPointer pointer;
std::atomic<bool> startup{false};
std::atomic<bool> runtimeEnabled{false}; // 默认不占用输入，只有游戏线程确认单人后才开放。
std::atomic<bool> reseedPad{true}; // 返回单人时以当前按住状态为基线，不重放多人期间的按键。
// 普通管理草稿与保存回执；调试换装另设请求，不随保存设置触发。
int lastRequest=-1,lastClose=-1,saveRequest=0,pendingSave=0,captureSlot=-1,suppressedCaptureKey=0;
constexpr int managementProtocol=7; // 安全菜单总开关与提示状态须搭配本版控制器。
bool draftModEnabled=true,draftFixedMountEnabled=true;
int draftAmmoPercentTwo=50,draftAmmoPercentThree=33;
std::array<int,4> draftKeys{49,50,51,52};
std::array<dualsp::Shortcut,3> shortcuts{{{VK_F6,0},{VK_F6,3},{VK_F7,3}}};
std::array<dualsp::Shortcut,3> draftShortcuts=shortcuts;
std::string draftLanguage="auto",notice,managementError; // 新草稿默认跟随游戏文本语言。
std::array<bool,3> debugSelected{}; // 按原生配置顺序选择，连续组成 SP1 至 SP3。
int debugRequest=0,pendingDebug=0,debugSession=-1,debugRevision=-1; // 请求回执与草稿身份。
std::string debugNotice; // 换装成功、拒绝或失败的独立反馈。
int hotkeyRequest=0,hotkeyResponse=0;
int masterResponse=0;
std::array<bool,256> consumedShortcutKeys{};
std::string hotkeyToast;
ULONGLONG hotkeyToastUntil{};
bool toastIsError=false;
Json reportedToastErrors=Json::object(); // 同一持续错误只提示一次，恢复后再次发生才重新提示。
void showToast(std::string message,bool error=false) {
    const auto newline=message.find_first_of("\r\n");
    if(newline!=std::string::npos)message.resize(newline);
    const auto source=message.find(".lua:");
    if(source!=std::string::npos) {
        size_t end=source+5;
        while(end<message.size()&&message[end]>='0'&&message[end]<='9')++end;
        if(end<message.size()&&message[end]==':') {
            ++end;while(end<message.size()&&message[end]==' ')++end;
            message.erase(0,end); // 悬浮窗省略源码路径和行号，完整信息仍保留在日志和管理页。
        }
    }
    // Lua保留完整堆栈供日志分析，悬浮窗只显示简短原因；截断不切开UTF-8字符。
    if(message.size()>240) {
        size_t end=240;
        while(end && (static_cast<unsigned char>(message[end])&0xc0)==0x80)--end;
        message.resize(end);message+="…";
    }
    if(message.empty())return;
    hotkeyToast=std::move(message);toastIsError=error;
    hotkeyToastUntil=GetTickCount64()+(error?6500:4500);
}
int managementTab=0,requestedTab=-1; // Ctrl+Tab 在四个管理分页间循环。
bool focusManagement=false; // 每次打开把键盘焦点放到语言选择，不依赖鼠标点击。
// 日志只在页面打开或手动刷新时读取。
bool logsLoaded=false;
std::string modLogText,nativeLogText,logError;
// 输入函数声明供窗口消息使用；实现与管理页集中在后文。
const char* tr(const char* zh,const char* en);
void loadManagementDraft();
void loadDebugDraft();
void captureKeyboardKey(int key,int mods);
int shortcutModifiers() {
    return ((GetKeyState(VK_CONTROL)&0x8000)?1:0)|((GetKeyState(VK_MENU)&0x8000)?2:0)|((GetKeyState(VK_SHIFT)&0x8000)?4:0);
}
bool shortcutModifierKey(int key) {
    return key==VK_CONTROL||key==VK_LCONTROL||key==VK_RCONTROL||key==VK_MENU||key==VK_LMENU||key==VK_RMENU||key==VK_SHIFT||key==VK_LSHIFT||key==VK_RSHIFT;
}
bool directInputEnabled();
void enqueue(Json action) {std::lock_guard<std::mutex> guard(queueMutex);actions.push_back(std::move(action));}
bool gameFocused() {return gameWindow && GetForegroundWindow()==GetAncestor(gameWindow,GA_ROOT);}
HCURSOR WINAPI panelSetCursor(HCURSOR requested) {
    return originalSetCursor(cursorOwned.load()&&gameFocused()?nullptr:requested);
}
BOOL WINAPI panelSetCursorPos(int x,int y) {
    // The engine's per-frame warp must not undo the user's panel movement.
    // This detour applies only inside this process and while our panel owns input.
    if(cursorOwned.load()&&gameFocused())return TRUE;
    return originalSetCursorPos(x,y);
}
BOOL WINAPI panelClipCursor(const RECT* requested) {
    if(cursorOwned.load()&&gameFocused()) {
        {std::lock_guard<std::mutex> guard(cursorMutex);
            restoreClipValid=requested!=nullptr;if(requested)restoreClip=*requested;}
        return originalClipCursor(nullptr);
    }
    return originalClipCursor(requested);
}
void togglePanel(bool open) {
    if(open&&!runtimeEnabled.load())return;
    if(panel==open)return;panel=open;
    if(open) {
        RECT client{};GetClientRect(gameWindow,&client);
        pointer.center(float(client.right-client.left),float(client.bottom-client.top));
        {std::lock_guard<std::mutex> guard(cursorMutex);restoreClipValid=GetClipCursor(&restoreClip)!=FALSE;}
        cursorOwned.store(true);
        if(originalClipCursor)originalClipCursor(nullptr);
        if(originalSetCursor)originalSetCursor(nullptr);
    }else {
        cursorOwned.store(false);
        RECT clip{};bool clipped=false;
        {std::lock_guard<std::mutex> guard(cursorMutex);clip=restoreClip;clipped=restoreClipValid;}
        if(originalClipCursor)originalClipCursor(gameFocused()&&clipped?&clip:nullptr);
        if(imguiContext){ImGui::GetIO().ClearInputKeys();ImGui::GetIO().ClearInputMouse();}
    }
    if(!open)captureSlot=-1;
    enqueue({{"type",open?"open":"close"}});
    if(open) {
        notice.clear();captureSlot=-1;managementError.clear();focusManagement=true;
        if(state.value("management_protocol",0)==managementProtocol) {
            try {loadManagementDraft();}
            catch(const std::exception& error){managementError=error.what();nativeLog(std::string("MANAGEMENT_DATA_ERROR ")+error.what());}
        }
    }
}
LRESULT CALLBACK windowProc(HWND hwnd,UINT msg,WPARAM w,LPARAM l) {
    std::lock_guard<std::recursive_mutex> guard(renderMutex);
    if(!runtimeEnabled.load())return CallWindowProcW(originalWindowProc,hwnd,msg,w,l);
    if(imguiContext)ImGui::SetCurrentContext(imguiContext);
    // 改键的按下、连发和释放一起消费，避免改完键后长按顺带操作管理页。
    if(msg==WM_KILLFOCUS){suppressedCaptureKey=0;captureSlot=-1;consumedShortcutKeys={};}
    const bool keyDown=msg==WM_KEYDOWN || msg==WM_SYSKEYDOWN;
    const bool keyUp=msg==WM_KEYUP || msg==WM_SYSKEYUP;
    if((keyDown||keyUp)&&w<consumedShortcutKeys.size()&&consumedShortcutKeys[w]) {
        if(keyUp)consumedShortcutKeys[w]=false;
        return 0; // 配置改变后仍消费旧按键的连发和释放。
    }
    if(keyDown&&!(l&(1LL<<30))&&gameFocused()) {
        if(panel&&captureSlot>=0) {
            if(!shortcutModifierKey(static_cast<int>(w))) {
                if(w<consumedShortcutKeys.size())consumedShortcutKeys[w]=true;
                captureKeyboardKey(static_cast<int>(w),shortcutModifiers());
            }
            return 0;
        }
        const int mods=shortcutModifiers();
        for(size_t index=0;index<shortcuts.size();++index)if(dualsp::shortcutMatches(shortcuts[index],static_cast<int>(w),mods)) {
            if(w<consumedShortcutKeys.size())consumedShortcutKeys[w]=true;
            if(index==0)togglePanel(!panel);
            else if(state.contains("debug_hotkey")&&state["debug_hotkey"].value("protocol",0)==1&&state.contains("debug_loadout")) {
                const auto& debug=state["debug_loadout"];
                enqueue({{"type","debug_hotkey"},{"hotkey_protocol",1},{"request",++hotkeyRequest},
                    {"session",debug.value("session",-1)},{"revision",debug.value("revision",-1)},
                    {"command",index==1?"cycle":"mount"}});
            }
            return 0;
        }
    }
    if(panel&&captureSlot>=0&&(keyDown||keyUp))return 0;
    if((msg==WM_KEYDOWN||msg==WM_KEYUP)&&suppressedCaptureKey==static_cast<int>(w)) {
        if(msg==WM_KEYUP)suppressedCaptureKey=0;
        return 0;
    }
    if(msg==WM_KEYDOWN && !(l&(1LL<<30))) {
        // 捕获时不让同一个按键顺带激活按钮、关闭页面或触发游戏操作。
        // Esc 先归还给下拉菜单；没有弹出层时才关闭整个管理面板。
        if(w==VK_ESCAPE&&panel&&!ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel)){togglePanel(false);return 0;}
        if(panel&&w==VK_TAB&&(GetKeyState(VK_CONTROL)&0x8000)) {
            requestedTab=((requestedTab>=0?requestedTab:managementTab)+((GetKeyState(VK_SHIFT)&0x8000)?3:1))%4;
            return 0;
        }
        if(directInputEnabled()&&shortcutModifiers()==0) {
            const auto keys=state.at("keyboard_keys"); // 使用已保存值，管理草稿不影响飞行。
            for(int slot=0;slot<4;++slot)if(keys.at(slot).get<int>()==static_cast<int>(w)) {
                enqueue({{"type","select_weapon"},{"target",slot}});break;
            }
        }
        // 直接选择键继续交给游戏原生映射；冲突由用户自行处理。
    }
    // A HUD is a passive overlay. Feeding its mouse messages into ImGui would
    // let the backend capture the mouse and change the native menu's cursor.
    if(panel&&gameFocused()&&msg==WM_SETCURSOR) {
        if(originalSetCursor)originalSetCursor(nullptr);
        return TRUE;
    }
    if(panel&&msg==WM_MOUSEMOVE) {
        // Legacy-only devices (for example remote desktop) also work. Native
        // warps are blocked above; these positions therefore represent the user.
        pointer.absolute(float(static_cast<SHORT>(LOWORD(l))),float(static_cast<SHORT>(HIWORD(l))));
        ImGui::GetIO().AddMousePosEvent(pointer.x,pointer.y);
        return 0;
    }
    if(initialized&&panel&&msg!=WM_NCMOUSEMOVE)ImGui_ImplWin32_WndProcHandler(hwnd,msg,w,l);
    // AC7 registers raw mouse input and can suppress legacy button messages.
    // Feed the real raw button events into the overlay without synthesizing
    // input or changing the game's device registration.
    if(initialized&&panel&&gameFocused()&&msg==WM_INPUT) {
        RAWINPUT raw{};UINT size=sizeof(raw);
        if(GetRawInputData(reinterpret_cast<HRAWINPUT>(l),RID_INPUT,&raw,&size,sizeof(RAWINPUTHEADER))!=static_cast<UINT>(-1)&&raw.header.dwType==RIM_TYPEMOUSE) {
            const USHORT flags=raw.data.mouse.usButtonFlags;
            if(raw.data.mouse.usFlags&MOUSE_MOVE_ABSOLUTE) {
                const bool desktop=(raw.data.mouse.usFlags&MOUSE_VIRTUAL_DESKTOP)!=0;
                POINT position{
                    (desktop?GetSystemMetrics(SM_XVIRTUALSCREEN):0)+MulDiv(raw.data.mouse.lLastX,GetSystemMetrics(desktop?SM_CXVIRTUALSCREEN:SM_CXSCREEN)-1,65535),
                    (desktop?GetSystemMetrics(SM_YVIRTUALSCREEN):0)+MulDiv(raw.data.mouse.lLastY,GetSystemMetrics(desktop?SM_CYVIRTUALSCREEN:SM_CYSCREEN)-1,65535)};
                ScreenToClient(gameWindow,&position);pointer.absolute(float(position.x),float(position.y));
            }else pointer.relative(float(raw.data.mouse.lLastX),float(raw.data.mouse.lLastY));
            ImGui::GetIO().AddMousePosEvent(pointer.x,pointer.y);
            const USHORT down[]={RI_MOUSE_LEFT_BUTTON_DOWN,RI_MOUSE_RIGHT_BUTTON_DOWN,RI_MOUSE_MIDDLE_BUTTON_DOWN,RI_MOUSE_BUTTON_4_DOWN,RI_MOUSE_BUTTON_5_DOWN};
            const USHORT up[]={RI_MOUSE_LEFT_BUTTON_UP,RI_MOUSE_RIGHT_BUTTON_UP,RI_MOUSE_MIDDLE_BUTTON_UP,RI_MOUSE_BUTTON_4_UP,RI_MOUSE_BUTTON_5_UP};
            for(int k=0;k<5;++k){if(flags&down[k])ImGui::GetIO().AddMouseButtonEvent(k,true);if(flags&up[k])ImGui::GetIO().AddMouseButtonEvent(k,false);}
            if(flags&RI_MOUSE_WHEEL)ImGui::GetIO().AddMouseWheelEvent(0,static_cast<SHORT>(raw.data.mouse.usButtonData)/120.f);
        }
    }
    if(panel&&msg==WM_INPUT)return DefWindowProcW(hwnd,msg,w,l);
    if(panel && ((msg>=WM_KEYFIRST && msg<=WM_KEYLAST)||(msg>=WM_MOUSEFIRST && msg<=WM_MOUSELAST)||msg==WM_INPUT))return 0;
    return CallWindowProcW(originalWindowProc,hwnd,msg,w,l);
}
void makeTarget(IDXGISwapChain* swap) {
    ID3D11Texture2D* buffer{};
    if(SUCCEEDED(swap->GetBuffer(0,IID_PPV_ARGS(&buffer)))) {
        device->CreateRenderTargetView(buffer,nullptr,&target);buffer->Release();
    }
}
bool init(IDXGISwapChain* swap) {
    DXGI_SWAP_CHAIN_DESC desc{};
    if(FAILED(swap->GetDesc(&desc)) || !desc.OutputWindow)return false;
    if(FAILED(swap->GetDevice(IID_PPV_ARGS(&device))))return false;
    gameWindow=desc.OutputWindow;mainSwap=swap;device->GetImmediateContext(&context);makeTarget(swap);
    if(!target)return false;
    IMGUI_CHECKVERSION();imguiContext=ImGui::CreateContext();
    auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;
    io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard|ImGuiConfigFlags_NavEnableGamepad;
    // AC7 owns OS cursor visibility/shape and its centering behavior. ImGui's
    // default SetCursor(Arrow) competes with the game's hide/recenter logic.
    // Draw only our software cursor while the configuration panel is open.
    io.ConfigFlags|=ImGuiConfigFlags_NoMouseCursorChange;
    io.ConfigNavMoveSetMousePos=false;
    io.ConfigNavEscapeClearFocusItem=false; // 关闭下拉框后保留可见的键盘焦点。
    const auto font=std::filesystem::path(L"C:/Windows/Fonts/msyh.ttc");
    if(std::filesystem::exists(font))io.Fonts->AddFontFromFileTTF(font.string().c_str(),26.f,nullptr,io.Fonts->GetGlyphRangesChineseFull());
    else io.Fonts->AddFontDefault();
    ImGui::StyleColorsDark();auto& s=ImGui::GetStyle();s.WindowRounding=2;s.FrameRounding=2;s.WindowPadding={24,22};s.ItemSpacing={14,12};
    s.Colors[ImGuiCol_WindowBg]={.025f,.065f,.085f,.98f};
    s.Colors[ImGuiCol_FrameBg]={.075f,.14f,.17f,1};s.Colors[ImGuiCol_Button]={.09f,.21f,.25f,1};
    s.Colors[ImGuiCol_ButtonHovered]={.15f,.39f,.43f,1};s.Colors[ImGuiCol_ButtonActive]={.16f,.48f,.50f,1};
    s.Colors[ImGuiCol_Header]={.11f,.30f,.34f,1};s.Colors[ImGuiCol_Text]={.82f,.94f,.94f,1};
    s.Colors[ImGuiCol_Border]={.20f,.43f,.48f,.7f};s.Colors[ImGuiCol_PlotHistogram]={.22f,.80f,.64f,1};
    ImGui_ImplWin32_Init(gameWindow);ImGui_ImplDX11_Init(device,context);
    originalWindowProc=reinterpret_cast<WNDPROC>(SetWindowLongPtrW(gameWindow,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(&windowProc)));
    initialized=true;nativeLog("DX11 overlay ready");return true;
}
// 管理界面的文字按用户选择显示；日志正文保留记录时的原文。
const char* tr(const char* zh,const char* en) {
    // 跟随游戏只解析显示语言，不覆盖用户已保存的手动选择。
    const auto language=draftLanguage=="auto"?state.value("game_language",std::string("en")):draftLanguage;
    return language=="zh"?zh:en;
}

// 单键绑定支持常用键盘键；当前管理入口和 Esc 不能绑定为武器选择。
bool supportedKey(int key) { return dualsp::supportedShortcutKey(key); }

// 返回稳定的键位名称，不依赖 Windows 显示语言。
std::string keyLabel(int key) {
    if((key>='0'&&key<='9')||(key>='A'&&key<='Z'))return std::string(1,static_cast<char>(key));
    if(key>=VK_F1&&key<=VK_F24)return "F"+std::to_string(key-VK_F1+1);
    if(key>=VK_NUMPAD0&&key<=VK_NUMPAD9)return std::string(tr("小键盘 ","Numpad "))+std::to_string(key-VK_NUMPAD0);
    switch(key) {
        case VK_BACK:return "Backspace";case VK_TAB:return "Tab";case VK_RETURN:return "Enter";
        case VK_SPACE:return tr("空格","Space");case VK_PRIOR:return "Page Up";case VK_NEXT:return "Page Down";
        case VK_END:return "End";case VK_HOME:return "Home";case VK_LEFT:return tr("左方向键","Left Arrow");
        case VK_UP:return tr("上方向键","Up Arrow");case VK_RIGHT:return tr("右方向键","Right Arrow");
        case VK_DOWN:return tr("下方向键","Down Arrow");case VK_INSERT:return "Insert";case VK_DELETE:return "Delete";
        case VK_MULTIPLY:return "Numpad *";case VK_ADD:return "Numpad +";case VK_SEPARATOR:return "Numpad Separator";
        case VK_SUBTRACT:return "Numpad -";case VK_DECIMAL:return "Numpad .";case VK_DIVIDE:return "Numpad /";
        case VK_OEM_1:return ";";case VK_OEM_PLUS:return "=";case VK_OEM_COMMA:return ",";case VK_OEM_MINUS:return "-";
        case VK_OEM_PERIOD:return ".";case VK_OEM_2:return "/";case VK_OEM_3:return "`";
        case VK_OEM_4:return "[";case VK_OEM_5:return "\\";case VK_OEM_6:return "]";case VK_OEM_7:return "'";
    }
    return tr("不支持的键","Unsupported key");
}

std::string shortcutLabel(dualsp::Shortcut value) {
    return std::string(value.mods&1?"Ctrl+":"")+(value.mods&2?"Alt+":"")+(value.mods&4?"Shift+":"")+keyLabel(value.key);
}
std::array<dualsp::Shortcut,3> readShortcuts(const Json& data) {
    std::array<dualsp::Shortcut,3> values;
    const char* names[]={"management","cycle","mount"};
    for(int i=0;i<3;++i) {
        const auto& item=data.at(names[i]);
        if(!item.at("key").is_number_integer()||!item.at("mods").is_number_integer())throw std::runtime_error("快捷键格式无效");
        values[i]={item.at("key").get<int>(),item.at("mods").get<int>()};
        if(!dualsp::validShortcut(values[i]))throw std::runtime_error("快捷键不可用");
    }
    return values;
}
Json shortcutData(const std::array<dualsp::Shortcut,3>& values) {
    Json data=Json::object();const char* names[]={"management","cycle","mount"};
    for(int i=0;i<3;++i)data[names[i]]={{"key",values[i].key},{"mods",values[i].mods}};
    return data;
}
bool bindingsValid() { return dualsp::validShortcuts(draftShortcuts,draftKeys); }

// 按键只产生请求，是否允许切换由游戏线程核对当前任务状态。
bool directInputEnabled() {
    return runtimeEnabled.load()&&!panel&&managementError.empty()&&gameFocused()&&state.value("management_protocol",0)==managementProtocol
        &&state.value("direct_selection_available",false);
}

// 捕获单次按键；错误显式提示，保持等待，不把无效输入换成其他键。
void captureKeyboardKey(int key,int mods) {
    if(key==VK_ESCAPE){captureSlot=-1;notice.clear();return;}
    auto nextKeys=draftKeys;auto nextShortcuts=draftShortcuts;
    if(captureSlot<4) {
        if(mods!=0){notice=tr("武器直接选择只支持单键。","Direct weapon selection accepts single keys.");return;}
        nextKeys[captureSlot]=key;
    } else nextShortcuts[captureSlot-4]={key,mods};
    if(!dualsp::validShortcuts(nextShortcuts,nextKeys)) {
        notice=tr("键位不可用或重复；请换一个按键。Esc取消。","Unsupported or duplicate binding. Choose another key; Esc cancels.");return;
    }
    draftKeys=nextKeys;draftShortcuts=nextShortcuts;captureSlot=-1;notice.clear();
}

// 管理页打开时加载保存值；未保存的草稿不改变游戏输入。
void loadManagementDraft() {
    draftLanguage=state.at("language").get<std::string>();
    draftModEnabled=state.at("mod_enabled").get<bool>();
    draftFixedMountEnabled=state.at("fixed_mount_enabled").get<bool>();
    if(!state.at("ammo_percent_two").is_number_integer()||!state.at("ammo_percent_three").is_number_integer())
        throw std::runtime_error("SP 弹药百分比必须为整数");
    draftAmmoPercentTwo=state.at("ammo_percent_two").get<int>();
    draftAmmoPercentThree=state.at("ammo_percent_three").get<int>();
    if(draftAmmoPercentTwo<50||draftAmmoPercentTwo>100||draftAmmoPercentThree<33||draftAmmoPercentThree>100)
        throw std::runtime_error("SP 弹药百分比超出范围");
    state.at("runtime_mod_enabled").get<bool>();
    state.at("runtime_fixed_mount_enabled").get<bool>();
    const auto keys=state.value("keyboard_keys",Json::array()); // 控制器负责验证配置，界面也拒绝不匹配的协议。
    if(keys.size()!=4)throw std::runtime_error("Management keyboard binding data is invalid");
    for(int slot=0;slot<4;++slot)draftKeys[slot]=keys.at(slot).get<int>();
    draftShortcuts=readShortcuts(state.at("shortcuts"));
    if(!bindingsValid())throw std::runtime_error("Management keyboard bindings are invalid");
    saveRequest=std::max(saveRequest,state.value("settings_response",0));
    captureSlot=-1;logsLoaded=false;
    loadDebugDraft();
}

// 打开管理页或任务上下文改变时读取真实当前搭载；编辑勾选不修改游戏。
void loadDebugDraft() {
    const auto& debug=state.at("debug_loadout"); // 新扩展缺失时明确报协议错误。
    if(debug.at("protocol").get<int>()!=1)throw std::runtime_error("调试换装协议不匹配");
    debugSelected={false,false,false};debugNotice.clear();
    debugSession=debug.at("session").get<int>();debugRevision=debug.at("revision").get<int>();
    debugRequest=std::max(debugRequest,debug.at("response").get<int>());
    if(debug.contains("profile"))for(const char* field:{"a","b","c"}) {
        const int index=debug.at("profile").at(field).get<int>(); // 映射原生配置到三项复选框。
        if(index < -1 || index > 2)throw std::runtime_error("调试方案编号无效");
        if(index>=0)debugSelected[index]=true;
    }
}

// 调试仅发送完整方案与任务版本，游戏线程重新校验身份、购买与活动武器。
void drawDebugLoadout(bool focus) {
    const auto& debug=state.at("debug_loadout"); // 发布状态是只读快照。
    ImGui::Text(tr("当前飞机：%s","Current aircraft: %s"),state.value("plane_name",std::string()).c_str());
    ImGui::TextWrapped("%s",tr("调试：任务内重配 SP。按勾选顺序（从上到下）分配 SP1 / SP2 / SP3。","Debug: rebuild SP loadout in this mission. Checked weapons become SP1 / SP2 / SP3 from top to bottom."));
    ImGui::TextWrapped(tr("快捷测试：%s 轮切SP组合；%s 即时开关模型。按键页可自定义。","Quick test: %s cycles SP combinations; %s toggles models. Customize in Bindings."),shortcutLabel(shortcuts[1]).c_str(),shortcutLabel(shortcuts[2]).c_str());
    ImGui::TextWrapped("%s",tr("应用后 SP 按已保存的管理比例配弹并补满，切回 MSL；普通导弹、机炮、干扰弹保留。个人方案和已有检查点不改写。","Apply refills SP ammunition using saved management percentages, then selects MSL. MSL, gun and flares are preserved. Saved loadouts and existing checkpoints are unchanged."));
    const bool available=debug.value("available",false)&&pendingDebug==0; // 等待回执时不能重复提交。
    const auto options=debug.value("options",Json::array()); // 没有任务时不展示猜测兵装。
    const int count=static_cast<int>(std::count(debugSelected.begin(),debugSelected.end(),true)); // 实际选择数量。
    bool valid=count>0&&options.is_array()&&options.size()==3; // UAV/ECM 多挂限制同步显示。
    if(options.is_array()&&options.size()==3) {
        for(int index=0;index<3;++index) {
            const auto& option=options.at(index); // 原生配置项包含购买和多挂许可。
            const bool owned=option.at("owned").get<bool>();
            const bool multiple=option.at("available").get<bool>();
            const auto label=option.at("code").get<std::string>()+" / "+option.at("description").get<std::string>()+"###DebugSP"+std::to_string(index);
            ImGui::BeginDisabled(!available||!owned);
            if(focus&&available&&owned){ImGui::SetKeyboardFocusHere();focus=false;}
            ImGui::Checkbox(label.c_str(),&debugSelected[index]);
            ImGui::EndDisabled();
            if(!owned){ImGui::SameLine();ImGui::TextDisabled("%s",tr("未购买","Not purchased"));}
            else if(!multiple){ImGui::SameLine();ImGui::TextDisabled("%s",tr("仅限单挂","Single SP only"));}
        }
        // 同帧勾选后重新计算，不能沿用绘制前的数量判定或配弹预览。
        const int selected=static_cast<int>(std::count(debugSelected.begin(),debugSelected.end(),true));
        valid=selected>0;
        for(int index=0;index<3;++index)if(debugSelected[index])
            valid=valid&&options.at(index).at("owned").get<bool>()&&(selected==1||options.at(index).at("available").get<bool>());
        if(!valid)ImGui::TextWrapped("%s",tr("请选择 1 至 3 种已购买 SP；标注仅限单挂的兵装不能组合。","Choose 1–3 purchased SP weapons. Single-only weapons cannot be combined."));
    }
    if(!debug.value("available",false)&&debug.contains("reason"))
        ImGui::TextWrapped("%s",debug.at("reason").get<std::string>().c_str());
    ImGui::TextWrapped("%s",tr("若提示仍有活动武器，请关闭面板、等待已发射武器结束后重试。任务内换飞机尚未开放。","If active weapons are reported, close this panel and wait for them to finish before retrying. In-mission aircraft replacement is not available yet."));
    ImGui::BeginDisabled(!available||!valid);
    if(ImGui::Button(tr("应用 SP 方案并补满###ApplyDebug","Apply SP loadout and refill###ApplyDebug"),{300,36})) {
        std::array<int,3> configs{-1,-1,-1};int slot=0; // 只在点击时形成连续逻辑槽。
        for(int index=0;index<3;++index)if(debugSelected[index])configs[slot++]=index;
        pendingDebug=++debugRequest;
        enqueue({{"type","debug_loadout"},{"debug_protocol",1},{"request",pendingDebug},
            {"session",debugSession},{"revision",debugRevision},{"profile",{{"a",configs[0]},{"b",configs[1]},{"c",configs[2]}}}});
        debugNotice=tr("正在应用，等待游戏线程确认…","Applying; waiting for the game thread...");
    }
    ImGui::EndDisabled();
    if(!debugNotice.empty())ImGui::TextWrapped("%s",debugNotice.c_str());
    if(debug.contains("error"))ImGui::TextWrapped("%s",debug.at("error").get<std::string>().c_str());
}

// 最多读取每份日志最后 64 KiB；超出时丢掉截断首行，不在飞行中读日志。
std::string readLogTail(const char* filename) {
    const auto file=modRoot()/filename; // 路径限定为模组自己的两份日志。
    std::ifstream input(file,std::ios::binary);
    if(!input)throw std::runtime_error(std::string("Cannot read log: ")+filename);
    input.seekg(0,std::ios::end);
    const std::streamoff end=input.tellg(); // 负数表示读取定位失败，不能冒充空日志。
    if(end<0)throw std::runtime_error(std::string("Cannot locate log end: ")+filename);
    const std::streamoff start=std::max<std::streamoff>(0,end-65536);
    input.seekg(start);
    if(start>0){std::string partial;std::getline(input,partial);}
    std::string result((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());
    if(input.bad())throw std::runtime_error(std::string("Cannot read log contents: ")+filename);
    return result;
}

// 日志只有打开页面或用户点击刷新时读取，失败保持可见。
void refreshLogs() {
    logError.clear();modLogText.clear();nativeLogText.clear();logsLoaded=true;
    try {modLogText=readLogTail("mod.log");nativeLogText=readLogTail("native.log");}
    catch(const std::exception& error){logError=error.what();nativeLog(std::string("MANAGEMENT_LOG_ERROR ")+error.what());}
}

// 保存按钮只提交完整参数；收到游戏线程的请求回执后才显示保存成功。
void saveManagementSettings() {
    pendingSave=++saveRequest;
    enqueue({{"type","settings"},{"management_protocol",managementProtocol},{"request",pendingSave},{"language",draftLanguage},
        {"mod_enabled",draftModEnabled},{"fixed_mount_enabled",draftFixedMountEnabled},
        {"ammo_percent_two",draftAmmoPercentTwo},{"ammo_percent_three",draftAmmoPercentThree},{"keyboard_keys",draftKeys},{"shortcuts",shortcutData(draftShortcuts)}});
    notice=tr("正在保存…","Saving...");
}

// 统一分页入口，让 Ctrl+Tab 的选择和鼠标选择共用当前页状态。
bool beginManagementTab(const char* label,int index) {
    const bool selected=ImGui::BeginTabItem(label,nullptr,requestedTab==index?ImGuiTabItemFlags_SetSelected:0); // 稳定分页 ID。
    if(selected)managementTab=index;
    return selected;
}
// 普通管理和显式调试分开，键盘焦点始终可以到达所有操作。
void drawPanel() {
    const auto display=ImGui::GetIO().DisplaySize; // 面板随窗口尺寸缩放，固定居中。
    const float width=std::min(1180.f,display.x-40),height=std::min(920.f,display.y-35);
    ImGui::SetNextWindowPos({display.x/2,display.y/2},ImGuiCond_Always,{.5f,.5f});ImGui::SetNextWindowSize({width,height});
    ImGui::Begin(tr("AC7MultiSP 2.7.1 管理###DualSPManagement","AC7MultiSP 2.7.1 Management###DualSPManagement"),nullptr,
        ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoSavedSettings);
    if(state.value("management_protocol",0)!=managementProtocol||!managementError.empty()) {
        ImGui::TextWrapped("%s",tr("管理界面与控制器版本不匹配，请完整安装同一版本。","Management UI and controller versions do not match. Install a complete matching version."));
        if(!managementError.empty())ImGui::TextWrapped("%s",managementError.c_str());
        if(ImGui::Button(tr("关闭","Close")))togglePanel(false);
        ImGui::End();return;
    }
    // 暂停归属来自游戏线程回执；渲染线程不推断原生任务状态。
    if(state.contains("management_pause")) {
        const auto& pause=state.at("management_pause"); // 关闭失败会重新打开面板并显示具体错误。
        if(pause.contains("error"))ImGui::TextWrapped("%s",pause.at("error").get<std::string>().c_str());
        else if(pause.value("owned",false))ImGui::TextWrapped("%s",tr("管理界面已暂停任务；关闭本界面后继续飞行。","Management paused the mission; close this panel to resume."));
        else if(pause.value("existing",false))ImGui::TextWrapped("%s",tr("打开前已有原生暂停；关闭本界面后仍保留，请从游戏暂停菜单继续。","The mission was already paused; resume from the game's pause menu after closing."));
    }
    // 语言切换只影响界面草稿，点击保存后持久化；稳定 ID 保留当前页和焦点。
    int language=draftLanguage=="auto"?0:draftLanguage=="zh"?1:2;
    ImGui::SetNextItemWidth(210);
    if(focusManagement){ImGui::SetKeyboardFocusHere();focusManagement=false;}
    if(ImGui::Combo("语言选择/Select Language###Language",&language,"Game Language\0中文 / Chinese\0English\0"))
        draftLanguage=language==0?"auto":language==1?"zh":"en";
    ImGui::TextWrapped("%s",tr("键盘：Tab / Shift+Tab 移动，方向键选择，Enter / 空格操作；Ctrl+Tab 切页，Esc 返回或关闭。","Keyboard: Tab / Shift+Tab to move, arrows to select, Enter / Space to act; Ctrl+Tab changes pages, Esc goes back or closes."));
    ImGui::Separator();
    if(ImGui::BeginTabBar("ManagementTabs")) {
        if(beginManagementTab(tr("功能与参数###Features","Features & Parameters###Features"),0)) {
            if(requestedTab==0)ImGui::SetKeyboardFocusHere();
            ImGui::Checkbox(tr("启用 AC7MultiSP###ModEnabled","Enable AC7MultiSP###ModEnabled"),&draftModEnabled);
            ImGui::TextWrapped("%s",tr("在主菜单或单人任务选择页保存，总开关可直接生效。任务、补给及机库中保存后，返回上述安全菜单自动应用，无需重启游戏；管理入口始终保留。","Save in the main menu or single-player mission selection to apply immediately. Saves in missions, resupply or the hangar apply automatically upon returning to those safe menus; no game restart is required. Management remains available."));
            ImGui::SetNextItemWidth(300);
            ImGui::SliderInt(tr("双 SP 各自弹药量###AmmoTwo","Ammo per SP with two SPs###AmmoTwo"),&draftAmmoPercentTwo,50,100,"%d%%",ImGuiSliderFlags_AlwaysClamp);
            ImGui::SetNextItemWidth(300);
            ImGui::SliderInt(tr("三 SP 各自弹药量###AmmoThree","Ammo per SP with three SPs###AmmoThree"),&draftAmmoPercentThree,33,100,"%d%%",ImGuiSliderFlags_AlwaysClamp);
            ImGui::TextWrapped("%s",tr("默认双 SP 各 50%、三 SP 各 33%；单 SP 为 100%。先计入部件，再按百分比向上取整。保存后于下一次出击、补给确认或调试应用时配弹。","Defaults: 50% per SP with two SPs, 33% with three, 100% with one. Parts apply first, then the percentage is rounded up. Saved values apply at the next departure, resupply confirmation or debug loadout application."));
            ImGui::Checkbox(tr("多 SP 武器模型同时挂载（实验性）###FixedMount","Simultaneous multi-SP weapon models (experimental)###FixedMount"),&draftFixedMountEnabled);
            ImGui::TextWrapped("%s",tr("保存后在下次出击、重启任务、检查点重试、补给确认或调试换装时生效，无需重启游戏。适配原有三机与新增十机（详见安装说明）；关闭或挂载失败时回退到原生单 SP 模型。","Applies at the next sortie, mission restart, checkpoint retry, confirmed resupply or debug loadout; no game restart needed. Supports the original three aircraft and ten new aircraft; see installation notes. Disabled or failed mounting falls back to native single-SP models."));
            ImGui::TextWrapped("%s",tr("同时控制机库与补给的挂载预览：模型随当前 SP 候选组合变化。预览只改变外观；失败时恢复原生显示，不影响选择、弹药或补给。","Also controls hangar and resupply model previews, following the current SP selection. Previews change appearance only; failures restore native visuals without affecting selection, ammunition or resupply."));
            ImGui::TextWrapped("%s",tr("模组开启时，中央武器简称与右下兵装信息固定开启，跟随游戏 HUD 显隐；原游戏循环切换键继续可用。","When the mod is enabled, the central weapon abbreviation and lower-right weapon information stay enabled and follow game HUD visibility. Native weapon cycling remains available."));
            if(state.contains("mount_warning"))ImGui::TextWrapped("%s",state.at("mount_warning").get<std::string>().c_str());
            if(state.at("mod_enabled").get<bool>()!=state.at("runtime_mod_enabled").get<bool>())
                ImGui::TextColored({1,.8f,.35f,1},"%s",tr("总开关已保存，等待返回主菜单或单人任务选择页。","Master switch saved; return to the main menu or single-player mission selection."));
            if(state.contains("master_switch")&&state.at("master_switch").contains("error"))
                ImGui::TextWrapped("%s",state.at("master_switch").at("error").get<std::string>().c_str());
            if(state.at("fixed_mount_enabled").get<bool>()!=state.at("runtime_fixed_mount_enabled").get<bool>())
                ImGui::TextColored({1,.8f,.35f,1},"%s",tr("模型开关已保存，等待重新出击、任务/检查点重试、补给确认或调试换装。","Model switch saved. Awaiting a sortie, mission/checkpoint retry, confirmed resupply or debug loadout."));
            ImGui::EndTabItem();
        }
        if(beginManagementTab(tr("按键###Bindings","Bindings###Bindings"),1)) {
            ImGui::TextWrapped("%s",tr("选择键位按钮并按 Enter，然后按新键；也可鼠标点击。武器选择使用单键，Esc 取消；键位不能重复。","Focus a binding and press Enter, then press the new key; mouse clicks also work. Weapon selection uses single keys; Esc cancels. Bindings must be distinct."));
            ImGui::TextWrapped("%s",tr("与原生键位冲突时，请自行调整游戏或模组键位。","If a binding conflicts with the game, adjust either mapping yourself."));
            if(ImGui::BeginTable("DirectBindings",3,ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn(tr("选择","Selection"));ImGui::TableSetupColumn(tr("键盘","Keyboard"));
                ImGui::TableSetupColumn(tr("手柄（固定）","Controller (fixed)"));ImGui::TableHeadersRow();
                const char* weapons[]={"MSL","SP1","SP2","SP3"}; // 固定逻辑槽，实际简称由任务 HUD 显示。
                const char* pads[]={tr("十字键左","D-pad Left"),tr("十字键上","D-pad Up"),tr("十字键右","D-pad Right"),tr("十字键下","D-pad Down")};
                for(int slot=0;slot<4;++slot) {
                    ImGui::PushID(slot);ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::TextUnformatted(weapons[slot]);
                    ImGui::TableNextColumn();
                    const auto label=captureSlot==slot?std::string(tr("请按键…","Press a key...")):keyLabel(draftKeys[slot]);
                    if(requestedTab==1&&slot==0)ImGui::SetKeyboardFocusHere();
                    if(ImGui::Button(label.c_str(),{190,0})){captureSlot=slot;notice.clear();}
                    ImGui::TableNextColumn();ImGui::TextUnformatted(pads[slot]);ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::SeparatorText(tr("模组快捷键","Mod shortcuts"));
            const char* shortcutNames[]={tr("呼出管理界面","Management"),tr("轮切SP组合","Cycle SP combinations"),tr("开关多SP模型","Toggle multi-SP models")};
            ImGui::TextWrapped("%s",tr("点击后按单键，或按住Ctrl、Alt、Shift再按主键；Esc取消。保存后立即生效。","Click, then press one key, optionally holding Ctrl, Alt or Shift. Esc cancels; saving applies immediately."));
            for(int i=0;i<3;++i) {
                ImGui::PushID(100+i);ImGui::TextUnformatted(shortcutNames[i]);ImGui::SameLine(230);
                const auto label=captureSlot==i+4?std::string(tr("请按快捷键…","Press shortcut...")):shortcutLabel(draftShortcuts[i]);
                if(ImGui::Button(label.c_str(),{230,0})){captureSlot=i+4;notice.clear();}
                ImGui::PopID();
            }
            if(ImGui::Button(tr("恢复三个默认快捷键","Reset three shortcuts"))) {
                const std::array<dualsp::Shortcut,3> defaults{{{VK_F6,0},{VK_F6,3},{VK_F7,3}}};
                if(dualsp::validShortcuts(defaults,draftKeys)){draftShortcuts=defaults;captureSlot=-1;notice.clear();}
                else notice=tr("默认快捷键与武器选择键重复，请先调整。","Defaults conflict with a direct weapon key.");
            }
            if(captureSlot>=0&&ImGui::Button(tr("取消改键","Cancel capture")))captureSlot=-1;
            if(ImGui::Button(tr("恢复默认 1 / 2 / 3 / 4","Reset to 1 / 2 / 3 / 4"))){draftKeys={49,50,51,52};captureSlot=-1;notice.clear();}
            ImGui::TextWrapped("%s",tr("装填中的武器可以选择；未搭载的 SP 或耗尽的 SP 保持当前选择。按住不会重复切换。","Reloading weapons can be selected. An empty or depleted SP keeps the current selection. Holding a key does not repeat."));
            ImGui::EndTabItem();
        }
        if(beginManagementTab(tr("日志###Logs","Logs###Logs"),2)) {
            if(!logsLoaded)refreshLogs();
            if(requestedTab==2)ImGui::SetKeyboardFocusHere();
            if(ImGui::Button(tr("刷新日志","Refresh logs")))refreshLogs();
            ImGui::SameLine();ImGui::TextDisabled("%s",tr("每份最多显示末尾 64 KiB，点击刷新更新。","Last 64 KiB per log; refresh to update."));
            if(!logError.empty())ImGui::TextWrapped("%s",logError.c_str());
            if(ImGui::BeginTabBar("LogFiles")) {
                const char* files[]={"mod.log","native.log"}; // 日志文件固定，不开放任意路径访问。
                const std::string* text[]={&modLogText,&nativeLogText};
                for(int file=0;file<2;++file)if(ImGui::BeginTabItem(files[file])) {
                    ImGui::BeginChild(files[file],{0,260},ImGuiChildFlags_Borders,ImGuiWindowFlags_HorizontalScrollbar);
                    ImGui::TextUnformatted(text[file]->c_str());ImGui::EndChild();ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            ImGui::EndTabItem();
        }
        if(beginManagementTab(tr("调试###Debug","Debug###Debug"),3)) {
            drawDebugLoadout(requestedTab==3);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
        // ImGui 的分页选择在下一帧生效；等目标内容实际绘制并接收焦点后再消费。
        if(requestedTab==managementTab)requestedTab=-1;
    }
    ImGui::Separator();
    ImGui::BeginDisabled(captureSlot>=0||!bindingsValid()||pendingSave>0);
    if(ImGui::Button(tr("保存设置","Save settings"),{180,36}))saveManagementSettings();
    ImGui::EndDisabled();ImGui::SameLine();
    if(ImGui::Button(tr("关闭","Close"),{120,36}))togglePanel(false);
    ImGui::TextWrapped("%s",tr("语言和键位保存后生效；总开关在主菜单等安全入口应用。修改比例不会立即补充当前弹药。关闭会丢弃未保存修改。","Language and bindings apply after saving; the master switch applies at safe menus such as the main menu. Percentage changes do not immediately refill ammunition. Closing discards unsaved changes."));
    ImGui::TextWrapped(tr("%s 或手柄 Back + Y 打开/关闭管理界面；任务中打开会暂停。","%s or controller Back + Y opens/closes management. Opening during a mission pauses the game."),shortcutLabel(shortcuts[0]).c_str());
    // 明确报告失败，不引导用户到已移除的 F8 兵装替代流程。
    const char* errorFields[]={"error","menu_error","hud_error","central_error","settings_error"};
    for(const char* field:errorFields)if(state.contains(field)) {
        ImGui::TextColored({1,.55f,.35f,1},"%s",tr("功能或保存错误（详见日志）","Feature or save error (see logs)"));
        ImGui::TextWrapped("%s",state.at(field).get<std::string>().c_str());
    }
    if(state.contains("input_notice")) {
        const auto reason=state.at("input_notice").get<std::string>(); // 拒绝直接选择的原因显示在管理页。
        ImGui::TextWrapped("%s",reason=="empty_sp"?tr("此 SP 未配置，保持当前武器。","This SP is empty; selection unchanged."):
            tr("目标 SP 弹药耗尽，保持当前武器。","Target SP is depleted; selection unchanged."));
    }
    if(!notice.empty())ImGui::TextWrapped("%s",notice.c_str());
    ImGui::End();
}

// 消费游戏线程发布的完整状态；保留未保存草稿，按请求编号显示保存结果。
void refreshState() {
    static uint64_t consumedGeneration{}; // 一份状态只消费一次，不在每帧重复复制。
    if(publishedGeneration.load(std::memory_order_acquire)==consumedGeneration)return;
    try {
        const bool wasManagement=state.value("management_protocol",0)==managementProtocol;
        {
            std::lock_guard<std::mutex> guard(publishedMutex);
            state=publishedState;consumedGeneration=publishedGeneration.load(std::memory_order_relaxed);
        }
        // 键位在交给窗口消息前核对，格式错误不能跨 Win32 回调抛出异常。
        if(state.value("management_protocol",0)==managementProtocol) {
            const auto checkedShortcuts=readShortcuts(state.at("shortcuts"));
            const auto& keys=state.at("keyboard_keys");
            if(!keys.is_array()||keys.size()!=4)throw std::runtime_error("Management keyboard binding data is invalid");
            std::array<int,4> checkedKeys{};
            for(int slot=0;slot<4;++slot) {
                if(!keys.at(slot).is_number_integer())throw std::runtime_error("Management keyboard binding is not an integer");
                checkedKeys[slot]=keys.at(slot).get<int>();
                if(!supportedKey(checkedKeys[slot]))throw std::runtime_error("Management keyboard binding is unsupported");
            }
            for(size_t index=0;index<checkedKeys.size();++index)
                for(size_t prior=0;prior<index;++prior)if(checkedKeys[index]==checkedKeys[prior])
                    throw std::runtime_error("Management keyboard bindings must be distinct");
            state.at("direct_selection_available").get<bool>(); // 固定启用，但仍校验当前场景是否允许操作。
            if(!dualsp::validShortcuts(checkedShortcuts,checkedKeys))throw std::runtime_error("快捷键有重复绑定");
            shortcuts=checkedShortcuts;
        }
        const int sequence=state.value("sequence",0); // 状态序号用于记录控制器最近更新。
        if(sequence!=lastSequence){lastSequence=sequence;lastControllerUpdate=GetTickCount64();}
        const int request=state.value("request_open",0),close=state.value("request_close",0);
        if(request!=lastRequest){lastRequest=request;if(request>0)togglePanel(true);}
        if(close!=lastClose){lastClose=close;if(close>0)togglePanel(false);}
        if(!wasManagement&&panel&&state.value("management_protocol",0)==managementProtocol)loadManagementDraft();
        if(!panel)draftLanguage=state.value("language",std::string("auto"));
        if(pendingSave>0&&state.value("settings_response",0)==pendingSave) {
            notice=state.value("settings_ack",0)==pendingSave?tr("设置已保存。","Settings saved."):
                tr("保存失败，详见错误和日志。","Save failed; see the error and logs.");
            pendingSave=0;
        }
        const auto& debug=state.at("debug_loadout"); // 回执只来自游戏线程完整提交。
        if(debug.at("session").get<int>()!=debugSession||debug.at("revision").get<int>()!=debugRevision)loadDebugDraft();
        if(pendingDebug>0&&debug.at("response").get<int>()==pendingDebug) {
            debugNotice=debug.at("ack").get<int>()==pendingDebug?tr("SP 方案已应用并补满。关闭面板后继续测试。","SP loadout applied and refilled. Close the panel to continue testing."):
                tr("换装未完成，请查看错误说明。","Loadout was not applied. See the error details.");
            pendingDebug=0;
        }
        const auto& hotkey=state.at("debug_hotkey");
        hotkeyRequest=std::max(hotkeyRequest,hotkey.at("response").get<int>());
        if(hotkey.at("response").get<int>()!=hotkeyResponse) {
            hotkeyResponse=hotkey.at("response").get<int>();showToast(hotkey.value("notice",std::string()));
            draftFixedMountEnabled=state.at("fixed_mount_enabled").get<bool>();
        }
        if(state.contains("master_switch")) {
            const auto& master=state.at("master_switch");
            if(master.value("response",0)!=masterResponse) {
                masterResponse=master.value("response",0);showToast(master.value("notice",std::string()));
            }
        }
        Json errors=Json::object();
        for(const char* field:{"error","menu_error","hud_error","central_error","settings_error","mount_warning"})
            if(state.contains(field)&&state.at(field).is_string()&&!state.at(field).get_ref<const std::string&>().empty())errors[field]=state.at(field);
        for(const char* field:{"debug_loadout","management_pause","master_switch"})
            if(state.contains(field)&&state.at(field).contains("error")&&state.at(field).at("error").is_string()
                &&!state.at(field).at("error").get_ref<const std::string&>().empty())errors[field]=state.at(field).at("error");
        for(auto item=errors.begin();item!=errors.end();++item) {
            if(!reportedToastErrors.contains(item.key())||reportedToastErrors.at(item.key())!=item.value()) {
                showToast(std::string(tr("Mod提示：","Mod: "))+item.value().get<std::string>(),true);break;
            }
        }
        reportedToastErrors=std::move(errors);
    }catch(const std::exception& error) {
        if(managementError!=error.what())showToast(std::string(tr("管理状态错误：","Management state error: "))+error.what(),true);
        managementError=error.what();nativeLog(std::string("MANAGEMENT_DATA_ERROR ")+error.what());
    }
}

HRESULT __stdcall onPresent(IDXGISwapChain* swap,UINT interval,UINT flags) {
    {
        std::lock_guard<std::recursive_mutex> guard(renderMutex);
        if(!initialized)init(swap);
        if(initialized&&swap==mainSwap) {
            ImGui::SetCurrentContext(imguiContext);if(!target)makeTarget(swap);
            if(target&&runtimeEnabled.load()) {
                refreshState();
                static bool mouseWas[5]={};const int mouseKeys[]={VK_LBUTTON,VK_RBUTTON,VK_MBUTTON,VK_XBUTTON1,VK_XBUTTON2};
                const bool foreground=gameFocused();
                if(foreground&&panel)for(int k=0;k<5;++k) {
                    const SHORT keyState=GetAsyncKeyState(mouseKeys[k]);
                    const bool down=(keyState&0x8000)!=0;
                    // Very short software/remote clicks can complete between
                    // two frames and may produce no raw-input message.
                    if((keyState&1)&&!down&&!mouseWas[k]){ImGui::GetIO().AddMouseButtonEvent(k,true);ImGui::GetIO().AddMouseButtonEvent(k,false);}
                    if(down!=mouseWas[k]){ImGui::GetIO().AddMouseButtonEvent(k,down);mouseWas[k]=down;}
                }
                // 复用同一次手柄读取；启用直接选择时最多每 20 ms 采样一次。
                static bool chordWas=false,padWasConnected=false;
                static WORD previousPadButtons=0;
                static ULONGLONG lastPadPoll{};
                const auto padNow=GetTickCount64();
                const ULONGLONG pollInterval=directInputEnabled()?20:100; // 非飞行选择场景只轮询管理组合键。
                if(padNow-lastPadPoll>=pollInterval) {
                    lastPadPoll=padNow;
                    XINPUT_STATE pad{};
                    const bool connected=XInputGetState(0,&pad)==ERROR_SUCCESS;
                    const WORD buttons=connected?pad.Gamepad.wButtons:0;
                    const bool chord=foreground&&connected&&(buttons&(XINPUT_GAMEPAD_BACK|XINPUT_GAMEPAD_Y))==(XINPUT_GAMEPAD_BACK|XINPUT_GAMEPAD_Y);
                    if(reseedPad.exchange(false)){chordWas=chord;previousPadButtons=buttons;padWasConnected=connected;}
                    if(chord&&!chordWas)togglePanel(!panel);chordWas=chord;
                    const WORD pressed=buttons&~previousPadButtons;
                    if(connected&&padWasConnected&&directInputEnabled()) {
                        const WORD keys[]={XINPUT_GAMEPAD_DPAD_LEFT,XINPUT_GAMEPAD_DPAD_UP,XINPUT_GAMEPAD_DPAD_RIGHT,XINPUT_GAMEPAD_DPAD_DOWN};
                        int desired=-1,count=0; // 同时按下多个方向不猜测目标。
                        for(int slot=0;slot<4;++slot)if(pressed&keys[slot]){desired=slot;++count;}
                        if(count==1)enqueue({{"type","select_weapon"},{"target",desired}});
                        else if(count>1)nativeLog("DIRECT_SELECTION_AMBIGUOUS_PAD: 多个方向同时按下，未切换");
                    }
                    previousPadButtons=buttons;padWasConnected=connected;
                }
                // 平时仍跳过整个绘制；只有管理面板或短暂通知可见时提交ImGui帧。
                const bool toastVisible=foreground&&!hotkeyToast.empty()&&GetTickCount64()<hotkeyToastUntil;
                if(panel||toastVisible) {
                ImGui_ImplDX11_NewFrame();
                ImGui::GetIO().WantSetMousePos=false;
                ImGui_ImplWin32_NewFrame();
                if(panel&&foreground) {
                    auto& io=ImGui::GetIO();pointer.bounds(io.DisplaySize.x,io.DisplaySize.y);
                    // Win32's fallback reads the OS cursor, which AC7 normally
                    // centers. Our raw-input position is authoritative for this
                    // panel and is submitted after the backend's fallback.
                    io.AddMousePosEvent(pointer.x,pointer.y);
                }
                ImGui::GetIO().MouseDrawCursor=panel&&foreground;ImGui::NewFrame();
                try {
                    if(panel)drawPanel();
                    if(toastVisible) {
                        ImGui::SetNextWindowPos({ImGui::GetIO().DisplaySize.x*.5f,72},ImGuiCond_Always,{.5f,0});
                        ImGui::SetNextWindowBgAlpha(.8f);
                        ImGui::Begin("##DebugHotkeyNotice",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoNav|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoFocusOnAppearing);
                        ImGui::PushTextWrapPos(std::min(ImGui::GetFontSize()*36,ImGui::GetIO().DisplaySize.x-80));
                        if(toastIsError)ImGui::PushStyleColor(ImGuiCol_Text,{1,.65f,.4f,1});
                        ImGui::TextUnformatted(hotkeyToast.c_str());
                        if(toastIsError)ImGui::PopStyleColor();
                        ImGui::PopTextWrapPos();ImGui::End();
                    }
                }catch(const Json::exception& e){notice=e.what();showToast(std::string(tr("界面错误：","UI error: "))+e.what(),true);nativeLog(std::string("UI_ERROR ")+e.what());}
                if(panel&&ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight))togglePanel(false);
                ImGui::Render();
                ID3D11RenderTargetView* oldTarget{};ID3D11DepthStencilView* oldDepth{};context->OMGetRenderTargets(1,&oldTarget,&oldDepth);
                context->OMSetRenderTargets(1,&target,nullptr);ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());context->OMSetRenderTargets(1,&oldTarget,oldDepth);
                if(oldTarget)oldTarget->Release();if(oldDepth)oldDepth->Release();
                }
            }
        }
    }
    return originalPresent(swap,interval,flags);
}
HRESULT __stdcall onResize(IDXGISwapChain* swap,UINT count,UINT width,UINT height,DXGI_FORMAT format,UINT flags) {
    std::lock_guard<std::recursive_mutex> guard(renderMutex);
    if(initialized&&swap==mainSwap&&target){target->Release();target=nullptr;}
    return originalResize(swap,count,width,height,format,flags);
}
DWORD WINAPI startThread(void*) {
    // A hidden dummy swap chain provides the DXGI function addresses without
    // waiting for, scanning, or dereferencing unverified game renderer objects.
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"DualSPDX11Probe";RegisterClassW(&wc);
    HWND hidden=CreateWindowExW(0,wc.lpszClassName,L"",WS_OVERLAPPEDWINDOW,0,0,32,32,nullptr,nullptr,wc.hInstance,nullptr);
    DXGI_SWAP_CHAIN_DESC desc{};desc.BufferCount=1;desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.OutputWindow=hidden;desc.SampleDesc.Count=1;desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    IDXGISwapChain* swap{};ID3D11Device* probeDevice{};ID3D11DeviceContext* probeContext{};
    const HRESULT result=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,&swap,&probeDevice,nullptr,&probeContext);
    if(SUCCEEDED(result)) {
        auto vtable=*reinterpret_cast<void***>(swap);
        const auto initStatus=MH_Initialize();
        const auto user32=GetModuleHandleW(L"user32.dll");
        const auto setCursor=reinterpret_cast<void*>(GetProcAddress(user32,"SetCursor"));
        const auto setCursorPos=reinterpret_cast<void*>(GetProcAddress(user32,"SetCursorPos"));
        const auto clipCursor=reinterpret_cast<void*>(GetProcAddress(user32,"ClipCursor"));
        if((initStatus==MH_OK||initStatus==MH_ERROR_ALREADY_INITIALIZED)
            &&MH_CreateHook(setCursor,reinterpret_cast<void*>(&panelSetCursor),reinterpret_cast<void**>(&originalSetCursor))==MH_OK
            &&MH_CreateHook(setCursorPos,reinterpret_cast<void*>(&panelSetCursorPos),reinterpret_cast<void**>(&originalSetCursorPos))==MH_OK
            &&MH_CreateHook(clipCursor,reinterpret_cast<void*>(&panelClipCursor),reinterpret_cast<void**>(&originalClipCursor))==MH_OK
            &&MH_CreateHook(vtable[8],reinterpret_cast<void*>(&onPresent),reinterpret_cast<void**>(&originalPresent))==MH_OK
            &&MH_CreateHook(vtable[13],reinterpret_cast<void*>(&onResize),reinterpret_cast<void**>(&originalResize))==MH_OK
            &&MH_EnableHook(setCursor)==MH_OK&&MH_EnableHook(setCursorPos)==MH_OK&&MH_EnableHook(clipCursor)==MH_OK
            &&MH_EnableHook(vtable[13])==MH_OK&&MH_EnableHook(vtable[8])==MH_OK)nativeLog("Overlay and panel cursor hooks installed");
        else nativeLog("OVERLAY_START_ERROR: cannot install DX11 hooks");
        swap->Release();probeContext->Release();probeDevice->Release();
    }else nativeLog("OVERLAY_START_ERROR: cannot create DX11 device");
    DestroyWindow(hidden);UnregisterClassW(wc.lpszClassName,wc.hInstance);return 0;
}
}
// 新版管理协议标记，Lua 与安装程序都据此拒绝旧界面混装。
// 游戏线程直接同步许可，不依赖每秒发布的 UI 文件；关闭后所有窗口输入直通。
extern "C" __declspec(dllexport) int dualsp_runtime_gate(void* luaState) {
    const dualsp::LuaApi* api=nullptr; // 先禁用再报告接口错误。
    try {
        static const dualsp::LuaApi bound;api=&bound;
        if(api->top(luaState)!=1)throw std::runtime_error("Runtime gate expects one argument");
        const auto enabled=api->readInteger(luaState,1); // 只允许游戏线程提供 0 或 1。
        if(enabled!=0&&enabled!=1)throw std::runtime_error("Invalid UI runtime gate value");
        std::lock_guard<std::recursive_mutex> guard(renderMutex);
        runtimeEnabled.store(enabled!=0);reseedPad.store(true);
        if(!enabled) {
            if(panel)togglePanel(false);
            captureSlot=-1;suppressedCaptureKey=0;pendingSave=0;notice.clear();
            std::lock_guard<std::mutex> queueGuard(queueMutex);actions.clear();
        }
        api->boolean(luaState,true);api->string(luaState,"",0);return 2;
    }catch(const std::exception& error) {
        runtimeEnabled.store(false);nativeLog(std::string("UI_RUNTIME_GATE_ERROR ")+error.what());
        if(api){api->boolean(luaState,false);api->string(luaState,error.what(),std::strlen(error.what()));return 2;}
        return 0;
    }
}

// 保留已有具名握手；新控制器同时要求第 5 版参数能力。
extern "C" __declspec(dllexport) int dualsp_three_sp_supported(void*) {return 0;}
extern "C" __declspec(dllexport) int dualsp_management_supported(void*) {return 0;}
extern "C" __declspec(dllexport) int dualsp_shortcuts_supported(void*) {return 0;}
// 第 5 版参数握手：避免新控制器误装仍可关闭中央提示的旧界面。
extern "C" __declspec(dllexport) int dualsp_management_options_supported(void*) {return 0;}
// 调试换装保留独立能力标记；Lua 要求此扩展存在。
extern "C" __declspec(dllexport) int dualsp_debug_loadout_supported(void*) {return 0;}
// 原生菜单会消费导航键，准备/补给页直接读真实按住状态；不发送或拦截键盘输入。
extern "C" __declspec(dllexport) int dualsp_preparation_navigation(void* luaState) {
    try {
        static const dualsp::LuaApi api; // 沿用已核对的专用加载器栈接口。
        std::lock_guard<std::recursive_mutex> guard(renderMutex);
        const bool ready=initialized&&gameWindow;
        const bool enabled=runtimeEnabled.load()&&ready&&!panel&&gameFocused(); // 多人、失焦及 F8 打开时不驱动选择。
        const bool up=enabled&&((GetAsyncKeyState(VK_UP)&0x8000)||(GetAsyncKeyState('W')&0x8000));
        const bool down=enabled&&((GetAsyncKeyState(VK_DOWN)&0x8000)||(GetAsyncKeyState('S')&0x8000));
        api.boolean(luaState,up);api.boolean(luaState,down);api.boolean(luaState,ready);
        return 3;
    }catch(const std::exception& error) {
        nativeLog(std::string("PREPARATION_INPUT_ERROR ")+error.what());
        return 0; // Lua 必须检查返回类型并报告，不能静默换回已确认失效的来源。
    }
}
extern "C" __declspec(dllexport) int dualsp_overlay_start(void*) {
    if(!startup.exchange(true)){HANDLE thread=CreateThread(nullptr,0,startThread,nullptr,0,nullptr);if(thread)CloseHandle(thread);else nativeLog("OVERLAY_START_ERROR: worker thread");}
    return 0;
}
extern "C" __declspec(dllexport) int dualsp_publish(void*) {
    const auto pending=modRoot()/"ui-state.pending.json",file=modRoot()/"ui-state.json";
    try {
        std::ifstream input(pending);Json fresh;input>>fresh;input.close();
        {std::lock_guard<std::mutex> guard(publishedMutex);
            publishedState=std::move(fresh);publishedGeneration.fetch_add(1,std::memory_order_release);}
        // Retain a low-frequency diagnostic snapshot, not a render-thread IPC.
        if(!MoveFileExW(pending.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING))throw std::runtime_error("UI diagnostic publish failed");
    }catch(const std::exception& e){nativeLog(std::string("UI_PUBLISH_ERROR ")+e.what());}
    return 0;
}
extern "C" __declspec(dllexport) int dualsp_drain_actions(void*) {
    std::vector<Json> batch;{std::lock_guard<std::mutex> guard(queueMutex);batch.swap(actions);}
    if(batch.empty())return 0;
    // Only actual user actions need a file. This ephemeral handoff must be
    // atomically complete, but need not force a durable storage flush.
    try {atomicJson(modRoot()/"ui-actions.json",batch,false);}catch(const std::exception& e){nativeLog(e.what());}return 0;
}

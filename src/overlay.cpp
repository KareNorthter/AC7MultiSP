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
#include <unordered_set>
#include <sstream>
#include <iterator>
#include <cmath>
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "MinHook.h"
#include "panel_pointer.hpp"
#include "keyboard_shortcuts.hpp"
#include "ue4ss_lua_api.hpp"
#include "ui_messages.hpp"
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
constexpr int managementProtocol=15; // 手柄布局与HUD选项整合，核心/UI/Lua须成套部署。
bool draftModEnabled=true,draftFixedMountEnabled=true,draftGunSalvo=false;
bool draftTargetGazePadL3=false,draftCameraHold=false;
bool draftCameraR3=true;
int draftTargetGazeKey=0,targetGazeResponse=0;
int draftCameraCenterKey=VK_MBUTTON,draftCameraSensitivity=100,draftCameraAutoCenterDegrees=30;
bool suppressedCaptureMouse=false;
int draftAmmoPercentTwo=50,draftAmmoPercentThree=33;
std::array<int,4> draftKeys{49,50,51,52};
std::array<dualsp::Shortcut,4> shortcuts{{{VK_F6,0},{VK_F6,3},{VK_F7,3},{VK_F5,3}}};
std::array<dualsp::Shortcut,4> draftShortcuts=shortcuts;
std::string draftLanguage="auto",notice,managementError; // 新草稿默认跟随游戏文本语言。
std::string draftCentralWeaponDisplay="always"; // 默认保留中央简称常驻。
std::array<bool,3> debugSelected{}; // 按原生配置顺序选择，连续组成 SP1 至 SP3。
int debugRequest=0,pendingDebug=0,debugSession=-1,debugRevision=-1; // 请求回执与草稿身份。
std::string debugNotice; // 换装成功、拒绝或失败的独立反馈。
struct DebugAircraftOption { int planeId{};std::string name,code;bool mod{}; };
std::vector<DebugAircraftOption> debugAircraftOptions; // 游戏线程从实际目录核准的官方及Mod机体。
std::array<char,128> aircraftSearch{};
int aircraftSourceFilter=0,aircraftInitialFilter=0; // 全部/官方/Mod；全部/A-Z/数字及其他。
int aircraftRequest=0,pendingAircraft=0,aircraftSession=-1,aircraftRevision=-1,aircraftTarget=-1;
bool aircraftAvailable=false,aircraftSupported=false;
std::string aircraftMode="restart",aircraftStatus="idle",aircraftNotice,aircraftReason,aircraftDataError;
int hotkeyRequest=0,hotkeyResponse=0;
int masterResponse=0;
std::array<bool,256> consumedShortcutKeys{};
std::string hotkeyToast;
ULONGLONG hotkeyToastUntil{};
bool toastIsError=false;
Json reportedToastErrors=Json::object(); // 同一持续错误只提示一次，恢复后再次发生才重新提示。
const char* tr(const char* zh,const char* en);
void panelHelp(const char* zh,const char* en);
std::string uiMessage(const std::string& message,
    const char* zh="操作未完成，请在日志页查看详细原因。",
    const char* en="The operation could not be completed. See Logs for details.");
void showToast(std::string message,bool error=false) {
    const std::string prefix="AC7MultiSP: ";
    if(message.rfind(prefix,0)==0)message.erase(0,prefix.size()); // 统一入口只添加一次前缀。
    message=uiMessage(message);
    // Lua保留完整堆栈供日志分析，悬浮窗只显示简短原因；截断不切开UTF-8字符。
    if(message.size()>240) {
        size_t end=240;
        while(end && (static_cast<unsigned char>(message[end])&0xc0)==0x80)--end;
        message.resize(end);message+="…";
    }
    if(message.empty())return;
    hotkeyToast=prefix+message;toastIsError=error;
    hotkeyToastUntil=GetTickCount64()+(error?6500:4500);
}
int managementTab=0,requestedTab=-1; // Ctrl+Tab 在四个管理分页间循环。
bool focusManagement=false; // 每次打开把键盘焦点放到语言选择，不依赖鼠标点击。
// 日志只在页面打开或手动刷新时读取。
bool logsLoaded=false;
std::string modLogText,nativeLogText,logError;
// 输入函数声明供窗口消息使用；实现与管理页集中在后文。
void loadManagementDraft();
void loadDebugDraft();
void loadDebugAircraftDraft() noexcept;
bool debugAircraftBusy();
void captureKeyboardKey(int key,int mods);
int shortcutModifiers() {
    return ((GetKeyState(VK_CONTROL)&0x8000)?1:0)|((GetKeyState(VK_MENU)&0x8000)?2:0)|((GetKeyState(VK_SHIFT)&0x8000)?4:0);
}
bool shortcutModifierKey(int key) {
    return key==VK_CONTROL||key==VK_LCONTROL||key==VK_RCONTROL||key==VK_MENU||key==VK_LMENU||key==VK_RMENU||key==VK_SHIFT||key==VK_LSHIFT||key==VK_RSHIFT;
}
bool directInputEnabled();
bool targetGazeInputEnabled();
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
    if(msg==WM_KILLFOCUS){suppressedCaptureKey=0;suppressedCaptureMouse=false;captureSlot=-1;consumedShortcutKeys={};}
    // 回正键捕获也接受真实鼠标中键；原生可能只交付RAWINPUT，所以两条消息路径都处理。
    if(gameFocused() && (msg==WM_MBUTTONDOWN || msg==WM_MBUTTONUP) && (suppressedCaptureMouse || (panel && captureSlot==8))) {
        if(msg==WM_MBUTTONDOWN && !suppressedCaptureMouse){suppressedCaptureMouse=true;captureKeyboardKey(VK_MBUTTON,shortcutModifiers());}
        if(msg==WM_MBUTTONUP)suppressedCaptureMouse=false;
        return 0;
    }
    if(gameFocused() && msg==WM_INPUT && (suppressedCaptureMouse || (panel && captureSlot==8))) {
        RAWINPUT raw{};UINT size=sizeof(raw);
        if(GetRawInputData(reinterpret_cast<HRAWINPUT>(l),RID_INPUT,&raw,&size,sizeof(RAWINPUTHEADER))!=static_cast<UINT>(-1) && raw.header.dwType==RIM_TYPEMOUSE) {
            const auto flags=raw.data.mouse.usButtonFlags;
            if(flags&RI_MOUSE_MIDDLE_BUTTON_DOWN) {
                if(!suppressedCaptureMouse){suppressedCaptureMouse=true;captureKeyboardKey(VK_MBUTTON,shortcutModifiers());}
            }
            if(flags&RI_MOUSE_MIDDLE_BUTTON_UP)suppressedCaptureMouse=false;
            if(flags&(RI_MOUSE_MIDDLE_BUTTON_DOWN|RI_MOUSE_MIDDLE_BUTTON_UP))return DefWindowProcW(hwnd,msg,w,l);
        }
    }
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
        const bool editingText=panel&&ImGui::GetIO().WantTextInput;
        // 搜索时字母/数字属于输入框；管理界面的功能键入口仍可直接关闭窗口。
        const int index=dualsp::matchingShortcut(shortcuts,static_cast<int>(w),mods);
        if(index>=0&&(!editingText||(index==0&&w>=VK_F1&&w<=VK_F24))) {
            if(w<consumedShortcutKeys.size())consumedShortcutKeys[w]=true;
            if(index==0)togglePanel(!panel);
            else if(index==3){if(!panel)enqueue({{"type","toggle_gun_salvo"}});}
            else if(index==1&&debugAircraftBusy())showToast(tr("正在处理换机，请等待任务重开。","Aircraft change is pending; wait for the mission restart."));
            else if(state.contains("debug_hotkey")&&state["debug_hotkey"].value("protocol",0)==1&&state.contains("debug_loadout")) {
                const auto& debug=state["debug_loadout"];
                enqueue({{"type","debug_hotkey"},{"hotkey_protocol",1},{"request",++hotkeyRequest},
                    {"session",debug.value("session",-1)},{"revision",debug.value("revision",-1)},
                    {"command",index==1?"cycle":"mount"}});
            }
            return 0;
        }
        if(targetGazeInputEnabled()&&dualsp::singleKeyMatches(state.value("target_gaze_key",0),static_cast<int>(w),mods)) {
            if(w<consumedShortcutKeys.size())consumedShortcutKeys[w]=true;
            enqueue({{"type","toggle_target_gaze"},{"source","keyboard"},{"generation",state.at("target_gaze").value("generation",-1)}});
            return 0;
        }
        if(directInputEnabled()) {
            const auto& keys=state.at("keyboard_keys"); // 使用已保存值，管理草稿不影响飞行。
            for(int slot=0;slot<4;++slot)if(dualsp::singleKeyMatches(keys.at(slot).get<int>(),static_cast<int>(w),mods)) {
                enqueue({{"type","select_weapon"},{"target",slot}});break;
            }
            // 直选键和修饰键继续交给原生输入，不打断加速等持续操作。
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
    if(std::filesystem::exists(font))io.Fonts->AddFontFromFileTTF(font.string().c_str(),22.f,nullptr,io.Fonts->GetGlyphRangesChineseFull());
    else io.Fonts->AddFontDefault();
    // 深色卡片、青绿强调色；增加留白，普通说明由帮助按钮按需展示。
    ImGui::StyleColorsDark();auto& s=ImGui::GetStyle();
    s.WindowRounding=14;s.ChildRounding=10;s.FrameRounding=6;s.PopupRounding=8;s.GrabRounding=6;s.TabRounding=6;
    s.WindowPadding={24,20};s.FramePadding={12,7};s.ItemSpacing={12,10};s.ItemInnerSpacing={8,6};
    s.ChildBorderSize=1;s.WindowBorderSize=1;s.ScrollbarSize=10;
    s.Colors[ImGuiCol_WindowBg]={.055f,.069f,.09f,.99f};s.Colors[ImGuiCol_ChildBg]={.080f,.099f,.125f,1};
    s.Colors[ImGuiCol_PopupBg]={.085f,.105f,.135f,.99f};s.Colors[ImGuiCol_Border]={.18f,.23f,.28f,.65f};
    s.Colors[ImGuiCol_FrameBg]={.12f,.15f,.19f,1};s.Colors[ImGuiCol_FrameBgHovered]={.16f,.23f,.27f,1};
    s.Colors[ImGuiCol_FrameBgActive]={.18f,.29f,.32f,1};s.Colors[ImGuiCol_Text]={.90f,.93f,.96f,1};
    s.Colors[ImGuiCol_TextDisabled]={.53f,.61f,.68f,1};s.Colors[ImGuiCol_Button]={.11f,.30f,.31f,1};
    s.Colors[ImGuiCol_ButtonHovered]={.14f,.42f,.41f,1};s.Colors[ImGuiCol_ButtonActive]={.17f,.52f,.48f,1};
    s.Colors[ImGuiCol_Header]={.12f,.25f,.29f,1};s.Colors[ImGuiCol_HeaderHovered]={.15f,.34f,.37f,1};
    s.Colors[ImGuiCol_HeaderActive]={.18f,.40f,.41f,1};s.Colors[ImGuiCol_CheckMark]={.35f,.86f,.74f,1};
    s.Colors[ImGuiCol_SliderGrab]={.30f,.75f,.68f,1};s.Colors[ImGuiCol_SliderGrabActive]={.40f,.92f,.80f,1};
    s.Colors[ImGuiCol_Tab]={.08f,.12f,.16f,1};s.Colors[ImGuiCol_TabSelected]={.13f,.29f,.32f,1};
    s.Colors[ImGuiCol_TabHovered]={.15f,.36f,.38f,1};s.Colors[ImGuiCol_Separator]={.20f,.27f,.33f,.7f};
    s.Colors[ImGuiCol_NavCursor]={.39f,.84f,.79f,1};
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
std::string uiMessage(const std::string& message,const char* zh,const char* en) {
    return dualsp::localizedUiMessage(message,std::string(tr("zh","en"))=="zh",tr(zh,en));
}

// 单键绑定支持常用键盘键；当前管理入口和 Esc 不能绑定为武器选择。
bool supportedKey(int key) { return dualsp::supportedShortcutKey(key); }

// 返回稳定的键位名称，不依赖 Windows 显示语言。
std::string keyLabel(int key) {
    if((key>='0'&&key<='9')||(key>='A'&&key<='Z'))return std::string(1,static_cast<char>(key));
    if(key>=VK_F1&&key<=VK_F24)return "F"+std::to_string(key-VK_F1+1);
    if(key>=VK_NUMPAD0&&key<=VK_NUMPAD9)return std::string(tr("小键盘 ","Numpad "))+std::to_string(key-VK_NUMPAD0);
    switch(key) {
        case VK_MBUTTON:return tr("鼠标中键","Middle mouse");
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
    if(value.key==0)return tr("未绑定","Unassigned");
    return std::string(value.mods&1?"Ctrl+":"")+(value.mods&2?"Alt+":"")+(value.mods&4?"Shift+":"")+keyLabel(value.key);
}
std::array<dualsp::Shortcut,4> readShortcuts(const Json& data) {
    std::array<dualsp::Shortcut,4> values;
    const char* names[]={"management","cycle","mount","salvo"};
    for(int i=0;i<4;++i) {
        const auto& item=data.at(names[i]);
        if(!item.at("key").is_number_integer()||!item.at("mods").is_number_integer())throw std::runtime_error("快捷键格式无效");
        values[i]={item.at("key").get<int>(),item.at("mods").get<int>()};
        if(!(i==3&&values[i].key==0&&values[i].mods==0)&&!dualsp::validShortcut(values[i]))throw std::runtime_error("快捷键不可用");
    }
    return values;
}
Json shortcutData(const std::array<dualsp::Shortcut,4>& values) {
    Json data=Json::object();const char* names[]={"management","cycle","mount","salvo"};
    for(int i=0;i<4;++i)data[names[i]]={{"key",values[i].key},{"mods",values[i].mods}};
    return data;
}
bool bindingsValid() { return dualsp::validShortcuts(draftShortcuts,draftKeys)&&dualsp::validTargetGazeKey(draftTargetGazeKey,draftShortcuts,draftKeys)
    &&dualsp::validCameraCenterKey(draftCameraCenterKey,draftCameraHold,draftTargetGazeKey,draftShortcuts,draftKeys); }

// 按键只产生请求，是否允许切换由游戏线程核对当前任务状态。
bool directInputEnabled() {
    return runtimeEnabled.load()&&!panel&&managementError.empty()&&gameFocused()&&state.value("management_protocol",0)==managementProtocol
        &&state.value("direct_selection_available",false);
}
bool targetGazeInputEnabled() {
    return runtimeEnabled.load()&&!panel&&managementError.empty()&&gameFocused()&&state.value("management_protocol",0)==managementProtocol
        &&state.contains("target_gaze")&&state.at("target_gaze").value("available",false);
}

// 捕获单次按键；错误显式提示，保持等待，不把无效输入换成其他键。
void captureKeyboardKey(int key,int mods) {
    if(key==VK_ESCAPE){captureSlot=-1;notice.clear();return;}
    auto nextKeys=draftKeys;auto nextShortcuts=draftShortcuts;int nextGaze=draftTargetGazeKey,nextCenter=draftCameraCenterKey;
    if(captureSlot<4) {
        if(mods!=0){notice=tr("武器直接选择只支持单键。","Direct weapon selection accepts single keys.");return;}
        nextKeys[captureSlot]=key;
    } else if(captureSlot==7) {
        if(mods!=0){notice=tr("目标注视只支持单键。","Target tracking accepts a single key.");return;}
        nextGaze=key;
    } else if(captureSlot==8) {
        if(mods!=0){notice=tr("镜头回正只支持单键。","Camera recenter accepts a single key.");return;}
        nextCenter=key;
    } else nextShortcuts[captureSlot==9?3:captureSlot-4]={key,mods};
    if(!dualsp::validShortcuts(nextShortcuts,nextKeys)||!dualsp::validTargetGazeKey(nextGaze,nextShortcuts,nextKeys)
        ||!dualsp::validCameraCenterKey(nextCenter,draftCameraHold,nextGaze,nextShortcuts,nextKeys)) {
        notice=tr("键位不可用或重复；请换一个按键。Esc取消。","Unsupported or duplicate binding. Choose another key; Esc cancels.");return;
    }
    draftKeys=nextKeys;draftShortcuts=nextShortcuts;draftTargetGazeKey=nextGaze;draftCameraCenterKey=nextCenter;captureSlot=-1;notice.clear();
}

// 管理页打开时加载保存值；未保存的草稿不改变游戏输入。
void loadManagementDraft() {
    draftLanguage=state.at("language").get<std::string>();
    draftModEnabled=state.at("mod_enabled").get<bool>();
    draftFixedMountEnabled=state.at("fixed_mount_enabled").get<bool>();
    draftGunSalvo=state.at("gun_salvo_enabled").get<bool>();
    draftCentralWeaponDisplay=state.at("central_weapon_display").get<std::string>();
    draftTargetGazePadL3=state.at("target_gaze_pad_l3").get<bool>();
    draftCameraHold=state.at("camera_hold_enabled").get<bool>();
    draftCameraR3=state.at("camera_r3_enabled").get<bool>();
    draftCameraCenterKey=state.at("camera_center_key").get<int>();
    draftCameraSensitivity=state.at("camera_sensitivity").get<int>();
    draftCameraAutoCenterDegrees=state.at("camera_auto_center_degrees").get<int>();
    draftTargetGazeKey=state.at("target_gaze_key").get<int>();
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
    loadDebugDraft();loadDebugAircraftDraft();
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

// 换机子协议独立于管理设置和SP换装；缺失或错误只禁用换机区域。
bool debugAircraftBusy() {return pendingAircraft>0||(aircraftSupported&&aircraftStatus=="restarting");}
void loadDebugAircraftDraft() noexcept {
    try {
        if(!state.contains("debug_aircraft")) {
            aircraftSupported=aircraftAvailable=false;debugAircraftOptions.clear();aircraftDataError.clear();return;
        }
        const auto& data=state.at("debug_aircraft");
        if(data.at("protocol").get<int>()!=2)throw std::runtime_error("换机协议不匹配");
        const int session=data.at("session").get<int>(),revision=data.at("revision").get<int>();
        const int response=data.at("response").get<int>(),ack=data.at("ack").get<int>();
        const auto mode=data.value("mode",std::string("restart")),status=data.value("status",std::string("idle"));
        if(mode!="restart"&&mode!="next_restart")throw std::runtime_error("换机生效方式无效");
        if(status!="idle"&&status!="restarting"&&status!="applied"&&status!="failed")throw std::runtime_error("换机状态无效");
        const auto& options=data.at("options");
        if(!options.is_array()||options.size()>4096)throw std::runtime_error("换机候选列表无效");
        std::vector<DebugAircraftOption> next;
        std::unordered_set<int> seen;
        for(const auto& option:options) {
            if(!option.at("plane_id").is_number_integer())throw std::runtime_error("换机候选编号无效");
            const int id=option.at("plane_id").get<int>();const auto name=option.value("label",option.at("name").get<std::string>());
            const auto code=option.at("code").get<std::string>();const bool mod=option.at("mod").get<bool>();
            if(id<0||name.empty()||name.size()>1024||code.empty()||code.size()>128||!seen.insert(id).second)
                throw std::runtime_error("换机候选编号重复或名称为空");
            next.push_back({id,name,code,mod});
        }
        const bool changedSession=session!=aircraftSession;
        const bool appliedNow=status=="applied"&&aircraftStatus!="applied";
        aircraftSession=session;aircraftRevision=revision;aircraftMode=mode;aircraftStatus=status;
        aircraftAvailable=data.at("available").get<bool>();aircraftReason=data.value("reason",std::string());
        aircraftSupported=true;aircraftDataError.clear();debugAircraftOptions=std::move(next);
        aircraftRequest=std::max(aircraftRequest,response);
        const auto contains=[&](int id){return std::any_of(debugAircraftOptions.begin(),debugAircraftOptions.end(),[&](const auto& option){return option.planeId==id;});};
        if(changedSession||!contains(aircraftTarget)) {
            const int current=state.value("plane_id",-1);
            aircraftTarget=contains(current)?current:debugAircraftOptions.empty()?-1:debugAircraftOptions.front().planeId;
        }
        if(pendingAircraft>0&&response==pendingAircraft) {
            const bool accepted=ack==pendingAircraft&&status!="failed";
            aircraftNotice=accepted?(status=="applied"?tr("换机已在重开的任务中生效。","Aircraft change applied in the restarted mission."):
                mode=="next_restart"?tr("机体已选择；请手动重开任务后生效。","Aircraft selected; restart the mission to apply it."):
                tr("请求已接受，正在重开任务；换机尚待生效。","Request accepted; restarting the mission. Aircraft change is not applied yet.")):
                tr("换机请求未完成，请查看错误说明。","Aircraft change was not completed; see the error details.");
            const auto message=data.value("notice",std::string());if(!message.empty())aircraftNotice=message;
            pendingAircraft=0;showToast(aircraftNotice,!accepted);
        } else if(appliedNow) {
            aircraftNotice=data.value("notice",std::string());
            if(aircraftNotice.empty())aircraftNotice=tr("换机已在重开的任务中生效。","Aircraft change applied in the restarted mission.");
            showToast(aircraftNotice);
        } else if(data.contains("notice")&&!data.at("notice").get<std::string>().empty())aircraftNotice=data.at("notice").get<std::string>();
        else if(changedSession&&status=="idle"&&pendingAircraft==0)aircraftNotice.clear();
    }catch(const std::exception& error) {
        aircraftSupported=aircraftAvailable=false;debugAircraftOptions.clear();
        if(aircraftDataError!=error.what()) {
            showToast(std::string(tr("换机状态错误：","Aircraft state error: "))+error.what(),true);
            nativeLog(std::string("DEBUG_AIRCRAFT_DATA_ERROR ")+error.what());
        }
        aircraftDataError=error.what();
    }
}
// 名称搜索忽略英文大小写及常见分隔符；中文等UTF-8字节原样保留。
std::string aircraftSearchKey(const std::string& text) {
    std::string result;
    for(unsigned char c:text) {
        if(c>='A'&&c<='Z')c=static_cast<unsigned char>(c+'a'-'A');
        if((c>='a'&&c<='z')||(c>='0'&&c<='9')||c>=128)result.push_back(static_cast<char>(c));
    }
    return result;
}
int aircraftInitial(const DebugAircraftOption& option) {
    const auto key=aircraftSearchKey(uiMessage(option.name));
    return !key.empty()&&key.front()>='a'&&key.front()<='z'?key.front()-'a'+1:27;
}
void drawDebugAircraft() {
    ImGui::SeparatorText(tr("任务内换机","Aircraft change"));
    if(state.contains("management_pause")&&state.at("management_pause").value("existing",false))
        ImGui::TextWrapped("%s",tr("任务暂停中可直接换机；关闭管理会返回暂停菜单。",
            "Aircraft changes are available while paused. Closing management returns to the pause menu."));
    ImGui::TextWrapped("%s",aircraftMode=="next_restart"?
        tr("选择机体后请手动重开本任务，换机才会生效；当前飞行中的机体不会立即替换。","Select an aircraft, then manually restart this mission to apply it. The current aircraft is not replaced immediately."):
        tr("选择机体后点击下方按钮，将重开本任务并使用目标机体；当前任务进度会重置。","Choose an aircraft and use the button below to restart this mission with it. Current mission progress will reset."));
    panelHelp("包含实际加载目录中具有完整三SP数据的官方及Mod机体，也列出未购买或缺DLC许可的官方机体。Mod名称缺失时显示代号。只替换外观的Mod沿用原机体条目；不改购买记录，实际生成仍由游戏和对应Mod决定。",
        "Includes official and mod aircraft with complete three-SP data in the loaded catalogue, including unpurchased or unlicensed official aircraft. Missing mod names fall back to codes. Visual replacements use the base aircraft entry. Purchase records stay unchanged; spawning depends on the game and the installed mod.");
    const bool busy=debugAircraftBusy();
    const bool available=aircraftSupported&&aircraftAvailable&&!busy&&pendingDebug==0;
    const auto selected=std::find_if(debugAircraftOptions.begin(),debugAircraftOptions.end(),[](const auto& option){return option.planeId==aircraftTarget;});
    const std::string preview=selected!=debugAircraftOptions.end()?uiMessage(selected->name)+"  ["+std::to_string(selected->planeId)+"]":tr("暂无可用机体","No available aircraft");
    ImGui::BeginDisabled(!available);ImGui::TextUnformatted(tr("目标机体","Target aircraft"));
    ImGui::SetNextItemWidth(std::min(520.f,ImGui::GetContentRegionAvail().x));
    if(ImGui::BeginCombo("###DebugAircraftTarget",preview.c_str(),ImGuiComboFlags_HeightLargest)) {
        if(ImGui::IsWindowAppearing())ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(std::max(140.f,ImGui::GetContentRegionAvail().x-72));
        ImGui::InputTextWithHint("##AircraftSearch",tr("搜索名称 / 代号 / 编号","Search name / code / ID"),aircraftSearch.data(),aircraftSearch.size());
        ImGui::SameLine();if(ImGui::SmallButton(tr("重置","Reset"))) {aircraftSearch.fill(0);aircraftSourceFilter=aircraftInitialFilter=0;}
        ImGui::SetNextItemWidth(std::max(110.f,ImGui::GetContentRegionAvail().x*.48f));
        ImGui::Combo("##AircraftSource",&aircraftSourceFilter,tr("全部机体\0官方机体\0Mod机体\0","All aircraft\0Official aircraft\0Mod aircraft\0"));
        ImGui::SameLine();ImGui::SetNextItemWidth(std::max(90.f,ImGui::GetContentRegionAvail().x));
        ImGui::Combo("##AircraftInitial",&aircraftInitialFilter,tr(
            "全部首字母\0A\0B\0C\0D\0E\0F\0G\0H\0I\0J\0K\0L\0M\0N\0O\0P\0Q\0R\0S\0T\0U\0V\0W\0X\0Y\0Z\0数字 / 其他\0",
            "All initials\0A\0B\0C\0D\0E\0F\0G\0H\0I\0J\0K\0L\0M\0N\0O\0P\0Q\0R\0S\0T\0U\0V\0W\0X\0Y\0Z\0Digits / Other\0"));
        std::istringstream query(aircraftSearch.data());std::string word;std::vector<std::string> words;
        while(query>>word) {auto key=aircraftSearchKey(word);if(!key.empty())words.push_back(std::move(key));}
        std::vector<const DebugAircraftOption*> matches;
        for(const auto& option:debugAircraftOptions) {
            if((aircraftSourceFilter==1&&option.mod)||(aircraftSourceFilter==2&&!option.mod)||
                (aircraftInitialFilter&&aircraftInitial(option)!=aircraftInitialFilter))continue;
            const auto haystack=aircraftSearchKey(uiMessage(option.name))+" "+aircraftSearchKey(option.code)+" "+std::to_string(option.planeId);
            if(std::all_of(words.begin(),words.end(),[&](const auto& key){return haystack.find(key)!=std::string::npos;}))matches.push_back(&option);
        }
        ImGui::TextDisabled(tr("匹配 %d / %d","Matches %d / %d"),static_cast<int>(matches.size()),static_cast<int>(debugAircraftOptions.size()));
        ImGui::BeginChild("AircraftResults",{0,250},ImGuiChildFlags_Borders,ImGuiWindowFlags_HorizontalScrollbar);
        ImGuiListClipper clipper;clipper.Begin(static_cast<int>(matches.size()));
        while(clipper.Step())for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i) {
            const auto& option=*matches[i];const bool chosen=option.planeId==aircraftTarget;
            const auto label=uiMessage(option.name)+"  ["+std::to_string(option.planeId)+"]"+(option.mod?"  Mod":"");
            ImGui::PushID(option.planeId);
            if(ImGui::Selectable(label.c_str(),chosen)) {aircraftTarget=option.planeId;ImGui::CloseCurrentPopup();}
            ImGui::PopID();
        }
        if(matches.empty())ImGui::TextWrapped("%s",tr("没有匹配机体，请修改搜索或重置筛选。","No matching aircraft. Change the search or reset filters."));
        ImGui::EndChild();
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    const bool same=aircraftTarget==state.value("plane_id",-1);
    if(same&&selected!=debugAircraftOptions.end())ImGui::TextDisabled("%s",tr("这是当前机体，请选择其他机体。","This is the current aircraft; choose another."));
    if(!aircraftSupported)ImGui::TextWrapped("%s",aircraftDataError.empty()?tr("当前控制器未提供任务换机。","The controller does not provide mission aircraft changes."):uiMessage(aircraftDataError,"换机资料不可用，请查看日志。","Aircraft information is unavailable. See Logs for details.").c_str());
    else if(!aircraftAvailable&&!aircraftReason.empty())ImGui::TextWrapped("%s",uiMessage(aircraftReason,"当前无法换机，请查看日志。","Aircraft change is unavailable. See Logs for details.").c_str());
    if(pendingDebug>0)ImGui::TextWrapped("%s",tr("正在应用SP方案，请等待完成。","An SP loadout is being applied; wait for it to finish."));
    ImGui::BeginDisabled(!available||same||selected==debugAircraftOptions.end());
    if(ImGui::Button(aircraftMode=="next_restart"?
        tr("选择机体，等待重开任务###ApplyAircraft","Select aircraft for next restart###ApplyAircraft"):
        tr("换机并重开任务###ApplyAircraft","Change aircraft and restart mission###ApplyAircraft"),
        {std::min(ImGui::GetContentRegionAvail().x,520.f),36})) {
        pendingAircraft=++aircraftRequest;
        enqueue({{"type","debug_aircraft"},{"debug_aircraft_protocol",2},{"request",pendingAircraft},
            {"session",aircraftSession},{"revision",aircraftRevision},{"target_plane_id",aircraftTarget}});
        aircraftNotice=tr("已提交换机请求，等待游戏线程确认…","Aircraft change submitted; waiting for the game thread...");
    }
    ImGui::EndDisabled();
    if(!aircraftNotice.empty())ImGui::TextWrapped("%s",uiMessage(aircraftNotice,"换机未完成，请查看日志。","Aircraft change was not completed. See Logs for details.").c_str());
    if(state.contains("debug_aircraft")&&state.at("debug_aircraft").contains("error")&&state.at("debug_aircraft").at("error").is_string())
        ImGui::TextWrapped("%s",uiMessage(state.at("debug_aircraft").at("error").get<std::string>(),"换机未完成，请查看日志。","Aircraft change failed. See Logs for details.").c_str());
}

// 调试仅发送完整方案与任务版本，游戏线程重新校验身份、购买与活动武器。
void drawDebugLoadout(bool focus) {
    const auto& debug=state.at("debug_loadout"); // 发布状态是只读快照。
    ImGui::Text(tr("当前飞机：%s","Current aircraft: %s"),uiMessage(state.value("plane_name",std::string())).c_str());
    panelHelp("按列表从上到下分配SP1/SP2/SP3。应用后按已保存比例补满SP并切回MSL；普通导弹、机炮、干扰弹保留，个人方案和已有检查点不改写。组合轮切及模型快捷键在基础页设置。",
        "Checked weapons become SP1/SP2/SP3 from top to bottom. Applying refills SPs with saved percentages and selects MSL; standard missiles, gun, countermeasures, saved loadouts and checkpoints remain unchanged. Configure loadout/model shortcuts in General.");
    const bool available=debug.value("available",false)&&pendingDebug==0&&!debugAircraftBusy(); // 等待回执时不能重复提交。
    const auto options=debug.value("options",Json::array()); // 没有任务时不展示猜测兵装。
    const int count=static_cast<int>(std::count(debugSelected.begin(),debugSelected.end(),true)); // 实际选择数量。
    bool valid=count>0&&options.is_array()&&options.size()==3; // UAV/ECM 多挂限制同步显示。
    if(options.is_array()&&options.size()==3) {
        for(int index=0;index<3;++index) {
            const auto& option=options.at(index); // 原生配置项包含购买和多挂许可。
            const bool owned=option.at("owned").get<bool>();
            const bool multiple=option.at("available").get<bool>();
            const auto label=option.at("code").get<std::string>()+" / "+uiMessage(option.at("description").get<std::string>(),"特殊兵装","Special weapon")+"###DebugSP"+std::to_string(index);
            const bool selectable=owned||debug.value("allow_unowned",false);
            ImGui::BeginDisabled(!available||!selectable);
            if(focus&&available&&selectable){ImGui::SetKeyboardFocusHere();focus=false;}
            ImGui::Checkbox(label.c_str(),&debugSelected[index]);
            ImGui::EndDisabled();
            if(!owned){ImGui::SameLine();ImGui::TextDisabled("%s",tr("未购买（任务调试可用）","Not purchased (mission debug allowed)"));}
            if(!multiple){ImGui::SameLine();ImGui::TextDisabled("%s",tr("仅限单挂","Single SP only"));}
        }
        // 同帧勾选后重新计算，不能沿用绘制前的数量判定或配弹预览。
        const int selected=static_cast<int>(std::count(debugSelected.begin(),debugSelected.end(),true));
        valid=selected>0;
        for(int index=0;index<3;++index)if(debugSelected[index])
            valid=valid&&(options.at(index).at("owned").get<bool>()||debug.value("allow_unowned",false))&&(selected==1||options.at(index).at("available").get<bool>());
        if(!valid)ImGui::TextWrapped("%s",tr("请选择 1 至 3 种 SP；标注仅限单挂的兵装不能组合。","Choose 1–3 SP weapons. Single-only weapons cannot be combined."));
    }
    if(!debug.value("available",false)&&debug.contains("reason"))
        ImGui::TextWrapped("%s",uiMessage(debug.at("reason").get<std::string>(),"当前无法调试兵装，请查看日志。","Loadout debugging is unavailable. See Logs for details.").c_str());

    ImGui::BeginDisabled(!available||!valid);
    if(ImGui::Button(tr("应用 SP 方案并补满###ApplyDebug","Apply SP loadout and refill###ApplyDebug"),
        {std::min(ImGui::GetContentRegionAvail().x,430.f),36})) {
        std::array<int,3> configs{-1,-1,-1};int slot=0; // 只在点击时形成连续逻辑槽。
        for(int index=0;index<3;++index)if(debugSelected[index])configs[slot++]=index;
        pendingDebug=++debugRequest;
        enqueue({{"type","debug_loadout"},{"debug_protocol",1},{"request",pendingDebug},
            {"session",debugSession},{"revision",debugRevision},{"profile",{{"a",configs[0]},{"b",configs[1]},{"c",configs[2]}}}});
        debugNotice=tr("正在应用，等待游戏线程确认…","Applying; waiting for the game thread...");
    }
    ImGui::EndDisabled();
    if(!debugNotice.empty())ImGui::TextWrapped("%s",uiMessage(debugNotice,"调试换装未完成，请查看日志。","The debug loadout was not applied. See Logs for details.").c_str());
    if(debug.contains("error"))ImGui::TextWrapped("%s",uiMessage(debug.at("error").get<std::string>(),"调试换装未完成，请查看日志。","The debug loadout failed. See Logs for details.").c_str());
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
        {"mod_enabled",draftModEnabled},{"fixed_mount_enabled",draftFixedMountEnabled},{"gun_salvo_enabled",draftGunSalvo},
        {"central_weapon_display",draftCentralWeaponDisplay},
        {"target_gaze_pad_l3",draftTargetGazePadL3},{"target_gaze_key",draftTargetGazeKey},{"camera_hold_enabled",draftCameraHold},
        {"camera_center_key",draftCameraCenterKey},{"camera_sensitivity",draftCameraSensitivity},
        {"camera_r3_enabled",draftCameraR3},{"camera_auto_center_degrees",draftCameraAutoCenterDegrees},
        {"ammo_percent_two",draftAmmoPercentTwo},{"ammo_percent_three",draftAmmoPercentThree},{"keyboard_keys",draftKeys},{"shortcuts",shortcutData(draftShortcuts)}});
    notice=tr("正在保存…","Saving...");
}

// 统一分页入口，让 Ctrl+Tab 的选择和鼠标选择共用当前页状态。
bool beginManagementTab(const char* label,int index) {
    const bool selected=ImGui::BeginTabItem(label,nullptr,requestedTab==index?ImGuiTabItemFlags_SetSelected:0); // 稳定分页 ID。
    if(selected)managementTab=index;
    return selected;
}
#include "management_panel.hpp"

// 消费游戏线程发布的完整状态；保留未保存草稿，按请求编号显示保存结果。
void refreshState() {
    static uint64_t consumedGeneration{}; // 一份状态只消费一次，不在每帧重复复制。
    if(publishedGeneration.load(std::memory_order_acquire)==consumedGeneration)return;
    try {
        const bool wasManagement=state.value("management_protocol",0)==managementProtocol;
        const bool wasCameraHold=state.value("camera_hold_enabled",false);
        const bool wasGunSalvo=state.value("gun_salvo_enabled",false);
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
            state.at("target_gaze_pad_l3").get<bool>();
            state.at("camera_hold_enabled").get<bool>();
            state.at("camera_r3_enabled").get<bool>();
            state.at("gun_salvo_enabled").get<bool>();
            const auto centralDisplay=state.at("central_weapon_display").get<std::string>();
            if(centralDisplay!="always"&&centralDisplay!="switch_1s")throw std::runtime_error("中央武器简称显示方式无效");
            if(state.at("camera_hold").at("protocol").get<int>()!=7)throw std::runtime_error("镜头保持协议不匹配");
            if(!state.at("camera_center_key").is_number_integer() || !dualsp::validCameraCenterKey(state.at("camera_center_key").get<int>(),
                state.at("camera_hold_enabled").get<bool>(),state.at("target_gaze_key").get<int>(),checkedShortcuts,checkedKeys))
                throw std::runtime_error("镜头回正键位无效或重复");
            if(!state.at("camera_sensitivity").is_number_integer() || state.at("camera_sensitivity").get<int>()<5 || state.at("camera_sensitivity").get<int>()>200)
                throw std::runtime_error("镜头灵敏度必须为5至200的整数");
            if(!state.at("target_gaze_key").is_number_integer() ||
                !dualsp::validTargetGazeKey(state.at("target_gaze_key").get<int>(),checkedShortcuts,checkedKeys))
                throw std::runtime_error("目标注视键位无效或重复");
            if(state.at("target_gaze").at("protocol").get<int>()!=2)throw std::runtime_error("目标注视协议不匹配");
            if(!state.at("camera_auto_center_degrees").is_number_integer() || state.at("camera_auto_center_degrees").get<int>()<0 || state.at("camera_auto_center_degrees").get<int>()>45)
                throw std::runtime_error("自动回正阈值必须为0至45的整数");
            shortcuts=checkedShortcuts;
        }
        const int sequence=state.value("sequence",0); // 状态序号用于记录控制器最近更新。
        if(sequence!=lastSequence){lastSequence=sequence;lastControllerUpdate=GetTickCount64();}
        const int request=state.value("request_open",0),close=state.value("request_close",0);
        if(request!=lastRequest){lastRequest=request;if(request>0)togglePanel(true);}
        if(close!=lastClose){lastClose=close;if(close>0)togglePanel(false);}
        if(!wasManagement&&panel&&state.value("management_protocol",0)==managementProtocol)loadManagementDraft();
        if(!panel)draftLanguage=state.value("language",std::string("auto"));
        if(!panel && wasManagement && wasGunSalvo!=state.at("gun_salvo_enabled").get<bool>()) {
            draftGunSalvo=state.at("gun_salvo_enabled").get<bool>();
            showToast(draftGunSalvo?tr("机炮齐射：开启","Gun salvo: ON"):tr("机炮齐射：关闭","Gun salvo: OFF"));
        }
        if(!panel && wasManagement && wasCameraHold!=state.at("camera_hold_enabled").get<bool>()) {
            draftCameraHold=state.at("camera_hold_enabled").get<bool>();
            showToast(draftCameraHold?tr("镜头保持：开启","Camera hold: ON"):tr("镜头保持：关闭","Camera hold: OFF"));
        }
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
        loadDebugAircraftDraft();
        const auto& gaze=state.at("target_gaze");
        if(gaze.value("response",0)!=targetGazeResponse) {
            targetGazeResponse=gaze.value("response",0);showToast(gaze.value("notice",std::string()));
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
        for(const char* field:{"debug_loadout","debug_aircraft","management_pause","master_switch","target_gaze","camera_hold"})
            if(state.contains(field)&&state.at(field).contains("error")&&state.at(field).at("error").is_string()
                &&!state.at(field).at("error").get_ref<const std::string&>().empty())errors[field]=state.at(field).at("error");
        for(auto item=errors.begin();item!=errors.end();++item) {
            if(!reportedToastErrors.contains(item.key())||reportedToastErrors.at(item.key())!=item.value()) {
                const bool visual=item.key()=="mount_warning";
                const auto detail=visual?uiMessage(item.value().get<std::string>(),"多挂外观已回退原生，请查看日志。",
                    "Multi-SP models reverted to native visuals. See Logs for details."):
                    uiMessage(item.value().get<std::string>());
                showToast(detail,true);break;
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
                const ULONGLONG pollInterval=(directInputEnabled()||targetGazeInputEnabled())?20:100;
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
                const bool popupAtFrameStart=ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel);
                ImGui::GetIO().MouseDrawCursor=panel&&foreground;ImGui::NewFrame();
                try {
                    if(panel)drawPanel();
                    if(toastVisible) {
                        ImGui::PushFont(nullptr,32.f); // 通知独立加大，不挤压管理面板。
                        ImGui::SetNextWindowPos({ImGui::GetIO().DisplaySize.x*.5f,72},ImGuiCond_Always,{.5f,0});
                        ImGui::SetNextWindowBgAlpha(.8f);
                        ImGui::Begin("##DebugHotkeyNotice",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoNav|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoFocusOnAppearing);
                        ImGui::PushTextWrapPos(std::min(ImGui::GetFontSize()*36,ImGui::GetIO().DisplaySize.x-80));
                        if(toastIsError)ImGui::PushStyleColor(ImGuiCol_Text,{1,.65f,.4f,1});
                        ImGui::TextUnformatted(hotkeyToast.c_str());
                        if(toastIsError)ImGui::PopStyleColor();
                        ImGui::PopTextWrapPos();ImGui::End();ImGui::PopFont();
                    }
                }catch(const Json::exception& e){notice=e.what();showToast(std::string(tr("界面错误：","UI error: "))+e.what(),true);nativeLog(std::string("UI_ERROR ")+e.what());}
                if(panel&&!popupAtFrameStart&&ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight)&&
                    !ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel))togglePanel(false);
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
extern "C" __declspec(dllexport) int dualsp_target_gaze_supported(void*) {return 0;}
extern "C" __declspec(dllexport) int dualsp_camera_r3_hold_toggle_supported(void*) {return 0;}
extern "C" __declspec(dllexport) int dualsp_camera_auto_center_supported(void*) {return 0;}
extern "C" __declspec(dllexport) int dualsp_camera_hold_supported(void*) {return 0;}
extern "C" __declspec(dllexport) int dualsp_camera_controls_supported(void*) {return 0;}
extern "C" __declspec(dllexport) int dualsp_camera_input_options_supported(void*) {return 0;}
extern "C" __declspec(dllexport) int dualsp_keyboard_modifier_routing_supported(void*) {return 0;}
extern "C" __declspec(dllexport) int dualsp_controller_clicks_supported(void*) {return 0;}
extern "C" __declspec(dllexport) int dualsp_gun_salvo_supported(void*) {return 0;}
extern "C" __declspec(dllexport) int dualsp_central_weapon_display_supported(void*) {return 0;}
// 第 5 版参数握手：避免新控制器误装仍可关闭中央提示的旧界面。
extern "C" __declspec(dllexport) int dualsp_management_options_supported(void*) {return 0;}
// 调试换装保留独立能力标记；Lua 要求此扩展存在。
extern "C" __declspec(dllexport) int dualsp_debug_loadout_supported(void*) {return 0;}
// 换机独立子协议1，不改变现有管理设置与SP换装协议。
extern "C" __declspec(dllexport) int dualsp_debug_aircraft_supported(void*) {return 0;}
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

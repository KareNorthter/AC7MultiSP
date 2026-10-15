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
// 管理草稿与保存回执；不再维护或提交 SP1/SP2 兵装选择。
int lastRequest=-1,lastClose=-1,saveRequest=0,pendingSave=0,captureSlot=-1,suppressedCaptureKey=0;
int draftRatioStep=0; // 0 至 10 对应 50% 至 100%，每 5% 一档，共 11 个可选值。
bool centralHint=true; // 唯一可关闭的 HUD 功能，右下列表和直接选择固定启用。
std::array<int,3> draftKeys{49,50,51};
int managementKey=VK_F8; // 管理入口由已校验状态提供；旧配置仍默认 F8。
std::string draftLanguage="auto",notice,managementError; // 新草稿默认跟随游戏文本语言。
// 日志只在页面打开或手动刷新时读取。
bool logsLoaded=false;
std::string modLogText,nativeLogText,logError;
// 输入函数声明供窗口消息使用；实现与管理页集中在后文。
const char* tr(const char* zh,const char* en);
void loadManagementDraft();
void captureKeyboardKey(int key);
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
        notice.clear();captureSlot=-1;managementError.clear();
        if(state.value("management_protocol",0)==2) {
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
    if(msg==WM_KILLFOCUS){suppressedCaptureKey=0;captureSlot=-1;}
    if((msg==WM_KEYDOWN||msg==WM_KEYUP)&&suppressedCaptureKey==static_cast<int>(w)) {
        if(msg==WM_KEYUP)suppressedCaptureKey=0;
        return 0;
    }
    if(msg==WM_KEYDOWN && !(l&(1LL<<30))) {
        // 捕获时不让同一个按键顺带激活按钮、关闭页面或触发游戏操作。
        if(panel&&captureSlot>=0){suppressedCaptureKey=static_cast<int>(w);captureKeyboardKey(static_cast<int>(w));return 0;}
        if(w==static_cast<WPARAM>(managementKey)){togglePanel(!panel);return 0;}
        if(w==VK_ESCAPE&&panel){togglePanel(false);return 0;}
        if(directInputEnabled()) {
            const auto keys=state.at("keyboard_keys"); // 使用已保存值，管理草稿不影响飞行。
            for(int slot=0;slot<3;++slot)if(keys.at(slot).get<int>()==static_cast<int>(w)) {
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
    const auto font=std::filesystem::path(L"C:/Windows/Fonts/msyh.ttc");
    if(std::filesystem::exists(font))io.Fonts->AddFontFromFileTTF(font.string().c_str(),20.f,nullptr,io.Fonts->GetGlyphRangesChineseFull());
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
bool supportedKey(int key,int reservedKey=managementKey) {
    return (key>=48&&key<=57)||(key>=65&&key<=90)||(key>=96&&key<=111)
        ||(key>=112&&key<=135&&key!=reservedKey)||(key>=186&&key<=192)||(key>=219&&key<=222)
        ||key==VK_BACK||key==VK_TAB||key==VK_RETURN||key==VK_SPACE
        ||(key>=VK_PRIOR&&key<=VK_DOWN)||key==VK_INSERT||key==VK_DELETE;
}

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

// 三个动作不可共用同一键；直接选择与原生键位的冲突由用户管理。
bool bindingsValid() {
    for(int key:draftKeys)if(!supportedKey(key))return false;
    return draftKeys[0]!=draftKeys[1]&&draftKeys[0]!=draftKeys[2]&&draftKeys[1]!=draftKeys[2];
}

// 按键只产生请求，是否允许切换由游戏线程核对当前任务状态。
bool directInputEnabled() {
    return runtimeEnabled.load()&&!panel&&managementError.empty()&&gameFocused()&&state.value("management_protocol",0)==2
        &&state.value("direct_selection_available",false);
}

// 捕获单次按键；错误显式提示，保持等待，不把无效输入换成其他键。
void captureKeyboardKey(int key) {
    if(key==VK_ESCAPE){captureSlot=-1;notice.clear();return;}
    if(!supportedKey(key)){notice=tr("此键不能绑定，请使用普通单键；管理快捷键和 Esc 为保留键。","Choose a regular single key. The management hotkey and Esc are reserved.");return;}
    for(int slot=0;slot<3;++slot)if(slot!=captureSlot&&draftKeys[slot]==key) {
        notice=tr("此键已绑定另一武器，请选择其他键。","This key is already assigned to another weapon.");return;
    }
    draftKeys[captureSlot]=key;captureSlot=-1;notice.clear();
}

// 管理页打开时加载保存值；未保存的草稿不改变游戏输入。
void loadManagementDraft() {
    draftLanguage=state.at("language").get<std::string>();
    centralHint=state.at("central_hint").get<bool>();
    const double ratio=state.at("ammo_ratio").get<double>(); // 拒绝不匹配的参数，不静默夹到有效范围。
    if(!std::isfinite(ratio)||ratio<.5||ratio>1||std::abs(ratio*20-std::round(ratio*20))>.00001)
        throw std::runtime_error("Ammo ratio must be 50% to 100% in 5% steps");
    draftRatioStep=static_cast<int>(std::lround(ratio*20))-10;
    const auto keys=state.value("keyboard_keys",Json::array()); // 控制器负责验证配置，界面也拒绝不匹配的协议。
    if(keys.size()!=3)throw std::runtime_error("Management keyboard binding data is invalid");
    for(int slot=0;slot<3;++slot)draftKeys[slot]=keys.at(slot).get<int>();
    if(!bindingsValid())throw std::runtime_error("Management keyboard bindings are invalid");
    saveRequest=std::max(saveRequest,state.value("settings_response",0));
    captureSlot=-1;logsLoaded=false;
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
    enqueue({{"type","settings"},{"management_protocol",2},{"request",pendingSave},{"language",draftLanguage},
        {"central_hint",centralHint},{"keyboard_keys",draftKeys},{"ammo_ratio",(10+draftRatioStep)/20.0}});
    notice=tr("正在保存…","Saving...");
}

// 只提供功能开关、参数、按键与日志；兵装选择统一留在原生页面。
void drawPanel() {
    const auto display=ImGui::GetIO().DisplaySize; // 面板随窗口尺寸缩放，固定居中。
    const float width=std::min(940.f,display.x-40),height=std::min(740.f,display.y-35);
    ImGui::SetNextWindowPos({display.x/2,display.y/2},ImGuiCond_Always,{.5f,.5f});ImGui::SetNextWindowSize({width,height});
    ImGui::Begin(tr("AC7DualSP 1.0.1 管理###DualSPManagement","AC7DualSP 1.0.1 Management###DualSPManagement"),nullptr,
        ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoSavedSettings);
    if(state.value("management_protocol",0)!=2||!managementError.empty()) {
        ImGui::TextWrapped("%s",tr("管理界面与控制器版本不匹配，请完整安装同一版本。","Management UI and controller versions do not match. Install a complete matching version."));
        if(!managementError.empty())ImGui::TextWrapped("%s",managementError.c_str());
        if(ImGui::Button(tr("关闭","Close")))togglePanel(false);
        ImGui::End();return;
    }
    // 语言切换只影响界面草稿，点击保存后持久化；稳定 ID 保留当前页和焦点。
    int language=draftLanguage=="auto"?0:draftLanguage=="zh"?1:2;
    ImGui::SetNextItemWidth(210);
    if(ImGui::Combo("语言选择/Select Language###Language",&language,"Game Language\0中文 / Chinese\0English\0"))
        draftLanguage=language==0?"auto":language==1?"zh":"en";
    ImGui::Separator();
    if(ImGui::BeginTabBar("ManagementTabs")) {
        if(ImGui::BeginTabItem(tr("功能与参数###Features","Features & Parameters###Features"))) {
            ImGui::Checkbox(tr("中央提示：常驻当前武器简称###Central","Persistent current weapon abbreviation###Central"),&centralHint);
            ImGui::TextWrapped("%s",tr("关闭中央常驻提示后，恢复游戏原本的短暂武器切换提示。","Disabling the persistent hint restores the game's temporary weapon switch message."));
            ImGui::TextWrapped("%s",tr("原游戏循环切换键继续可用。兵装选择只能通过原生出击或补给页面完成。","The original cycling key remains available. Choose loadouts through the game's departure or resupply page."));
            const auto ratioFormat=std::to_string(50+draftRatioStep*5)+"%%"; // 整数滑块只允许 11 档，显示真实百分比。
            ImGui::SliderInt(tr("双 SP 配弹比例###Ratio","Dual SP ammo ratio###Ratio"),&draftRatioStep,0,10,ratioFormat.c_str(),
                ImGuiSliderFlags_AlwaysClamp|ImGuiSliderFlags_NoInput);
            ImGui::TextWrapped("%s",tr("比例分别作用于两个 SP，默认各 50%；SP2 为空时 SP1 保留完整载弹量。当前任务弹药不因改设置而重算。","Each SP uses this ratio, 50% by default. With SP2 empty, SP1 retains its full stock. Current mission ammo is not recalculated."));
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem(tr("按键###Bindings","Bindings###Bindings"))) {
            ImGui::TextWrapped("%s",tr("点击键位后按下新键。仅支持单键，Esc 取消；三个键不能重复。","Click a binding, then press a new key. Single keys only; Esc cancels. Bindings must be distinct."));
            ImGui::TextWrapped("%s",tr("与原生键位冲突时，请自行调整游戏或模组键位。","If a binding conflicts with the game, adjust either mapping yourself."));
            if(ImGui::BeginTable("DirectBindings",3,ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn(tr("选择","Selection"));ImGui::TableSetupColumn(tr("键盘","Keyboard"));
                ImGui::TableSetupColumn(tr("手柄（固定）","Controller (fixed)"));ImGui::TableHeadersRow();
                const char* weapons[]={"MSL","SP1","SP2"}; // 固定逻辑槽，实际简称由任务 HUD 显示。
                const char* pads[]={tr("十字键左","D-pad Left"),tr("十字键上","D-pad Up"),tr("十字键右","D-pad Right")};
                for(int slot=0;slot<3;++slot) {
                    ImGui::PushID(slot);ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::TextUnformatted(weapons[slot]);
                    ImGui::TableNextColumn();
                    const auto label=captureSlot==slot?std::string(tr("请按键…","Press a key...")):keyLabel(draftKeys[slot]);
                    if(ImGui::Button(label.c_str(),{190,0})){captureSlot=slot;notice.clear();}
                    ImGui::TableNextColumn();ImGui::TextUnformatted(pads[slot]);ImGui::PopID();
                }
                ImGui::EndTable();
            }
            if(captureSlot>=0&&ImGui::Button(tr("取消改键","Cancel capture")))captureSlot=-1;
            if(ImGui::Button(tr("恢复默认 1 / 2 / 3","Reset to 1 / 2 / 3"))){draftKeys={49,50,51};captureSlot=-1;notice.clear();}
            ImGui::TextWrapped("%s",tr("装填中的武器可以选择；空 SP2 或耗尽的 SP 保持当前选择。按住不会重复切换。","Reloading weapons can be selected. An empty or depleted SP keeps the current selection. Holding a key does not repeat."));
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem(tr("日志###Logs","Logs###Logs"))) {
            if(!logsLoaded)refreshLogs();
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
        ImGui::EndTabBar();
    }
    ImGui::Separator();
    ImGui::BeginDisabled(captureSlot>=0||!bindingsValid()||pendingSave>0);
    if(ImGui::Button(tr("保存设置","Save settings"),{180,36}))saveManagementSettings();
    ImGui::EndDisabled();ImGui::SameLine();
    if(ImGui::Button(tr("关闭","Close"),{120,36}))togglePanel(false);
    ImGui::TextWrapped("%s",tr("开关和键位保存后生效；配弹比例用于下一次新开的原生选择页面。已有选择草稿和当前任务不重新配弹。关闭会丢弃未保存修改。","Saved switches and keys take effect after saving. Ammo ratio applies to the next newly opened native selection page. Existing drafts and current mission ammo stay unchanged. Closing discards unsaved changes."));
    ImGui::TextWrapped(tr("%s 或手柄 Back + Y 打开/关闭管理界面；任务中打开会暂停。","%s or controller Back + Y opens/closes management. Opening during a mission pauses the game."),keyLabel(managementKey).c_str());
    // 明确报告失败，不引导用户到已移除的 F8 兵装替代流程。
    const char* errorFields[]={"error","menu_error","hud_error","central_error","settings_error"};
    for(const char* field:errorFields)if(state.contains(field)) {
        ImGui::TextColored({1,.55f,.35f,1},"%s",tr("功能或保存错误（详见日志）","Feature or save error (see logs)"));
        ImGui::TextWrapped("%s",state.at(field).get<std::string>().c_str());
    }
    if(state.contains("input_notice")) {
        const auto reason=state.at("input_notice").get<std::string>(); // 拒绝直接选择的原因显示在管理页。
        ImGui::TextWrapped("%s",reason=="empty_sp2"?tr("SP2 未配置，保持当前武器。","SP2 is empty; selection unchanged."):
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
        const bool wasManagement=state.value("management_protocol",0)==2;
        {
            std::lock_guard<std::mutex> guard(publishedMutex);
            state=publishedState;consumedGeneration=publishedGeneration.load(std::memory_order_relaxed);
        }
        // 键位在交给窗口消息前核对，格式错误不能跨 Win32 回调抛出异常。
        if(state.value("management_protocol",0)==2) {
            const auto& hotkey=state.at("management_key"); // Lua/UI 成套部署；缺少新字段明确报告。
            if(!hotkey.is_number_integer())throw std::runtime_error("Management hotkey is not an integer");
            const int checkedManagementKey=hotkey.get<int>(); // 完整校验后才替换窗口消息使用的键。
            if(checkedManagementKey<VK_F1||checkedManagementKey>VK_F24)throw std::runtime_error("Management hotkey must be F1 through F24");
            const auto& keys=state.at("keyboard_keys");
            if(!keys.is_array()||keys.size()!=3)throw std::runtime_error("Management keyboard binding data is invalid");
            std::array<int,3> checkedKeys{};
            for(int slot=0;slot<3;++slot) {
                if(!keys.at(slot).is_number_integer())throw std::runtime_error("Management keyboard binding is not an integer");
                checkedKeys[slot]=keys.at(slot).get<int>();
                if(!supportedKey(checkedKeys[slot],checkedManagementKey))throw std::runtime_error("Management keyboard binding is unsupported");
            }
            if(checkedKeys[0]==checkedKeys[1]||checkedKeys[0]==checkedKeys[2]||checkedKeys[1]==checkedKeys[2])
                throw std::runtime_error("Management keyboard bindings must be distinct");
            state.at("direct_selection_available").get<bool>(); // 固定启用，但仍校验当前场景是否允许操作。
            managementKey=checkedManagementKey;
        }
        const int sequence=state.value("sequence",0); // 状态序号用于记录控制器最近更新。
        if(sequence!=lastSequence){lastSequence=sequence;lastControllerUpdate=GetTickCount64();}
        const int request=state.value("request_open",0),close=state.value("request_close",0);
        if(request!=lastRequest){lastRequest=request;if(request>0)togglePanel(true);}
        if(close!=lastClose){lastClose=close;if(close>0)togglePanel(false);}
        if(!wasManagement&&panel&&state.value("management_protocol",0)==2)loadManagementDraft();
        if(!panel)draftLanguage=state.value("language",std::string("auto"));
        if(pendingSave>0&&state.value("settings_response",0)==pendingSave) {
            notice=state.value("settings_ack",0)==pendingSave?tr("设置已保存。","Settings saved."):
                tr("保存失败，详见错误和日志。","Save failed; see the error and logs.");
            pendingSave=0;
        }
    }catch(const std::exception& error) {
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
                        const WORD keys[]={XINPUT_GAMEPAD_DPAD_LEFT,XINPUT_GAMEPAD_DPAD_UP,XINPUT_GAMEPAD_DPAD_RIGHT};
                        int desired=-1,count=0; // 同时按下多个方向不猜测目标。
                        for(int slot=0;slot<3;++slot)if(pressed&keys[slot]){desired=slot;++count;}
                        if(count==1)enqueue({{"type","select_weapon"},{"target",desired}});
                        else if(count>1)nativeLog("DIRECT_SELECTION_AMBIGUOUS_PAD: 多个方向同时按下，未切换");
                    }
                    previousPadButtons=buttons;padWasConnected=connected;
                }
                // Flight HUD is native UMG. With the menu closed there is no
                // ImGui frame, backend update or render-target state change.
                if(panel) {
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
                try {if(panel)drawPanel();}catch(const Json::exception& e){notice=e.what();nativeLog(std::string("UI_ERROR ")+e.what());}
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

extern "C" __declspec(dllexport) int dualsp_management_supported(void*) {return 0;}
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

// DX11 overlay: the render thread only handles UI and a command queue. All game
// objects and weapon mutations stay in the UE4SS game-thread controller.
#include "common.hpp"
#include <d3d11.h>
#include <Xinput.h>
#include <atomic>
#include <chrono>
#include <vector>
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "MinHook.h"
#include "panel_pointer.hpp"
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
int draftA=0,draftB=-1,lastPlane=-1,lastRequest=-1,lastClose=-1;
float draftRatio=.5f;bool showHud=true,ignoreUnlocks=false,nativeLoadoutEnabled=true;float hudScale=1;
std::string notice;
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
    enqueue({{"type",open?"open":"close"}});
    if(open) {
        const auto p=state.value("profile",Json::object());
        draftA=p.value("a",0);draftB=p.value("b",-1);draftRatio=p.value("ratio",.5f);
        showHud=state.value("hud",true);hudScale=state.value("hud_scale",1.0f);ignoreUnlocks=state.value("ignore_unlocks",false);
        nativeLoadoutEnabled=state.value("native_loadout_enabled",true);
        notice.clear();
    }
}
LRESULT CALLBACK windowProc(HWND hwnd,UINT msg,WPARAM w,LPARAM l) {
    std::lock_guard<std::recursive_mutex> guard(renderMutex);
    if(imguiContext)ImGui::SetCurrentContext(imguiContext);
    if(msg==WM_KEYDOWN && !(l&(1LL<<30))) {
        if(w==VK_F8){togglePanel(!panel);return 0;}
        if(w==VK_F9 && !panel){enqueue({{"type","cycle"}});return 0;}
        if(w==VK_ESCAPE && panel){togglePanel(false);return 0;}
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
std::string codeLabel(const Json& weapon) {
    auto code=weapon.value("code",std::string("?"));
    for(auto& c:code)c=static_cast<char>(toupper(static_cast<unsigned char>(c)));
    return code;
}
void refreshState() {
    static uint64_t consumedGeneration{};
    if(publishedGeneration.load(std::memory_order_acquire)==consumedGeneration)return;
    try {
        {std::lock_guard<std::mutex> guard(publishedMutex);
            state=publishedState;consumedGeneration=publishedGeneration.load();}
        const int sequence=state.value("sequence",0);
        if(sequence!=lastSequence){lastSequence=sequence;lastControllerUpdate=GetTickCount64();}
        const int request=state.value("request_open",0);
        if(request!=lastRequest){lastRequest=request;if(request>0)togglePanel(true);}
        const int close=state.value("request_close",0);
        if(close!=lastClose){lastClose=close;if(close>0)togglePanel(false);}
        const int plane=state.value("plane_id",0);
        if(plane!=lastPlane) {
            lastPlane=plane;const auto p=state.value("profile",Json::object());
            draftA=p.value("a",0);draftB=p.value("b",-1);draftRatio=p.value("ratio",.5f);
        }
    }catch(const Json::exception& e){notice=std::string("界面数据错误：")+e.what();}
}
void weaponIcon(ImDrawList* draw,ImVec2 center,const std::string& code,float scale,ImU32 color) {
    const bool bomb=code.find("GB")!=std::string::npos || code.find("FF")!=std::string::npos || code.find("DB")!=std::string::npos;
    if(bomb) {draw->AddEllipse(center,{10*scale,25*scale},color,0,24,1.7f);draw->AddLine({center.x-12*scale,center.y-18*scale},{center.x+12*scale,center.y-18*scale},color,2);}
    else if(code=="EML"||code=="TLS"||code=="PLSL") {
        draw->AddRect({center.x-17*scale,center.y-8*scale},{center.x+17*scale,center.y+8*scale},color,2.0f,2.0f,ImDrawFlags_None);
        for(int k=-1;k<=1;++k)draw->AddLine({center.x+20*scale,center.y+k*6*scale},{center.x+39*scale,center.y+k*6*scale},color,1.5f);
    }else {
        draw->AddLine({center.x,center.y-28*scale},{center.x,center.y+24*scale},color,3*scale);
        draw->AddTriangle({center.x,center.y-30*scale},{center.x-6*scale,center.y-15*scale},{center.x+6*scale,center.y-15*scale},color,1.5f);
        draw->AddLine({center.x-14*scale,center.y+15*scale},{center.x+14*scale,center.y+15*scale},color,2*scale);
    }
}
void chooseSlot(int slot,const Json& weapons,bool enabled,float width) {
    ImGui::BeginChild(slot==0?"SlotA":"SlotB",{width,190},ImGuiChildFlags_Borders);
    ImGui::TextColored({.37f,.89f,.83f,1},"%s",slot==0?"SP 1 / 第一特殊兵装":"SP 2 / 第二特殊兵装");
    int& selected=slot==0?draftA:draftB;
    std::string title=selected<0?"不搭载 / 单 SP":codeLabel(weapons.at(selected));
    ImGui::BeginDisabled(!enabled);
    if(ImGui::BeginCombo(slot==0?"##weaponA":"##weaponB",title.c_str())) {
        if(slot==1 && ImGui::Selectable("不搭载 / 单 SP",selected<0))selected=-1;
        for(size_t i=0;i<weapons.size();++i) {
            const auto& item=weapons[i];bool owned=item.value("owned",false);
            const bool other=(slot==0?draftB:draftA)==static_cast<int>(i);
            const bool compatible=item.value("available",true)||(slot==0&&draftB<0);
            auto label=codeLabel(item)+(other?"  · 已用于另一槽位":(!owned?"  · 未购买":(!compatible?"  · 暂不支持双挂载":"")));
            ImGui::BeginDisabled(!owned||other||!compatible);
            if(ImGui::Selectable(label.c_str(),selected==static_cast<int>(i)))selected=static_cast<int>(i);
            ImGui::EndDisabled();
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    if(selected>=0) {
        const auto& item=weapons.at(selected);const int stock=item.value("max",0);
        const int loaded=draftB<0?stock:std::max(1,static_cast<int>(stock*draftRatio));
        ImGui::Text("%s",item.value("description",std::string()).c_str());
        ImGui::Text("携带 %d   /   单挂载 %d",loaded,stock);
        weaponIcon(ImGui::GetWindowDrawList(),{ImGui::GetWindowPos().x+width-55,ImGui::GetWindowPos().y+135},codeLabel(item),1,IM_COL32(100,215,201,255));
    }else ImGui::TextWrapped("只使用第一种 SP，保留原始完整载弹量。");
    ImGui::EndChild();
}
void drawPanel() {
    const auto display=ImGui::GetIO().DisplaySize;
    const float width=std::min(920.f,display.x-40),height=std::min(690.f,display.y-35);
    ImGui::SetNextWindowPos({display.x/2,display.y/2},ImGuiCond_Always,{.5f,.5f});ImGui::SetNextWindowSize({width,height});
    ImGui::Begin("DUAL SP / 兵装配置",nullptr,ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoSavedSettings);
    if(ImGui::BeginTabBar("DualSPTabs")) {
        if(ImGui::BeginTabItem("兵装选择")) {
            ImGui::TextColored({.37f,.89f,.83f,1},"%s",state.value("plane_name",std::string("正在读取机体")).c_str());
            ImGui::SameLine();ImGui::BeginDisabled(!state.value("allow_edit",false)||draftB<0);
            if(ImGui::SmallButton("交换两个槽位"))std::swap(draftA,draftB);ImGui::EndDisabled();
            ImGui::TextDisabled("普通导弹始终保留。为两个槽位选择不同的特殊兵装。");
            const auto weapons=state.value("weapons",Json::array());
            const bool editable=state.value("allow_edit",false)&&!state.contains("error");
            if(weapons.size()==3) {
                draftA=std::clamp(draftA,0,2);draftB=std::clamp(draftB,-1,2);
                const float col=(ImGui::GetContentRegionAvail().x-14)/2;
                chooseSlot(0,weapons,editable,col);ImGui::SameLine();chooseSlot(1,weapons,editable,col);
                ImGui::Separator();
                ImGui::BeginDisabled(!editable||draftB<0);
                float percent=draftRatio*100.f;
                if(ImGui::SliderFloat("双挂载配弹比例",&percent,10.f,100.f,"%.0f%%",ImGuiSliderFlags_AlwaysClamp))draftRatio=percent/100.f;
                ImGui::EndDisabled();
                ImGui::TextDisabled("默认 50%% 为本模组平衡规则；AC8 的精确配弹比例尚未确认。");
                const auto stats=weapons.at(draftA).value("stats",Json::array());
                const char* labels[]={"对空","对地","制导","装填","射程","范围"};
                if(stats.size()==6)for(int k=0;k<6;++k){ImGui::Text("%s",labels[k]);ImGui::SameLine(90);ImGui::ProgressBar(std::clamp(stats[k].get<float>()/100.f,0.f,1.f),{width-160,5},"");}
                if(!editable)ImGui::TextWrapped("本次出击的兵装已经锁定。回到机库后可重新选择。");
                const bool valid=weapons.at(draftA).value("owned",false)&&(draftB<0||(draftB!=draftA&&weapons.at(draftB).value("owned",false)));
                ImGui::BeginDisabled(!editable||!valid);
                if(ImGui::Button("确认装备 / 应用",{230,42})) {
                    enqueue({{"type","apply"},{"plane_id",state.value("plane_id",0)},{"a",draftA},{"b",draftB},{"ratio",draftRatio}});
                    notice="正在应用兵装…";
                }
                ImGui::EndDisabled();ImGui::SameLine();if(ImGui::Button("取消 / 返回",{170,42}))togglePanel(false);
            } else ImGui::TextWrapped("当前机体数据尚未就绪，请在机库选择机体后打开此界面。");
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("界面设置")) {
            ImGui::Checkbox("使用原生出击 SP 选择界面",&nativeLoadoutEnabled);
            ImGui::TextDisabled("原兵装页顺序选择 SP1 / SP2；F8 保留为备用和调试入口。");
            ImGui::Checkbox("启用原生双 SP 兵装列表",&showHud);
            ImGui::TextDisabled("沿用游戏原生字体、颜色和位置；关闭后恢复单 SP 列表。");
            ImGui::Checkbox("允许选择未购买兵装（试玩模式）",&ignoreUnlocks);ImGui::TextDisabled("不改变原存档中的购买记录。");
            if(ImGui::Button("保存界面设置",{230,40})){enqueue({{"type","settings"},{"hud",showHud},{"hud_scale",hudScale},{"ignore_unlocks",ignoreUnlocks},{"native_loadout",nativeLoadoutEnabled}});notice="界面设置已保存";}
            ImGui::Separator();ImGui::TextWrapped("F8：打开 / 关闭配置。F9：额外的武器循环键。原游戏武器切换键仍可使用。手柄使用原切换键；Back + Y 可打开配置，十字键、A、B 可操作菜单。");
            ImGui::TextWrapped("配置按机体保存。任务中打开菜单会暂停游戏；取消会恢复之前的暂停状态。双 SP 的弹药与装填进度独立保存。");
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    if(state.contains("error")){ImGui::TextColored({1,.43f,.35f,1},"%s",state["error"].get<std::string>().c_str());ImGui::TextWrapped("请退出任务，保留 mod.log 和 native.log 供排查。");}
    if(state.contains("hud_error")){ImGui::TextColored({1,.7f,.35f,1},"原生双 SP 列表接入失败，兵装功能继续运行。");ImGui::TextWrapped("%s",state["hud_error"].get<std::string>().c_str());}
    if(state.contains("menu_error")){ImGui::TextColored({1,.7f,.35f,1},"原生出击选择接入失败，可继续在 F8 中配置兵装。");ImGui::TextWrapped("%s",state["menu_error"].get<std::string>().c_str());}
    if(!notice.empty())ImGui::TextWrapped("%s",notice.c_str());
    ImGui::End();
}
HRESULT __stdcall onPresent(IDXGISwapChain* swap,UINT interval,UINT flags) {
    {
        std::lock_guard<std::recursive_mutex> guard(renderMutex);
        if(!initialized)init(swap);
        if(initialized&&swap==mainSwap) {
            ImGui::SetCurrentContext(imguiContext);if(!target)makeTarget(swap);
            if(target) {
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
                static bool chordWas=false;static ULONGLONG lastPadPoll{};
                const auto padNow=GetTickCount64();
                if(padNow-lastPadPoll>=100) {
                    lastPadPoll=padNow;XINPUT_STATE pad{};
                    const bool chord=foreground&&XInputGetState(0,&pad)==ERROR_SUCCESS&&(pad.Gamepad.wButtons&(XINPUT_GAMEPAD_BACK|XINPUT_GAMEPAD_Y))==(XINPUT_GAMEPAD_BACK|XINPUT_GAMEPAD_Y);
                    if(chord&&!chordWas)togglePanel(!panel);chordWas=chord;
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

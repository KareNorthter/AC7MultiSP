// Install hooks in a standalone test process with a hidden dummy DX11 window.
// Never open AC7, simulate input, or request ownership of the desktop pointer.
#include "../src/overlay.cpp"
#include <cassert>
#include <iostream>

int main() {
    assert(!cursorOwned.load());
    startThread(nullptr);
    assert(originalSetCursor && originalSetCursorPos && originalClipCursor && originalPresent && originalResize);
    std::ifstream log(modRoot()/"native.log");
    std::string line,last;
    while(std::getline(log,line))last=line;
    assert(last.find("Overlay and panel cursor hooks installed")!=std::string::npos);
    assert(!cursorOwned.load() && !initialized);
    const auto actionFile=modRoot()/"ui-actions.json";
    assert(!std::filesystem::exists(actionFile));
    for(int i=0;i<500;++i)dualsp_drain_actions(nullptr);
    assert(!std::filesystem::exists(actionFile));
    enqueue({{"type","cycle"}});dualsp_drain_actions(nullptr);
    {std::ifstream input(actionFile);Json batch;input>>batch;assert(batch.size()==1&&batch[0]["type"]=="cycle");}
    std::filesystem::remove(actionFile);dualsp_drain_actions(nullptr);
    assert(!std::filesystem::exists(actionFile));
    const auto pending=modRoot()/"ui-state.pending.json",diagnostic=modRoot()/"ui-state.json";
    assert(!std::filesystem::exists(pending)&&!std::filesystem::exists(diagnostic));
    {std::ofstream out(pending);out<<Json{{"sequence",91},{"plane_id",0}};}
    dualsp_publish(nullptr);refreshState();
    assert(state["sequence"]==91&&publishedGeneration.load()==1);
    // A renderer refresh uses the memory snapshot even with no diagnostic file.
    std::filesystem::remove(diagnostic);refreshState();assert(state["sequence"]==91);
    // All hooks here belong to this short-lived test executable.
    assert(MH_DisableHook(MH_ALL_HOOKS)==MH_OK);
    assert(MH_Uninitialize()==MH_OK);
    std::cout<<"overlay_hooks: DX11 and 3 Win32 hooks installed; 500 idle drains wrote no files; memory UI handoff passed; no game or pointer input operated\n";
}

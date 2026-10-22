#include "../src/panel_pointer.hpp"
#include "../src/keyboard_shortcuts.hpp"
#include "../src/version_updates.hpp"
#include "../src/ui_messages.hpp"
#include <cassert>
#include <iostream>

int main() {
    // 使用离线清单验证稳定版/alpha比较、重复字段和链接边界，不发起网络或浏览器操作。
    using namespace dualsp::updates;
    assert(newerThanCurrent("2.24.0","2.24.0 alpha1"));
    assert(!newerThanCurrent("2.23.1","2.24.0 alpha1"));
    assert(!newerThanCurrent("2.24.0","2.24.0"));
    assert(newerThanCurrent("2.25.0","2.24.0"));
    const std::string manifest=R"({"schema":1,"product":"AC7MultiSP","channel":"stable","status":"ready","latest_version":"9.0.0","release_notes":{"zh":"离线样本","en":"Offline fixture"},"downloads":[{"id":"r2","label":{"zh":"下载","en":"Download"},"url":"https://downloads.iot-node-service-api.online/offline.zip"}]})";
    assert(parse(manifest).status==Status::Newer && parse(manifest).downloads.size()==1);
    const auto legacy=R"({"schema_version":1,"product":"AC7MultiSP","channel":"stable","status":"preparing","latest_version":"2.21.1","package":null})";
    assert(parse(legacy).status==Status::Unpublished && parse(legacy).latest.empty() && parse(legacy).downloads.empty());
    assert(parse(R"({"schema":1,"product":"AC7MultiSP","channel":"stable","status":"preparing"})").status==Status::Unpublished);
    Service idle;assert(!idle.requestCheck() && !idle.checking() && idle.status()==Status::Waiting);
    assert(dualsp::noticeText("提示")=="ACMultiSP: 提示");
    assert(dualsp::noticeText("ACMultiSP: 提示")=="ACMultiSP: 提示");
    assert(dualsp::noticeText("AC7MultiSP: ACMultiSP: 提示")=="ACMultiSP: 提示");
    const auto rejects=[](auto callback){try {callback();return false;}catch(const std::exception&) {return true;}};
    assert(rejects([&]{parse("{\"schema\":1,"+manifest.substr(1));}));
    assert(rejects([&]{checkedUrl("file:///C:/Windows/notepad.exe");}));
    assert(rejects([&]{checkedUrl("https://unregistered.invalid/offline.zip");}));
    assert(rejects([&]{newerThanCurrent("2.24.0 alpha2","2.24.0");}));
    assert(rejects([&]{parse(R"({"schema_version":1,"product":"AC7MultiSP","channel":"stable","status":"ready","package":null})");}));
    // 四个动作共用组合优先规则；新功能键可清除，不能遮挡既有键位。
    std::array<dualsp::Shortcut,4> chords{{{117,0},{117,3},{118,3},{116,3}}};
    const std::array<int,4> direct{49,50,51,52};
    assert(dualsp::validShortcuts(chords,direct));
    for(int mods=0;mods<8;++mods) {
        assert(dualsp::shortcutMatches(chords[0],117,mods)==(mods==0));
        assert(dualsp::shortcutMatches(chords[1],117,mods)==(mods==3));
        assert(dualsp::validShortcut({120,mods}));
    }
    chords[2]=chords[1];assert(!dualsp::validShortcuts(chords,direct));
    chords[2]={49,0};assert(!dualsp::validShortcuts(chords,direct));
    chords[2]={49,4};assert(dualsp::validShortcuts(chords,direct));
    assert(!dualsp::validShortcut({117,8})&&!dualsp::validShortcut({27,0}));
    assert(!dualsp::validShortcut({115,2})&&!dualsp::validShortcut({46,3}));
    for(int mods=0;mods<8;++mods)assert(dualsp::singleKeyMatches(49,49,mods));
    assert(dualsp::matchingShortcut(chords,117,3)==1);
    assert(dualsp::matchingShortcut(chords,117,4)==0);
    assert(dualsp::matchingShortcut(chords,116,3)==3);
    assert(!dualsp::singleKeyMatches(115,115,2));
    chords[3]={0,0};assert(dualsp::validShortcuts(chords,direct));
    dualsp::PanelPointer p;
    p.center(1920,1080);assert(p.x==960 && p.y==540);
    p.relative(120,-40);assert(p.x==1080 && p.y==500);
    // A physical OS cursor repeatedly centered by another component is not an
    // input to this class; consecutive real raw movements continue accumulating.
    p.relative(30,20);assert(p.x==1110 && p.y==520);
    p.relative(2000,-2000);assert(p.x==1919 && p.y==0);
    p.absolute(-100,1500);assert(p.x==0 && p.y==1079);
    p.bounds(800,600);assert(p.x==0 && p.y==599);
    p.relative(500,0);assert(p.x==500 && p.y==599);
    p.center(0,0);assert(p.x==0 && p.y==0);
    std::cout<<"panel_pointer: relative motion, bounds, absolute input and resize passed\n";
}

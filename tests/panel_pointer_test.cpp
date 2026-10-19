#include "../src/panel_pointer.hpp"
#include "../src/keyboard_shortcuts.hpp"
#include <cassert>
#include <iostream>

int main() {
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

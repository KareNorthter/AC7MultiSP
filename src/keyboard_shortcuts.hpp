// 快捷键只使用一个主键，修饰键位掩码：Ctrl=1、Alt=2、Shift=4。
#pragma once
#include <array>
#include <cstddef>
namespace dualsp {
struct Shortcut { int key{},mods{}; };
inline bool supportedShortcutKey(int key) {
    return (key>=48&&key<=57)||(key>=65&&key<=90)||(key>=96&&key<=135)
        ||(key>=186&&key<=192)||(key>=219&&key<=222)||key==8||key==9||key==13||key==32
        ||(key>=33&&key<=40)||key==45||key==46;
}
inline bool validShortcut(Shortcut value) {
    return supportedShortcutKey(value.key)&&value.mods>=0&&value.mods<=7
        &&!(value.mods==3&&value.key==46)&&!(value.mods==2&&value.key==115);
}
inline bool sameShortcut(Shortcut a,Shortcut b) { return a.key==b.key&&a.mods==b.mods; }
inline bool shortcutMatches(Shortcut value,int key,int mods) { return value.key==key&&value.mods==mods; }
inline bool validShortcuts(const std::array<Shortcut,3>& values,const std::array<int,4>& direct) {
    for(std::size_t i=0;i<values.size();++i) {
        if(!validShortcut(values[i]))return false;
        for(std::size_t j=0;j<i;++j)if(sameShortcut(values[i],values[j]))return false;
        for(int key:direct)if(values[i].mods==0&&values[i].key==key)return false;
    }
    for(std::size_t i=0;i<direct.size();++i) {
        if(!supportedShortcutKey(direct[i]))return false;
        for(std::size_t j=0;j<i;++j)if(direct[i]==direct[j])return false;
    }
    return true;
}
inline bool validTargetGazeKey(int key,const std::array<Shortcut,3>& shortcuts,const std::array<int,4>& direct) {
    if(key==0)return true; // 零明确表示未绑定，不替用户占用原生按键。
    if(!supportedShortcutKey(key))return false;
    for(const auto& chord:shortcuts)if(chord.mods==0 && chord.key==key)return false;
    for(int value:direct)if(value==key)return false;
    return true;
}
}

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
// 单键允许同时按住飞行用的修饰键；系统组合不回落到模组单键动作。
inline bool singleKeyMatches(int binding,int key,int mods) {
    const bool system=((mods&2)!=0&&(key==9||key==27||key==32||key==115))
        ||((mods&1)!=0&&key==27)||((mods&3)==3&&key==46);
    return binding!=0&&binding==key&&!system;
}
inline int matchingShortcut(const std::array<Shortcut,4>& values,int key,int mods) {
    // 先匹配明确设置的组合，再回落到单键，不能受快捷键的保存顺序影响。
    for(std::size_t i=0;i<values.size();++i)
        if(values[i].mods!=0&&shortcutMatches(values[i],key,mods))return static_cast<int>(i);
    for(std::size_t i=0;i<values.size();++i)
        if(values[i].mods==0&&singleKeyMatches(values[i].key,key,mods))return static_cast<int>(i);
    return -1;
}
inline bool validShortcuts(const std::array<Shortcut,4>& values,const std::array<int,4>& direct) {
    for(std::size_t i=0;i<values.size();++i) {
        if(i==3 && values[i].key==0 && values[i].mods==0)continue; // 新键冲突时允许未绑定。
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
inline bool validTargetGazeKey(int key,const std::array<Shortcut,4>& shortcuts,const std::array<int,4>& direct) {
    if(key==0)return true; // 零明确表示未绑定，不替用户占用原生按键。
    if(!supportedShortcutKey(key))return false;
    for(const auto& chord:shortcuts)if(chord.mods==0 && chord.key==key)return false;
    for(int value:direct)if(value==key)return false;
    return true;
}
inline bool validCameraCenterKey(int key,bool enabled,int gazeKey,const std::array<Shortcut,4>& shortcuts,const std::array<int,4>& direct) {
    if(key!=0 && key!=4 && !supportedShortcutKey(key))return false; // 4为鼠标中键，仅回正功能支持。
    if(!enabled || key==0)return true;
    return key!=gazeKey && (key==4 || validTargetGazeKey(key,shortcuts,direct));
}
}

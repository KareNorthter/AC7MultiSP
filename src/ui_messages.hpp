#pragma once
#include <string>
#include <utility>
#include "generated/user_texts.hpp"

// 管理与通知的中英资源；先替换完整长句，再处理组合状态的前后缀。
// 原生日志保留原文，不能以显示翻译改写错误证据。
namespace dualsp {
inline std::string noticeBody(std::string message) {
    for(;;) {
        if(message.rfind("AC7MultiSP: ",0)==0)message.erase(0,12);
        else if(message.rfind("ACMultiSP: ",0)==0)message.erase(0,11);
        else return message;
    }
}
inline std::string noticeText(const std::string& message) {return "ACMultiSP: "+noticeBody(message);}
inline constexpr auto& uiMessages=user_texts::messageReplacements;
inline bool containsHan(const std::string& text) {
    for(size_t i=0;i+2<text.size();++i) {
        const auto a=static_cast<unsigned char>(text[i]),b=static_cast<unsigned char>(text[i+1]),c=static_cast<unsigned char>(text[i+2]);
        if((a&0xf0)==0xe0 && (b&0xc0)==0x80 && (c&0xc0)==0x80) {
            const unsigned code=((a&15)<<12)|((b&63)<<6)|(c&63);
            if((code>=0x3400 && code<=0x9fff)||(code>=0xf900 && code<=0xfaff))return true;
        }
    }
    return false;
}
inline std::string localizedUiMessage(std::string text,bool chinese,const char* fallback) {
    // 面板和通知都去掉Lua文件位置与堆栈；完整原文仍在日志页。
    const auto newline=text.find_first_of("\r\n");if(newline!=std::string::npos)text.resize(newline);
    const auto source=text.find(".lua:");
    if(source!=std::string::npos) {
        size_t end=source+5;while(end<text.size() && text[end]>='0' && text[end]<='9')++end;
        if(end<text.size() && text[end]==':') {++end;while(end<text.size() && text[end]==' ')++end;text.erase(0,end);}
    }
    if(text.empty())return text;
    for(const auto& entry:uiMessages) {
        // 原始异常只充当稳定匹配键，两种显示语言都取catalog；改中文显示值不会破坏匹配。
        if(text==entry.sourceZh || text==entry.sourceEn || text==entry.zh || text==entry.en)return chinese?entry.zh:entry.en;
    }
    for(const auto& entry:uiMessages) {
        const std::string from=entry.sourceZh,to=chinese?entry.zh:entry.en;
        // 英文回译仅限整句，避免改写游戏/Mod机体名称；中文片段允许更新为catalog的新值。
        if(chinese && from==to)continue;
        size_t at=0;while((at=text.find(from,at))!=std::string::npos){text.replace(at,from.size(),to);at+=to.size();}
        if(!chinese && from!=entry.zh) {
            const std::string current=entry.zh;at=0;
            while((at=text.find(current,at))!=std::string::npos){text.replace(at,current.size(),to);at+=to.size();}
        }
    }
    // 未登记的底层异常使用对应功能的英文摘要；不在英文页泄漏中文堆栈。
    if(!chinese && containsHan(text))return fallback;
    return text;
}
}

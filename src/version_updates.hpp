#pragma once
#include "common.hpp"
#include <winhttp.h>
#include <shellapi.h>
#include <objbase.h>
#include <algorithm>
#include <array>
#include <cwctype>
#include <memory>
#include <set>
#include <vector>

#ifndef AC7MULTISP_VERSION
#define AC7MULTISP_VERSION "2.24.0 alpha2"
#endif

namespace dualsp::updates {
// 与独立下载安装器无关：只取稳定版提示清单，用户点击后交给浏览器。
inline constexpr char currentVersion[]=AC7MULTISP_VERSION;
inline constexpr wchar_t manifestHost[]=L"ac7multisp-updates.pages.dev";
inline constexpr wchar_t manifestPath[]=L"/stable.json";
inline constexpr size_t manifestLimit=32768;
enum class Status {Waiting,Checking,Current,Newer,Unpublished,Failed};
enum class BrowserStatus {Idle,Opening,Opened,Failed};
struct Download {std::string id,zh,en,url;};
struct Snapshot {
    Status status{Status::Waiting};
    std::string latest,notesZh,notesEn;
    std::vector<Download> downloads;
};
inline void ensure(bool accepted,const char* message) {if(!accepted)throw std::runtime_error(message);}
inline std::wstring wide(const std::string& text) {
    const int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0);
    ensure(count>0,"更新数据不是有效UTF-8");
    std::wstring value(count,L'\0');
    ensure(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),value.data(),count)==count,"更新数据转换失败");
    return value;
}
inline std::array<unsigned,3> version(const std::string& text) {
    ensure(!text.empty() && text.size()<=14,"更新版本号无效");
    std::array<unsigned,3> parts{};size_t offset=0;
    for(unsigned i=0;i<parts.size();++i) {
        const size_t begin=offset;
        while(offset<text.size() && text[offset]>='0' && text[offset]<='9') {
            parts[i]=parts[i]*10+unsigned(text[offset++]-'0');
            ensure(parts[i]<=9999,"更新版本号分段过大");
        }
        ensure(offset>begin && (offset-begin==1 || text[begin]!='0'),"更新版本号格式无效");
        if(i<2){ensure(offset<text.size() && text[offset]=='.',"更新版本号缺少分段");++offset;}
    }
    ensure(offset==text.size(),"稳定版不接受测试版本标识");return parts;
}
inline bool newerThanCurrent(const std::string& latest,const std::string& current=currentVersion) {
    const auto split=current.find(" alpha");
    const auto base=split==std::string::npos?current:current.substr(0,split);
    if(split!=std::string::npos) {
        const auto number=current.substr(split+6);
        ensure(!number.empty() && number.front()>='1' && number.front()<='9' &&
            std::all_of(number.begin(),number.end(),[](char ch){return ch>='0' && ch<='9';}),"当前测试版本标识无效");
    }
    const auto target=version(latest),local=version(base);
    return target>local || (split!=std::string::npos && target==local); // 同号正式版晚于alpha。
}
inline std::string text(const Json& value,size_t limit,bool multiline=false) {
    ensure(value.is_string(),"更新清单文字字段无效");
    const auto result=value.get<std::string>();
    ensure(result.size()<=limit,"更新清单文字超长");
    for(unsigned char ch:result)ensure(ch>=32 || (multiline && (ch=='\n' || ch=='\r' || ch=='\t')),"更新清单含控制字符");
    if(!result.empty())wide(result);return result;
}
inline std::wstring checkedUrl(const std::string& url) {
    ensure(!url.empty() && url.size()<=2048,"下载链接长度无效");
    // 不经过命令解释器；另外拒绝空白、引号、反斜杠和控制字符。
    for(unsigned char ch:url)ensure(ch>32 && ch<127 && ch!='"' && ch!='\'' && ch!='\\',"下载链接含无效字符");
    const auto value=wide(url);
    URL_COMPONENTS parts{};parts.dwStructSize=sizeof(parts);
    parts.dwSchemeLength=parts.dwHostNameLength=parts.dwUserNameLength=parts.dwPasswordLength=static_cast<DWORD>(-1);
    ensure(WinHttpCrackUrl(value.c_str(),static_cast<DWORD>(value.size()),0,&parts)!=FALSE,"下载链接格式无效");
    ensure(parts.nScheme==INTERNET_SCHEME_HTTPS && parts.nPort==443 && !parts.dwUserNameLength && !parts.dwPasswordLength,"下载链接不是公开HTTPS地址");
    std::wstring host(parts.lpszHostName,parts.dwHostNameLength);
    std::transform(host.begin(),host.end(),host.begin(),[](wchar_t ch){return std::towlower(ch);});
    // 只放行作者已配置的发布域名；新增来源须在源码中明确接收。
    ensure(host==L"wwbru.lanzoue.com" || host==L"downloads.iot-node-service-api.online" ||
        host==manifestHost,"下载链接域名未登记");
    return value;
}
inline Snapshot parse(const std::string& body) {
    std::vector<std::set<std::string>> keys;
    const auto data=Json::parse(body,[&](int depth,Json::parse_event_t event,Json& value) {
        ensure(depth<=10,"更新清单嵌套过深");
        if(event==Json::parse_event_t::object_start)keys.emplace_back();
        else if(event==Json::parse_event_t::key)ensure(!keys.empty() && keys.back().insert(value.get<std::string>()).second,"更新清单字段重复");
        else if(event==Json::parse_event_t::object_end){ensure(!keys.empty(),"更新清单对象无效");keys.pop_back();}
        return true;
    });
    ensure(data.is_object() && data.value("product",std::string())=="AC7MultiSP" &&
        data.value("channel",std::string())=="stable","更新清单产品或渠道不匹配");
    const bool currentSchema=data.contains("schema") && data.at("schema").is_number_integer() && data.at("schema")==1;
    const bool legacyPreparing=!data.contains("schema") && data.contains("schema_version") &&
        data.at("schema_version").is_number_integer() && data.at("schema_version")==1 &&
        data.contains("package") && data.at("package").is_null();
    if(data.value("status",std::string())=="preparing" && (currentSchema || legacyPreparing)) {
        Snapshot result;result.status=Status::Unpublished;return result; // 不把旧清单的版本和下载入口当作可用更新。
    }
    ensure(currentSchema,"更新清单格式不匹配：需要schema=1");
    ensure(data.value("status",std::string())=="ready","更新清单状态不受支持");
    Snapshot result;result.latest=text(data.at("latest_version"),14);
    result.status=newerThanCurrent(result.latest)?Status::Newer:Status::Current;
    const auto& notes=data.at("release_notes");
    ensure(notes.is_object(),"更新内容格式无效");
    result.notesZh=text(notes.at("zh"),8192,true);result.notesEn=text(notes.at("en"),8192,true);
    ensure(!result.notesZh.empty() && !result.notesEn.empty(),"更新内容缺少中文或英文");
    const auto& downloads=data.at("downloads");
    ensure(downloads.is_array() && !downloads.empty() && downloads.size()<=4,"更新下载入口数量无效");
    std::set<std::string> ids;
    for(const auto& entry:downloads) {
        Download link;link.id=text(entry.at("id"),32);link.url=text(entry.at("url"),2048);
        ensure(!link.id.empty() && ids.insert(link.id).second,"更新下载入口重复或缺少标识");
        for(unsigned char ch:link.id)ensure((ch>='a' && ch<='z') || (ch>='0' && ch<='9') || ch=='_' || ch=='-',"下载入口标识无效");
        link.zh=text(entry.at("label").at("zh"),96);link.en=text(entry.at("label").at("en"),96);
        ensure(!link.zh.empty() && !link.en.empty(),"下载入口名称为空");
        checkedUrl(link.url);result.downloads.push_back(std::move(link));
    }
    return result;
}
struct HttpHandle {
    HINTERNET value{};
    explicit HttpHandle(HINTERNET handle):value(handle){ensure(value!=nullptr,"后台更新连接创建失败");}
    HttpHandle(const HttpHandle&)=delete;
    ~HttpHandle(){WinHttpCloseHandle(value);}
};
inline void httpCheck(BOOL accepted,const char* message) {
    if(!accepted) {
        const auto code=GetLastError();
        throw std::runtime_error(std::string(message)+" (WinHTTP "+std::to_string(code)+")");
    }
}
inline std::string fetch() {
    // 使用WinHTTP已配置的静态代理；不执行PAC/WPAD发现、不读取浏览器Cookie。
    const auto agent=wide(std::string("AC7MultiSP/")+currentVersion+" version-check");
    HttpHandle session(WinHttpOpen(agent.c_str(),WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
    ensure(WinHttpSetTimeouts(session.value,2000,2000,2000,2000)!=FALSE,"更新超时设置失败");
    DWORD connectionAttempts=1;
    ensure(WinHttpSetOption(session.value,WINHTTP_OPTION_CONNECT_RETRIES,&connectionAttempts,sizeof(connectionAttempts))!=FALSE,"更新单次连接设置失败");
    HttpHandle connection(WinHttpConnect(session.value,manifestHost,INTERNET_DEFAULT_HTTPS_PORT,0));
    const wchar_t* accepted[]={L"application/json",nullptr};
    HttpHandle request(WinHttpOpenRequest(connection.value,L"GET",manifestPath,nullptr,WINHTTP_NO_REFERER,accepted,WINHTTP_FLAG_SECURE));
    DWORD disabled=WINHTTP_DISABLE_COOKIES|WINHTTP_DISABLE_AUTHENTICATION|WINHTTP_DISABLE_REDIRECTS|WINHTTP_DISABLE_KEEP_ALIVE;
    ensure(WinHttpSetOption(request.value,WINHTTP_OPTION_DISABLE_FEATURE,&disabled,sizeof(disabled))!=FALSE,"更新请求限制设置失败");
    DWORD logon=WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH;
    ensure(WinHttpSetOption(request.value,WINHTTP_OPTION_AUTOLOGON_POLICY,&logon,sizeof(logon))!=FALSE,"更新匿名请求设置失败");
    const auto start=GetTickCount64();
    httpCheck(WinHttpSendRequest(request.value,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0),"更新请求发送失败");
    httpCheck(WinHttpReceiveResponse(request.value,nullptr),"更新清单响应失败");
    DWORD code{},length=sizeof(code);
    httpCheck(WinHttpQueryHeaders(request.value,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&code,&length,WINHTTP_NO_HEADER_INDEX),"更新清单HTTP状态读取失败");
    ensure(code==200,("更新清单HTTP状态无效："+std::to_string(code)).c_str());
    wchar_t type[128]{};length=sizeof(type);
    ensure(WinHttpQueryHeaders(request.value,WINHTTP_QUERY_CONTENT_TYPE,WINHTTP_HEADER_NAME_BY_INDEX,type,&length,WINHTTP_NO_HEADER_INDEX)!=FALSE &&
        std::wstring(type).find(L"application/json")==0,"更新清单不是JSON响应");
    DWORD size{};length=sizeof(size);
    if(WinHttpQueryHeaders(request.value,WINHTTP_QUERY_CONTENT_LENGTH|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&size,&length,WINHTTP_NO_HEADER_INDEX))
        ensure(size>0 && size<=manifestLimit,"更新清单体积超限");
    std::string body;body.reserve(4096);
    for(;;) {
        ensure(GetTickCount64()-start<=8000,"更新检查总读取时间超限");
        std::array<char,4096> chunk{};DWORD bytes{};
        httpCheck(WinHttpReadData(request.value,chunk.data(),static_cast<DWORD>(std::min(chunk.size(),manifestLimit+1-body.size())),&bytes),"更新清单读取失败");
        if(!bytes)break;
        body.append(chunk.data(),bytes);ensure(body.size()<=manifestLimit,"更新清单体积超限");
    }
    ensure(!body.empty(),"更新清单为空");return body;
}
class Service {
    std::atomic<int> context_{0}; // 0停用，1任务外，2任务内，3主菜单。
    std::atomic<bool> started_{false},claimed_{false},browserBusy_{false},checkBusy_{false};
    std::atomic<Status> status_{Status::Waiting};
    std::atomic<BrowserStatus> browser_{BrowserStatus::Idle};
    std::shared_ptr<const Snapshot> snapshot_;
    static bool pin() noexcept {
        HMODULE module{};
        return GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(&checkEntry),&module)!=FALSE;
    }
    void finish(Snapshot result) {
        const auto status=result.status;
        std::shared_ptr<const Snapshot> shared=std::make_shared<const Snapshot>(std::move(result));
        std::atomic_store_explicit(&snapshot_,std::move(shared),std::memory_order_release);
        status_.store(status,std::memory_order_release);
    }
    struct CheckRequest {Service* owner;bool automatic;};
    static DWORD WINAPI checkEntry(void* pointer) noexcept {
        std::unique_ptr<CheckRequest> request(static_cast<CheckRequest*>(pointer));
        auto& self=*request->owner;
        SetThreadPriority(GetCurrentThread(),THREAD_MODE_BACKGROUND_BEGIN);
        try {
            // 延后两秒避开首次主菜单加载；只等待本线程，不让游戏线程等联网。
            if(request->automatic) {
                const HANDLE delay=CreateEventW(nullptr,TRUE,FALSE,nullptr);
                ensure(delay!=nullptr,"无法安排后台版本检查");
                WaitForSingleObject(delay,2000);CloseHandle(delay);
            }
            auto result=parse(fetch());
            const auto status=result.status;
            self.finish(std::move(result));
            nativeLog(status==Status::Unpublished?"UPDATE_CHECK_UNPUBLISHED":"UPDATE_CHECK_FINISHED");
        }catch(const std::exception& error) {
            self.status_.store(Status::Failed,std::memory_order_release);
            try {nativeLog(std::string("UPDATE_CHECK_ABANDONED ")+error.what());}catch(...) {}
        }catch(...) {self.status_.store(Status::Failed,std::memory_order_release);}
        self.checkBusy_.store(false,std::memory_order_release);return 0;
    }
    struct BrowserRequest {Service* owner;std::wstring url;};
    static DWORD WINAPI browserEntry(void* pointer) noexcept {
        std::unique_ptr<BrowserRequest> request(static_cast<BrowserRequest*>(pointer));
        const HRESULT com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED|COINIT_DISABLE_OLE1DDE);
        bool opened=false;
        if(SUCCEEDED(com)) {
            // 只使用HTTPS文件参数，无命令行参数、提权或包执行；不等待浏览器关闭。
            SHELLEXECUTEINFOW info{};info.cbSize=sizeof(info);info.fMask=SEE_MASK_FLAG_NO_UI|SEE_MASK_NOASYNC;
            info.lpVerb=L"open";info.lpFile=request->url.c_str();info.nShow=SW_SHOWNORMAL;
            opened=ShellExecuteExW(&info)!=FALSE;
            CoUninitialize();
        }
        request->owner->browser_.store(opened?BrowserStatus::Opened:BrowserStatus::Failed,std::memory_order_release);
        request->owner->browserBusy_.store(false,std::memory_order_release);
        nativeLog(opened?"UPDATE_BROWSER_OPENED":"UPDATE_BROWSER_OPEN_FAILED");return 0;
    }
public:
    void context(int value) noexcept {
        context_.store(value,std::memory_order_release);
        if(value!=3 || started_.exchange(true))return; // 自动检查只申请一次；额外请求必须由用户点击。
        requestCheck(false);
    }
    bool requestCheck(bool manual=true) noexcept {
        if(!context_.load(std::memory_order_acquire) || checkBusy_.exchange(true))return false;
        started_.store(true,std::memory_order_release);
        status_.store(Status::Checking,std::memory_order_release);
        std::atomic_store_explicit(&snapshot_,std::shared_ptr<const Snapshot>{},std::memory_order_release);
        try {
            ensure(pin(),"无法保持版本检查模块有效");
            auto request=std::make_unique<CheckRequest>();request->owner=this;request->automatic=!manual;
            const HANDLE thread=CreateThread(nullptr,0,checkEntry,request.get(),0,nullptr);
            if(!thread)throw std::runtime_error("版本检查线程创建失败："+std::to_string(GetLastError()));
            request.release();CloseHandle(thread);
            nativeLog(manual?"UPDATE_CHECK_STARTED manual":"UPDATE_CHECK_STARTED automatic");return true;
        }catch(const std::exception& error) {
            status_.store(Status::Failed,std::memory_order_release);
            checkBusy_.store(false,std::memory_order_release);
            try {nativeLog(std::string("UPDATE_CHECK_ABANDONED ")+error.what());}catch(...) {}
            return false;
        }catch(...) {
            status_.store(Status::Failed,std::memory_order_release);
            checkBusy_.store(false,std::memory_order_release);return false;
        }
    }
    bool checking()const noexcept{return checkBusy_.load(std::memory_order_acquire);}
    Status status()const noexcept{return status_.load(std::memory_order_acquire);}
    bool outsideMission()const noexcept {const int value=context_.load(std::memory_order_acquire);return value==1 || value==3;}
    BrowserStatus browserStatus()const noexcept{return browser_.load(std::memory_order_acquire);}
    std::shared_ptr<const Snapshot> snapshot()const {return std::atomic_load_explicit(&snapshot_,std::memory_order_acquire);}
    std::shared_ptr<const Snapshot> claimNotice() {
        const auto context=context_.load(std::memory_order_acquire);
        if((context!=1 && context!=3) || status()!=Status::Newer || claimed_.exchange(true))return {};
        return snapshot(); // 结果在任务内返回时暂存，回到任务外才消费，且整次启动只消费一次。
    }
    bool openDownload(const std::string& id) noexcept {
        if(!context_.load(std::memory_order_acquire) || browserBusy_.exchange(true))return false;
        try {
            const auto data=snapshot();ensure(data!=nullptr,"尚无有效下载清单");
            const auto link=std::find_if(data->downloads.begin(),data->downloads.end(),[&](const Download& entry){return entry.id==id;});
            ensure(link!=data->downloads.end(),"下载入口不属于当前清单");
            auto request=std::make_unique<BrowserRequest>();request->owner=this;request->url=checkedUrl(link->url);
            ensure(pin(),"无法保持浏览器接口模块有效");
            browser_.store(BrowserStatus::Opening,std::memory_order_release);
            const HANDLE thread=CreateThread(nullptr,0,browserEntry,request.get(),0,nullptr);
            ensure(thread!=nullptr,"浏览器接口线程创建失败");request.release();CloseHandle(thread);return true;
        }catch(...) {browser_.store(BrowserStatus::Failed,std::memory_order_release);browserBusy_.store(false,std::memory_order_release);return false;}
    }
};
inline Service& service() {static auto* value=new Service();return *value;}
}

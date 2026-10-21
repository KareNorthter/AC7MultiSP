#pragma once
#include <audioclient.h>
#include <audiopolicy.h>
#include <mmdeviceapi.h>

// 只静音本游戏进程的音频会话；枚举、COM和恢复记录均在独立线程执行。
// Windows会保存会话静音，所以先留下恢复记录，异常退出后下一次加载归还。
namespace dualsp {
class ManagementAudio {
    template<class T> struct Ref {
        T* value{};
        ~Ref(){if(value)value->Release();}
        T** out(){return &value;}
        T* operator->()const{return value;}
    };
    struct Lease {
        std::string device,session;
        ISimpleAudioVolume* volume{};
    };
    HANDLE event{};
    std::atomic<bool> wanted{false};
    std::vector<Lease> leases;
    bool applied{};
    const std::filesystem::path record=modRoot()/"management-audio-state.json";
    static std::string utf8(const wchar_t* text) {
        if(!text)return {};
        const int bytes=WideCharToMultiByte(CP_UTF8,0,text,-1,nullptr,0,nullptr,nullptr);
        if(bytes<=0)throw std::runtime_error("音频会话名称转换失败");
        std::string value(bytes,'\0');WideCharToMultiByte(CP_UTF8,0,text,-1,value.data(),bytes,nullptr,nullptr);
        value.pop_back();return value;
    }
    static void checked(HRESULT result,const char* message) {
        if(FAILED(result))throw std::runtime_error(message);
    }
    void save() {
        Json entries=Json::array();
        for(const auto& lease:leases)entries.push_back({{"device",lease.device},{"session",lease.session}});
        const Json data={{"version",1},{"leases",entries}};
        if(data.dump().size()>65536)throw std::runtime_error("音频静音恢复记录过大");
        auto pending=record;pending+=L".tmp";
        {std::ofstream out(pending,std::ios::binary|std::ios::trunc);
            out<<data.dump();out.flush();
            if(!out)throw std::runtime_error("无法写入静音恢复记录");
            out.close();if(!out)throw std::runtime_error("无法关闭静音恢复记录");}
        if(!MoveFileExW(pending.c_str(),record.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("无法保存静音恢复记录");
    }
    void load() {
        if(!std::filesystem::exists(record))return;
        if(std::filesystem::file_size(record)>65536)throw std::runtime_error("音频静音恢复记录过大");
        std::ifstream input(record,std::ios::binary);
        Json data;input>>data;
        if(data.at("version")!=1 || data.at("leases").size()>64)throw std::runtime_error("音频静音恢复记录无效");
        for(const auto& entry:data.at("leases"))leases.push_back({entry.at("device"),entry.at("session"),nullptr});
    }
    template<class F> void enumerate(F&& visit) {
        Ref<IMMDeviceEnumerator> enumerator;
        checked(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,__uuidof(IMMDeviceEnumerator),
            reinterpret_cast<void**>(enumerator.out())),"无法读取游戏音频设备");
        Ref<IMMDeviceCollection> devices;
        checked(enumerator->EnumAudioEndpoints(eRender,DEVICE_STATE_ACTIVE,devices.out()),"无法枚举游戏音频设备");
        UINT deviceCount{};checked(devices->GetCount(&deviceCount),"无法读取音频设备数量");
        for(UINT d=0;d<deviceCount;++d) {
            Ref<IMMDevice> device;if(FAILED(devices->Item(d,device.out())))continue;
            LPWSTR deviceText{};
            if(FAILED(device->GetId(&deviceText)))continue;
            std::string deviceId;
            try {deviceId=utf8(deviceText);}catch(...){CoTaskMemFree(deviceText);throw;}
            CoTaskMemFree(deviceText);
            Ref<IAudioSessionManager2> manager;
            if(FAILED(device->Activate(__uuidof(IAudioSessionManager2),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(manager.out()))))continue;
            Ref<IAudioSessionEnumerator> sessions;
            if(FAILED(manager->GetSessionEnumerator(sessions.out())))continue;
            int count{};if(FAILED(sessions->GetCount(&count)))continue;
            for(int i=0;i<count;++i) {
                Ref<IAudioSessionControl> control;
                if(FAILED(sessions->GetSession(i,control.out())))continue;
                Ref<IAudioSessionControl2> detail;
                if(FAILED(control->QueryInterface(__uuidof(IAudioSessionControl2),reinterpret_cast<void**>(detail.out()))))continue;
                DWORD process{};
                if(detail->GetProcessId(&process)!=S_OK || process!=GetCurrentProcessId())continue;
                LPWSTR sessionText{};if(FAILED(detail->GetSessionIdentifier(&sessionText)))continue;
                std::string sessionId;
                try {sessionId=utf8(sessionText);}catch(...){CoTaskMemFree(sessionText);throw;}
                CoTaskMemFree(sessionText);
                Ref<ISimpleAudioVolume> volume;
                if(FAILED(control->QueryInterface(__uuidof(ISimpleAudioVolume),reinterpret_cast<void**>(volume.out()))))continue;
                visit(deviceId,sessionId,volume.value);
            }
        }
    }
    void restore() {
        // 异常退出留下的记录只有名称，先在当前进程中重新认领；绝不匹配其他进程。
        if(std::any_of(leases.begin(),leases.end(),[](const Lease& lease){return !lease.volume;}))
            enumerate([&](const auto& device,const auto& session,ISimpleAudioVolume* volume) {
                for(auto& lease:leases)if(!lease.volume && lease.device==device && lease.session==session) {
                    volume->AddRef();lease.volume=volume;
                }
            });
        bool changed=false;
        for(auto it=leases.begin();it!=leases.end();) {
            if(it->volume && SUCCEEDED(it->volume->SetMute(FALSE,nullptr))) {
                it->volume->Release();it=leases.erase(it);changed=true;
            }else {
                if(it->volume){it->volume->Release();it->volume=nullptr;}
                ++it;
            }
        }
        if(changed || leases.empty())save();
        applied=false;
        if(!leases.empty())nativeLog("MANAGEMENT_AUDIO_RECOVERY_PENDING");
    }
    void mute() {
        if(!leases.empty())restore(); // 先尝试归还异常退出或设备失效留下的会话。
        enumerate([&](const auto& device,const auto& session,ISimpleAudioVolume* volume) {
            if(std::any_of(leases.begin(),leases.end(),[&](const Lease& lease){return lease.device==device && lease.session==session;}))return;
            BOOL muted{};if(FAILED(volume->GetMute(&muted)) || muted)return; // 原本静音的会话不归本Mod管理。
            if(leases.size()>=64)throw std::runtime_error("游戏音频会话数量超出管理上限");
            leases.push_back({device,session,volume});volume->AddRef();
        });
        save(); // 写入失败时不静音，防止丢失恢复凭据；同步磁盘只发生在本后台线程。
        for(auto& lease:leases)if(lease.volume)checked(lease.volume->SetMute(TRUE,nullptr),"游戏音频静音失败");
        applied=true;
        nativeLog("MANAGEMENT_AUDIO_MUTED");
    }
    static DWORD WINAPI entry(void* context) {static_cast<ManagementAudio*>(context)->run();return 0;}
    void run() noexcept {
        SetThreadPriority(GetCurrentThread(),THREAD_MODE_BACKGROUND_BEGIN);
        const HRESULT com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
        if(FAILED(com)){nativeLog("MANAGEMENT_AUDIO_ERROR COM初始化失败");return;}
        try {load();if(!leases.empty())restore();}catch(const std::exception& error){nativeLog(std::string("MANAGEMENT_AUDIO_RECOVERY_ERROR ")+error.what());CoUninitialize();return;}
        for(;;) {
            if(WaitForSingleObject(event,INFINITE)!=WAIT_OBJECT_0)break;
            try {
                const bool target=wanted.load(std::memory_order_acquire);
                if(target && !applied)mute();
                else if(!target && (applied || !leases.empty())) {restore();nativeLog("MANAGEMENT_AUDIO_RESTORED");}
            }catch(const std::exception& error) {
                nativeLog(std::string("MANAGEMENT_AUDIO_ERROR ")+error.what());
                try {restore();}catch(...) {nativeLog("MANAGEMENT_AUDIO_RECOVERY_DEFERRED");}
            }
        }
        CoUninitialize();
    }
public:
    ManagementAudio() {
        HMODULE module{};
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(&entry),&module))throw std::runtime_error("无法保持音频控制模块有效");
        event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
        if(!event)throw std::runtime_error("无法创建音频控制事件");
        const HANDLE worker=CreateThread(nullptr,0,entry,this,0,nullptr);
        if(!worker){CloseHandle(event);throw std::runtime_error("无法启动音频控制线程");}
        CloseHandle(worker);
    }
    void request(bool mute) noexcept {
        wanted.store(mute,std::memory_order_release);SetEvent(event);
    }
};
inline ManagementAudio& managementAudio() {
    // 核心与Lua解释器同寿命；不在DLL卸载锁内等待COM或后台线程。
    static auto* control=new ManagementAudio();return *control;
}
}

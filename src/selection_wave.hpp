#pragma once
#include <xaudio2.h>
#include <objbase.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <stdexcept>
#include <string>

namespace dualsp {
// 只播放用户提供的 PCM 短提示；实例由游戏线程持有，不接触原生锁定音设备。
class SelectionWave {
    HMODULE library_{}; // 系统 XAudio2 模块；与本机编译器导入库版本解耦。
    IXAudio2* engine_{}; // 独立引擎，退出本次双 SP 会话时释放。
    IXAudio2MasteringVoice* output_{}; // 使用默认输出设备，音量归属当前游戏进程。
    IXAudio2SourceVoice* voice_{}; // 单个声音通道，切换过快时替换旧提示，不排长队。
    bool comOwned_{}; // 只配对本实例成功取得的 COM 初始化计数。
    bool allowed_{}; // 暂停、补给、前后台状态由现有游戏线程调度器更新。
    static constexpr float gain_=0.175f; // 0.1.9 按用户要求在候选 9 基础上再减半；原始 WAV 不变。
    float volume_{}; // 游戏当前 SFXVolume，取值 0..1。
    WAVEFORMATEX format_{}; // 用户 WAV 的 PCM 格式，校验后才创建声源。
    std::vector<BYTE> samples_; // 整段音频常驻会话内存，声源销毁后才释放。

    // 所有音频 API 失败均明确报告，不改播其他提示音。
    static void checked(HRESULT result,const char* operation) {
        if(FAILED(result))throw std::runtime_error(std::string(operation)+" HRESULT="+std::to_string(result));
    }
    // 读取 RIFF 中的无对齐整数，不把原始字节强转成结构体。
    template<class T> static T value(const std::vector<BYTE>& bytes,size_t offset) {
        T result{};std::memcpy(&result,bytes.data()+offset,sizeof(result));return result;
    }
    // 只接受完整 PCM16 短 WAV；跳过合法未知块，损坏文件直接拒绝。
    void load(const std::filesystem::path& path) {
        std::ifstream file(path,std::ios::binary|std::ios::ate); // 本地安装资源，不依赖提取目录。
        if(!file)throw std::runtime_error("无法读取 Audio/SP2.wav");
        const auto length=file.tellg(); // 限制短提示文件大小，避免错误文件占用大量内存。
        if(length<44 || length>4*1024*1024)throw std::runtime_error("SP2 WAV 大小无效");
        std::vector<BYTE> bytes(static_cast<size_t>(length)); // 加载期间的 RIFF 容器。
        file.seekg(0);file.read(reinterpret_cast<char*>(bytes.data()),bytes.size());
        if(!file || std::memcmp(bytes.data(),"RIFF",4) || std::memcmp(bytes.data()+8,"WAVE",4) ||
            value<uint32_t>(bytes,4)!=bytes.size()-8)throw std::runtime_error("SP2 WAV 的 RIFF 不完整");
        bool hasFormat=false,hasData=false; // fmt/data 必须各出现一次。
        for(size_t offset=12;offset<bytes.size();) {
            if(bytes.size()-offset<8)throw std::runtime_error("SP2 WAV 块头不完整");
            const size_t size=value<uint32_t>(bytes,offset+4),data=offset+8; // 当前 RIFF 块长度与数据起点。
            if(size>bytes.size()-data)throw std::runtime_error("SP2 WAV 块越界");
            if(std::memcmp(bytes.data()+offset,"fmt ",4)==0) {
                if(hasFormat || size<16)throw std::runtime_error("SP2 WAV 格式块无效");
                std::memcpy(&format_,bytes.data()+data,16);format_.cbSize=0;hasFormat=true;
            } else if(std::memcmp(bytes.data()+offset,"data",4)==0) {
                if(hasData || !size)throw std::runtime_error("SP2 WAV 音频块无效");
                samples_.assign(bytes.begin()+data,bytes.begin()+data+size);hasData=true;
            }
            offset=data+size+(size&1);
            if(offset>bytes.size())throw std::runtime_error("SP2 WAV 缺少块填充");
        }
        if(!hasFormat || !hasData || format_.wFormatTag!=WAVE_FORMAT_PCM || format_.wBitsPerSample!=16 ||
            (format_.nChannels!=1 && format_.nChannels!=2) || format_.nSamplesPerSec<8000 || format_.nSamplesPerSec>96000 ||
            format_.nBlockAlign!=format_.nChannels*2 || format_.nAvgBytesPerSec!=format_.nSamplesPerSec*format_.nBlockAlign ||
            samples_.size()%format_.nBlockAlign || samples_.size()>format_.nAvgBytesPerSec*5)
            throw std::runtime_error("SP2 WAV 必须是 5 秒以内的单声道或立体声 PCM16");
    }
public:
    // 解绑时先销毁声源，保证异步音频线程不再读取样本，再释放引擎与模块。
    void close() noexcept {
        if(voice_){voice_->DestroyVoice();voice_=nullptr;}
        if(output_){output_->DestroyVoice();output_=nullptr;}
        if(engine_){engine_->Release();engine_=nullptr;}
        if(library_){FreeLibrary(library_);library_=nullptr;}
        if(comOwned_){CoUninitialize();comOwned_=false;}
        samples_.clear();allowed_=false;volume_=0;
    }
    // 每次会话只读取一次 WAV；初始化失败同样释放已取得的资源。
    void open(const std::filesystem::path& path) {
        if(engine_)return;
        try {
            load(path);
            const auto com=CoInitializeEx(nullptr,COINIT_MULTITHREADED); // 已有 STA 时保留宿主线程模式。
            if(com!=RPC_E_CHANGED_MODE)checked(com,"初始化 SP2 音频 COM 失败");
            comOwned_=SUCCEEDED(com);
            library_=LoadLibraryExW(L"xaudio2_9.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
            if(!library_)throw std::runtime_error("无法加载系统 XAudio2 2.9");
            using Create=HRESULT(WINAPI*)(IXAudio2**,UINT32,XAUDIO2_PROCESSOR); // Windows 10/11 系统入口。
            const auto create=reinterpret_cast<Create>(GetProcAddress(library_,"XAudio2Create"));
            if(!create)throw std::runtime_error("系统缺少 XAudio2Create");
            checked(create(&engine_,0,XAUDIO2_DEFAULT_PROCESSOR),"创建 SP2 音频引擎失败");
            checked(engine_->CreateMasteringVoice(&output_),"创建 SP2 输出失败");
        } catch(...) {close();throw;}
    }
    // 暂停或失去焦点即取消短提示，恢复时不补播；不触碰其他游戏音效。
    void update(bool allowed,float volume) {
        if(!std::isfinite(volume) || volume<0 || volume>1)throw std::runtime_error("游戏音效音量超出 0..1");
        DWORD process{};GetWindowThreadProcessId(GetForegroundWindow(),&process); // 只允许本游戏位于前台时播放。
        allowed_=allowed && process==GetCurrentProcessId();volume_=volume;
        if(!allowed_){stop();return;}
        if(voice_)checked(voice_->SetVolume(volume_*gain_),"更新 SP2 音量失败");
    }
    // 切到其他武器时取消残留提示，避免与下一次原生选择音重叠。
    void stop() noexcept {if(voice_){voice_->DestroyVoice();voice_=nullptr;}}
    // 创建一个新声源实现立即重播；销毁旧源已等待音频线程结束，不存在 Flush 的异步竞态。
    void play() {
        stop();update(allowed_,volume_);
        if(!allowed_)return;
        if(!engine_)throw std::runtime_error("SP2 音频尚未初始化");
        checked(engine_->CreateSourceVoice(&voice_,&format_),"创建 SP2 声源失败");
        checked(voice_->SetVolume(volume_*gain_),"设置 SP2 音量失败");
        XAUDIO2_BUFFER buffer{}; // 只借用会话样本内存，播放结束前不得释放。
        buffer.Flags=XAUDIO2_END_OF_STREAM;buffer.AudioBytes=static_cast<UINT32>(samples_.size());buffer.pAudioData=samples_.data();
        checked(voice_->SubmitSourceBuffer(&buffer),"提交 SP2 WAV 失败");
        checked(voice_->Start(),"播放 SP2 WAV 失败");
    }
};
}

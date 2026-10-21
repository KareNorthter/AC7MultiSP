#pragma once
#include <array>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string_view>

namespace dualsp {
inline constexpr size_t logSegmentBytes=1000000,logTextBytes=7680;
inline constexpr unsigned logQueueSlots=128,logImportantReserve=16;
struct LogRecord {
    SYSTEMTIME utc{};
    ULONGLONG tick{},sequence{};
    DWORD thread{};
    unsigned source{},length{};
    bool truncated{};
    std::array<char,logTextBytes> text{};
};

// 固定文件名轮换；只覆盖本Mod最旧分段，不扫描目录或建立额外备份。
class LogStream {
    const wchar_t* name_;
    unsigned archives_;
    HANDLE file_{INVALID_HANDLE_VALUE};
    size_t bytes_{};
    bool failed_{};
    std::filesystem::path path(unsigned index)const {
        return modRoot()/(std::wstring(name_)+(index?L"."+std::to_wstring(index):L"")+L".log");
    }
    bool fail() {
        if(file_!=INVALID_HANDLE_VALUE){CloseHandle(file_);file_=INVALID_HANDLE_VALUE;}
        failed_=true;
        OutputDebugStringW(L"AC7MultiSP: background log storage unavailable; logging stopped for this stream.\n");
        return false;
    }
    static bool writeAll(HANDLE file,const char* data,size_t length) {
        while(length) {
            DWORD written{};
            if(!WriteFile(file,data,static_cast<DWORD>(length),&written,nullptr)||!written)return false;
            data+=written;length-=written;
        }
        return true;
    }
    bool trim(unsigned index) {
        const auto name=path(index);
        const HANDLE file=CreateFileW(name.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,
            nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file==INVALID_HANDLE_VALUE)return GetLastError()==ERROR_FILE_NOT_FOUND;
        LARGE_INTEGER size{};bool ok=GetFileSizeEx(file,&size)!=0;
        if(ok && size.QuadPart>static_cast<LONGLONG>(logSegmentBytes)) {
            std::string tail(logSegmentBytes,'\0');
            LARGE_INTEGER start{};start.QuadPart=size.QuadPart-logSegmentBytes;
            DWORD read{};
            ok=SetFilePointerEx(file,start,nullptr,FILE_BEGIN) &&
                ReadFile(file,tail.data(),static_cast<DWORD>(tail.size()),&read,nullptr) && read==tail.size();
            const auto newline=tail.find('\n');
            const size_t skip=newline==std::string::npos?0:newline+1;
            LARGE_INTEGER zero{};
            if(ok)ok=SetFilePointerEx(file,zero,nullptr,FILE_BEGIN) &&
                writeAll(file,tail.data()+skip,tail.size()-skip) && SetEndOfFile(file);
        }
        CloseHandle(file);return ok;
    }
    bool open(DWORD creation) {
        file_=CreateFileW(path(0).c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,
            creation,FILE_ATTRIBUTE_NORMAL,nullptr);
        LARGE_INTEGER size{},zero{};
        if(file_==INVALID_HANDLE_VALUE || !GetFileSizeEx(file_,&size) || size.QuadPart<0 ||
            size.QuadPart>static_cast<LONGLONG>(logSegmentBytes) || !SetFilePointerEx(file_,zero,nullptr,FILE_END))return fail();
        bytes_=static_cast<size_t>(size.QuadPart);return true;
    }
    bool rotate() {
        CloseHandle(file_);file_=INVALID_HANDLE_VALUE;
        unsigned target=1;ULONGLONG oldest=~ULONGLONG{};
        for(unsigned index=1;index<=archives_;++index) {
            WIN32_FILE_ATTRIBUTE_DATA info{};
            if(!GetFileAttributesExW(path(index).c_str(),GetFileExInfoStandard,&info)){target=index;break;}
            const ULONGLONG time=(ULONGLONG(info.ftLastWriteTime.dwHighDateTime)<<32)|info.ftLastWriteTime.dwLowDateTime;
            if(time<oldest){oldest=time;target=index;}
        }
        // 后台复制到固定旧分段，再清空当前文件；总共最多十个1 MB文件。
        if(!CopyFileW(path(0).c_str(),path(target).c_str(),FALSE))return fail();
        return open(CREATE_ALWAYS);
    }
public:
    LogStream(const wchar_t* name,unsigned archives):name_(name),archives_(archives){}
    ~LogStream(){if(file_!=INVALID_HANDLE_VALUE)CloseHandle(file_);}
    LogStream(const LogStream&)=delete;
    LogStream& operator=(const LogStream&)=delete;
    bool prepare() {
        for(unsigned index=0;index<=archives_;++index)if(!trim(index))return fail();
        return open(OPEN_ALWAYS);
    }
    bool append(const std::string& batch) {
        if(failed_ || batch.empty())return !failed_;
        if(batch.size()>logSegmentBytes)return fail();
        if(bytes_+batch.size()>logSegmentBytes && !rotate())return false;
        if(!writeAll(file_,batch.data(),batch.size()))return fail();
        bytes_+=batch.size();return true;
    }
};

class AsyncLog {
    struct Recent {ULONGLONG tick{};unsigned length{};std::array<char,logTextBytes> text{};};
    std::mutex mutex_;
    std::array<LogRecord,logQueueSlots> queue_{};
    std::array<Recent,3> recent_{};
    unsigned head_{},count_{};
    HANDLE wake_{};
    std::atomic<bool> ready_{false};
    std::atomic<ULONGLONG> sequence_{0},normalLost_{0},importantLost_{0},duplicates_{0};
    static bool important(std::string_view text) {
        for(const char* token:{"ERROR","FAIL","FATAL","CRASH","NATIVE_BEGIN","DEBUG_AIRCRAFT_RESTART","CHECKPOINT"})
            if(text.find(token)!=std::string_view::npos)return true;
        return false;
    }
    bool pop(LogRecord& record) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!count_)return false;
        record=queue_[head_];head_=(head_+1)%logQueueSlots;--count_;return true;
    }
    static std::string format(const LogRecord& record) {
        char header[192]{};const auto& t=record.utc;
        std::snprintf(header,sizeof(header),"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ tick=%llu seq=%llu pid=%lu tid=%lu %s ",
            unsigned(t.wYear),unsigned(t.wMonth),unsigned(t.wDay),unsigned(t.wHour),unsigned(t.wMinute),unsigned(t.wSecond),unsigned(t.wMilliseconds),
            record.tick,record.sequence,static_cast<unsigned long>(GetCurrentProcessId()),static_cast<unsigned long>(record.thread),
            record.source==0?"LUA":record.source==1?"CORE":"UI");
        std::string line(header);line.append(record.text.data(),record.length);
        if(record.truncated)line+=" [LOG_TRUNCATED]";
        line+='\n';return line;
    }
    LogRecord internal(const char* text) {
        LogRecord record;GetSystemTime(&record.utc);record.tick=GetTickCount64();record.thread=GetCurrentThreadId();
        record.source=1;record.sequence=0; // 0为后台汇总，业务事件使用跨文件的单调入队序号。
        record.length=static_cast<unsigned>(std::min(std::strlen(text),logTextBytes));
        std::memcpy(record.text.data(),text,record.length);return record;
    }
    static DWORD WINAPI entry(void* self)noexcept {
        try {static_cast<AsyncLog*>(self)->run();}
        catch(...) {static_cast<AsyncLog*>(self)->ready_.store(false,std::memory_order_release);}
        return 0;
    }
    void run() {
        SetThreadPriority(GetCurrentThread(),THREAD_MODE_BACKGROUND_BEGIN);
        LogStream mod(L"mod",3),native(L"native",5);
        const bool modReady=mod.prepare(),nativeReady=native.prepare();
        if(!modReady || !nativeReady){ready_.store(false,std::memory_order_release);return;}
        const auto start=format(internal("LOG_SESSION version=2.23.0 budget_bytes=10000000 segment_bytes=1000000 mod_files=4 native_files=6 queue_slots=128 interval_ms=500"));
        mod.append(start);native.append(start);
        std::array<std::string,2> batches;for(auto& batch:batches)batch.reserve(65536);
        ULONGLONG nextBatch=GetTickCount64();
        for(;;) {
            WaitForSingleObject(wake_,500);
            const auto now=GetTickCount64();
            if(now<nextBatch)Sleep(static_cast<DWORD>(nextBatch-now)); // 只限制后台，故障刷屏也不无限抢写。
            LogRecord record;
            size_t batchBytes=0;
            // 每批不超过约64 KiB，间隔至少250毫秒；不随游戏帧率增加磁盘吞吐。
            for(unsigned i=0;i<64 && batchBytes<57344 && pop(record);++i) {
                const unsigned channel=record.source==0?0:1;
                const auto line=format(record);auto& batch=batches[channel];
                batchBytes+=line.size();
                if(batch.size()+line.size()>65536) {(channel?native:mod).append(batch);batch.clear();}
                batch+=line;
            }
            const auto normal=normalLost_.exchange(0),urgent=importantLost_.exchange(0),repeated=duplicates_.exchange(0);
            if(normal || urgent || repeated) {
                char summary[192]{};
                std::snprintf(summary,sizeof(summary),"LOG_PRESSURE normal_dropped=%llu important_dropped=%llu duplicates_suppressed=%llu",normal,urgent,repeated);
                batches[1]+=format(internal(summary));
            }
            mod.append(batches[0]);native.append(batches[1]);
            for(auto& batch:batches)batch.clear();
            nextBatch=GetTickCount64()+250;
        }
    }
public:
    bool available()const noexcept {return ready_.load(std::memory_order_acquire);}
    AsyncLog() {
        HMODULE pin{};
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(&entry),&pin))return;
        wake_=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!wake_)return;
        ready_.store(true,std::memory_order_release);
        const HANDLE thread=CreateThread(nullptr,0,entry,this,0,nullptr);
        if(!thread){ready_.store(false,std::memory_order_release);CloseHandle(wake_);wake_=nullptr;return;}
        CloseHandle(thread);
    }
    bool submit(unsigned source,const char* text,size_t length) {
        if(!ready_.load(std::memory_order_acquire)||source>2||!text)return false;
        const bool urgent=important(std::string_view(text,std::min(length,logTextBytes)));
        std::unique_lock<std::mutex> lock(mutex_,std::try_to_lock);
        if(!lock.owns_lock() || count_>=logQueueSlots || (!urgent && count_>=logQueueSlots-logImportantReserve)) {
            (urgent?importantLost_:normalLost_).fetch_add(1,std::memory_order_relaxed);return false;
        }
        const auto now=GetTickCount64();size_t bytes=std::min(length,logTextBytes);
        if(bytes<length)while(bytes && (static_cast<unsigned char>(text[bytes])&0xc0)==0x80)--bytes;
        auto& recent=recent_[source];
        if(now-recent.tick<1000 && recent.length==bytes && !std::memcmp(recent.text.data(),text,bytes)) {
            duplicates_.fetch_add(1,std::memory_order_relaxed);return true;
        }
        auto& record=queue_[(head_+count_)%logQueueSlots];
        GetSystemTime(&record.utc);record.tick=now;record.sequence=++sequence_;record.thread=GetCurrentThreadId();
        record.source=source;record.length=static_cast<unsigned>(bytes);record.truncated=bytes<length;
        std::memcpy(record.text.data(),text,bytes);++count_;
        recent.tick=now;recent.length=record.length;std::memcpy(recent.text.data(),text,bytes);
        const bool wake=urgent || count_==64;
        lock.unlock();if(wake)SetEvent(wake_);return true;
    }
};
inline AsyncLog& asyncLog() {
    static auto* instance=new AsyncLog; // 进程退出交给系统回收，不在DLL卸载锁内等待后台线程。
    return *instance;
}
}

// CFA类别/配置切换边界：取消尚未开始的旧输入，活动批次由原生自然收尾。
// 在continuous_sp.hpp之后包含；仅游戏线程、当前玩家会话使用，与多挂外观开关无关。
#pragma once

void cfaFireTransitionQualified() {
    static bool qualified=false;
    if(qualified)return;
    verifyVersion();
    const Addr base=continuousBase();
    auto code=[base](size_t offset,const auto& bytes) {
        checkMemory(base+offset,sizeof(bytes));
        require(std::memcmp(reinterpret_cast<const void*>(base+offset),bytes,sizeof(bytes))==0,
            "CFA切换：原生发射队列布局不匹配");
    };
    // Pressed把单字节输入追加至+2b0数组，+2b8是Num；不是弹药或发射体数组。
    constexpr unsigned char append[]={0x48,0x63,0xbb,0xb8,0x02,0,0,0x8d,0x47,0x01,0x89,0x83,0xb8,0x02,0,0};
    constexpr unsigned char data[]={0x48,0x8b,0xc7,0x48,0x03,0x83,0xb0,0x02,0,0,0x74,0x89};
    constexpr unsigned char value[]={0xc6,0,0x01};
    // 每次Tick先从当前armed生成组号；旧输入/活动批次本身没有固定武器组号。
    constexpr unsigned char category[]={0x80,0xbf,0xa1,0x02,0,0,0,0x48,0x8b,0xcf,0x0f,0x95,0xc2,0xfe,0xc2,0x88,0x97,0xe4,0x02,0,0};
    constexpr unsigned char start[]={0x66,0xc7,0x87,0xc0,0x02,0,0,0x01,0,0x44,0x89,0xa7,0xd8,0x02,0,0};
    constexpr unsigned char finishing[]={0x66,0xc7,0x87,0xc0,0x02,0,0,0,0x01};
    constexpr unsigned char finished[]={0x66,0xc7,0x87,0xc0,0x02,0,0,0,0};
    code(0x884289,append);code(0x8842af,data);code(0x8842c5,value);
    code(0x889d46,category);code(0x889e47,start);code(0x88a557,finishing);code(0x88a7eb,finished);
    qualified=true; // 静态代码只核对一次，不在每50ms等待时重复扫描。
}

bool cfaFireTransitionBusy(Addr plane,Addr activator) {
    if(!continuousCfa44(plane))return false;
    require(session.gameThread && GetCurrentThreadId()==session.gameThread,"CFA切换：不在会话游戏线程");
    cfaFireTransitionQualified();
    // +2c0是活动批次，+2c1是待收尾；+2e0是总数，不能用>1判断是否已结束。
    return read<unsigned short>(activator+0x2c0)!=0 || read<unsigned char>(activator+0x2a5)!=0 ||
        read<int>(activator+0x2b8)!=0;
}

void cfaDiscardQueuedFireInput(Addr plane,Addr activator) {
    if(!continuousCfa44(plane))return;
    require(session.gameThread && GetCurrentThreadId()==session.gameThread,"CFA切换：不在会话游戏线程");
    cfaFireTransitionQualified();
    require(read<unsigned char>(activator+0x2a5)==0,"CFA切换：须先释放发射输入");
    const Addr data=read<Addr>(activator+0x2b0);
    const int count=read<int>(activator+0x2b8),capacity=read<int>(activator+0x2bc);
    require(count>=0 && capacity>=count && (!capacity || data),"CFA切换：待发输入数组无效");
    // 数组元素是平凡单字节。仅清Num，保留分配；不动+2c8目标表、批次计数或活动武器。
    // 本段没有引擎调用，读取与写回在同一游戏线程完成。
    if(count)write<int>(activator+0x2b8,0);
}

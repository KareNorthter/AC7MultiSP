#pragma once
#include <algorithm>

namespace dualsp {
struct PanelPointer {
    float x{},y{},width{1},height{1};
    void bounds(float w,float h) {
        width=std::max(1.f,w);height=std::max(1.f,h);
        absolute(x,y);
    }
    void center(float w,float h) {bounds(w,h);absolute(width*.5f,height*.5f);}
    void absolute(float px,float py) {
        x=std::clamp(px,0.f,width-1.f);y=std::clamp(py,0.f,height-1.f);
    }
    void relative(float dx,float dy) {absolute(x+dx,y+dy);}
};
}

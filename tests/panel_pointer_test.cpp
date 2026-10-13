#include "../src/panel_pointer.hpp"
#include <cassert>
#include <iostream>

int main() {
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

#include "../src/reload_timer.hpp"
#include <cassert>
#include <iostream>
#include <limits>

int main() {
    using namespace dualsp;
    validateReload(10,10.25f); // Previously stopped the entire controller.
    validateReload(10,-.5f);
    assert(advanceReload(10,-.5f,.25,true)==-.25f); // Preserve the delay.
    assert(advanceReload(10,-.5f,1,true)==.5f);
    assert(advanceReload(10,10.25f,30,true)==10);
    assert(advanceReload(10,0,30,false)==0); // No extra hardpoint reloading.
    assert(advanceReload(10,-.5f,30,false)==-.5f);
    assert(reloadProgress(10,-.5f)==0);
    assert(reloadProgress(10,10.25f)==1);
    assert(reloadProgress(0,0)==1);
    assert(rebuildReload(10,10,0)<10);
    assert(rebuildReload(10,10,0)>9.99f);
    assert(rebuildReload(10,10,10)==10);
    assert(rebuildReload(10,-.5f,0)==-.5f);
    int rejected=0;
    for(float elapsed : {std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
        try {validateReload(10,elapsed);} catch(const std::runtime_error&) {++rejected;}
    }
    assert(rejected==2);
    try {validateReload(-1,0);assert(false);}catch(const std::runtime_error&) {}
    try {advanceReload(10,0,-1,true);assert(false);}catch(const std::runtime_error&) {}
    std::cout<<"reload_timer: signed offsets, overshoot, inactive window and invalid values passed\n";
}

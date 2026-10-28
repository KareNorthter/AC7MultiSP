#pragma once
#include "common.hpp"
#include <set>
#include <stdexcept>

namespace dualsp::development {
struct FaultConfig {std::string test;int target{-1};};
inline const FaultConfig& config() {
    static const FaultConfig value=[] {
        FaultConfig result;
#ifdef AC7MULTISP_DEVELOPMENT_FAULT_TESTS
        nativeLog("DEV_FAULT_BUILD enabled; configuration defaults to off");
        try {
            const auto file=modRoot()/"DevFaultTest.json";
            if(!std::filesystem::exists(file))return result;
            if(std::filesystem::file_size(file)>1024)throw std::runtime_error("config too large");
            std::ifstream input(file,std::ios::binary);const auto data=Json::parse(input);
            const std::set<std::string> allowed{"T1","T2","T3-weapon","T3-ammo","T3-aircraft","T4","T5","T6","T7",
                "U-current","U-newer","U-failed","U-unpublished"};
            if(!data.is_object() || data.size()!=2 || !data.contains("case") || !data.contains("target") ||
                !data["case"].is_string() || !data["target"].is_number_integer())throw std::runtime_error("invalid fields");
            result.test=data["case"].get<std::string>();result.target=data["target"].get<int>();
            if(!allowed.count(result.test) || result.target<0 || result.target>100000)throw std::runtime_error("invalid case or target");
            nativeLog("DEV_FAULT_TEST_ARMED case="+result.test+" target="+std::to_string(result.target));
        }catch(const std::exception& error) {nativeLog(std::string("DEV_FAULT_CONFIG_REJECTED ")+error.what());result={};}
#endif
        return result;
    }();
    return value;
}
inline bool is(const char* test,int target) {const auto& value=config();return value.test==test && value.target==target;}
inline std::atomic<unsigned> consumed{0};
inline bool has(unsigned bit) {return (consumed.load()&bit)!=0;}
inline bool claim(unsigned bit,const char* stage) {
    if(consumed.fetch_or(bit)&bit)return false;
    nativeLog("DEV_FAULT_TEST_TRIGGER case="+config().test+" target="+std::to_string(config().target)+" stage="+stage);
    return true;
}
inline bool catalogue() {
    const auto& test=config().test;
    return (test=="T1" || test=="T2" || test.rfind("T3-",0)==0) && claim(1,"catalogue_input_copy");
}
inline std::string updateResponse() {
    const auto& test=config().test;
    nativeLog("DEV_UPDATE_RESPONSE "+test);
    if(test=="U-failed")throw std::runtime_error("DEV_TEST controlled update transport failure");
    Json result={{"schema",1},{"product","AC7MultiSP"},{"channel","stable"}};
    if(test=="U-unpublished") {result["status"]="preparing";return result.dump();}
    result["status"]="ready";result["latest_version"]=test=="U-newer"?"2.26.3":"2.26.1";
    result["release_notes"]={{"zh","开发测试响应，请勿据此下载。"},{"en","Development response. Do not download."}};
    result["downloads"]=Json::array({{{"id","dev"},{"url","https://ac7multisp-updates.pages.dev/"},
        {"label",{{"zh","开发测试"},{"en","Development test"}}}}});
    return result.dump();
}
}

#pragma once
#include "../../domain/Perf.h"
#include "../../safety/ReadOnlyGuard.h"
#include <vector>
#include <string>
#include <dirent.h>
#include <fstream>

namespace polaris::providers::real {

class RealThermalProvider {
public:
    static std::vector<domain::ThermalInfo> getThermals() {
        std::vector<domain::ThermalInfo> out;
        // /sys/class/hwmon/hwmon*/temp*_input
        DIR* d = opendir("/sys/class/hwmon");
        if(d){
            struct dirent* e;
            while((e=readdir(d))){
                if(e->d_name[0]=='.') continue;
                std::string hw = std::string("/sys/class/hwmon/") + e->d_name + "/";
                // name
                std::string name;
                auto f = safety::openReadOnly(hw + "name");
                if(f.is_open()) std::getline(f,name);
                // iterate temp*_input
                for(int i=1;i<=10;i++){
                    std::string p = hw + "temp" + std::to_string(i) + "_input";
                    auto ff = safety::openReadOnly(p);
                    if(!ff.is_open()) continue;
                    std::string v; ff>>v;
                    try {
                        float temp = std::stof(v)/1000.0f;
                        domain::ThermalInfo t;
                        t.source = std::string(e->d_name)+"/temp"+std::to_string(i);
                        t.label = name;
                        t.tempC = temp;
                        // high/crit if exists
                        auto fh = safety::openReadOnly(hw+"temp"+std::to_string(i)+"_max");
                        if(fh.is_open()){ std::string vv; fh>>vv; try{t.highC=std::stof(vv)/1000;}catch(...){} }
                        auto fc = safety::openReadOnly(hw+"temp"+std::to_string(i)+"_crit");
                        if(fc.is_open()){ std::string vv; fc>>vv; try{t.critC=std::stof(vv)/1000;}catch(...){} }
                        out.push_back(t);
                    } catch(...){}
                }
            }
            closedir(d);
        }
        // /sys/class/thermal/thermal_zone*/temp
        DIR* d2 = opendir("/sys/class/thermal");
        if(d2){
            struct dirent* e;
            while((e=readdir(d2))){
                if(std::string(e->d_name).rfind("thermal_zone",0)!=0) continue;
                std::string base = std::string("/sys/class/thermal/")+e->d_name+"/";
                auto ff = safety::openReadOnly(base+"temp");
                if(!ff.is_open()) continue;
                std::string v; ff>>v; try{
                    float temp=std::stof(v)/1000.0f;
                    std::string type; auto ft=safety::openReadOnly(base+"type"); if(ft.is_open()) std::getline(ft,type);
                    domain::ThermalInfo t; t.source=e->d_name; t.label=type; t.tempC=temp;
                    out.push_back(t);
                }catch(...){}
            }
            closedir(d2);
        }
        return out;
    }
};

} // namespace polaris::providers::real

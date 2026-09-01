#include <cassert>
#include <string>
#include <sstream>
#include <iostream>
#include <cstdint>
#include <map>
#include <cstdio>

// Test parsers for /etc/os-release, /proc/meminfo, /proc/cpuinfo without touching host
void test_os_release_parser(){
    std::string sample = R"EOS(NAME="Fedora Linux"
VERSION="44 (KDE Plasma Desktop Edition)"
ID=fedora
VERSION_ID=44
VARIANT="KDE Plasma Desktop Edition"
VARIANT_ID=kde
)EOS";
    std::istringstream iss(sample);
    std::string line;
    std::map<std::string,std::string> kv;
    while(std::getline(iss,line)){
        auto eq=line.find('=');
        if(eq==std::string::npos) continue;
        std::string k=line.substr(0,eq);
        std::string v=line.substr(eq+1);
        if(!v.empty() && v.front()=='"'){ v=v.substr(1); if(!v.empty() && v.back()=='"') v.pop_back(); }
        kv[k]=v;
    }
    assert(kv["ID"]=="fedora");
    assert(kv["VARIANT_ID"]=="kde");
    assert(kv["VERSION_ID"]=="44");
    std::cout << "os-release parser OK\n";
}

void test_meminfo_parser(){
    std::string sample="MemTotal:       11968360 kB\nMemAvailable:    7222804 kB\nCached:          6205716 kB\n";
    std::istringstream iss(sample);
    uint64_t total=0, avail=0;
    std::string line;
    while(std::getline(iss,line)){
        if(line.rfind("MemTotal:",0)==0){ unsigned long v=0; sscanf(line.c_str(),"MemTotal: %lu",&v); total=v; }
        if(line.rfind("MemAvailable:",0)==0){ unsigned long v=0; sscanf(line.c_str(),"MemAvailable: %lu",&v); avail=v; }
    }
    (void)total; (void)avail;
    assert(total==11968360);
    assert(avail==7222804);
    std::cout << "meminfo parser OK\n";
}

void test_boot_parse(){
    std::string s="Startup finished in 3.275s (firmware) + 11.427s (loader) + 1.484s (kernel) + 3.931s (initrd) + 54.106s (userspace) = 1min 14.225s";
    float fw=0,lo=0,ke=0,init=0,us=0;
    sscanf(s.c_str(),"Startup finished in %fs (firmware) + %fs (loader) + %fs (kernel) + %fs (initrd) + %fs (userspace)",&fw,&lo,&ke,&init,&us);
    assert(fw==3.275f);
    assert(us==54.106f);
    std::cout << "boot parser OK\n";
}

int main(){
    test_os_release_parser();
    test_meminfo_parser();
    test_boot_parse();
    std::cout << "All parser tests passed\n";
    return 0;
}

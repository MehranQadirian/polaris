#include "../../core/providers/mock/FakeProviders.h"
#include <cassert>
int main(){
    polaris::providers::mock::FakeHardwareProvider p;
    auto cpu = p.getCpu();
    assert(cpu.model == "Intel(R) Core(TM) i5-10210U");
    auto gpus = p.getGpus();
    assert(gpus.size()==2);
    assert(gpus[1].pciId=="10de:174d");
    return 0;
}

#include "examples/alpha250/adc-dac-bram/adc_dac_bram.hpp"
#include <cstdlib>
#include <algorithm>
#include <iostream>
#include <string_view>
void check(bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';std::abort();}}
int main(int argc,char** argv) {
    check(argc==2,"case required");const std::string_view test=argv[1];AdcDacBram driver;
    if(test=="fresh") {
        const auto first=driver.get_adc();
        check(first.front()==1 && first.back()==1,"First capture was stale/incomplete");
        const auto second=driver.get_adc();
        check(second.front()==2 && second.back()==2,"Second capture was stale/incomplete");
        check(hw::capture.reads==2 && hw::capture.complete==1,"Completion toggle not handled in both directions");
    } else if(test=="manual") {
        driver.trigger_acquisition();
        check(!hw::capture.pending && hw::capture.requests==1 && hw::capture.reads==0,"Manual trigger did not finish");
        check(driver.get_adc().front()==2,"Manual request confused next get_adc completion");
    } else if(test=="timeout") {
        hw::capture.stopped=true;
        const auto [valid,data]=driver.get_adc_checked();
        check(!valid && hw::capture.reads==0,"Incomplete acquisition returned stale data");
        check(std::all_of(data.begin(),data.end(),[](auto word){return word==0;}),"Failed capture data not cleared");
        hw::capture.stopped=false;
        const auto [recovered,fresh]=driver.get_adc_checked();
        check(recovered && fresh.front()==2 && hw::capture.reads==1,"Did not recover after timeout");
    } else {check(false,"unknown case");}
}

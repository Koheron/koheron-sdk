#include "../phase-spectrum.hpp"
#include <fstream>
#include <iostream>
#include <string>
using Phase=scicpp::units::radian<float>;
int main(int argc,char** argv) {
 if(argc!=5)return 2;
 std::ifstream input(argv[1],std::ios::binary);
 std::ofstream output(argv[2],std::ios::binary);
 const float fs=std::stof(argv[3]);
 const bool detrend=std::stoi(argv[4])!=0;
 scicpp::signal::Spectrum<float> spectrum;
 std::array<float,32000> a{},b{};
 std::array<Phase,32000> x{},y{};
 while(input.read(reinterpret_cast<char*>(a.data()),sizeof(a))) {
  if(!input.read(reinterpret_cast<char*>(b.data()),sizeof(b)))return 3;
  for(std::size_t i=0;i<a.size();++i){x[i]=Phase{a[i]};y[i]=Phase{b[i]};}
  const auto csd=pna_spectrum::cross_density(x,y,scicpp::units::frequency<float>{fs},spectrum,detrend);
  for(auto value:csd){const float pair[]={value.real().eval(),value.imag().eval()};output.write(reinterpret_cast<const char*>(pair),sizeof(pair));}
 }
 return output ? 0 : 4;
}

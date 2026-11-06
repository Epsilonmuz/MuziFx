#include "../../Source/shared/MatchedFilters.h"
#include <cstdio>
using namespace muzi::dsp;
int main(){ const char* names[]={"highPass2","lowPass2","bandPass","notch","bell-12","bell+12","lowShelf","highShelf"}; long cnt[8]={0}; bool shown[8]={0};
 for(double fs:{44100.,48000.,96000.}) for(double f=10; f<0.49*fs; f*=1.01) for(double q=0.1;q<20;q*=1.15){
   Biquad bs[]={highPass2(f,q,fs),lowPass2(f,q,fs),bandPass(f,q,fs),notch(f,q,fs),bell(f,-12,q,fs),bell(f,12,q,fs),lowShelf(f,6,q,fs),highShelf(f,-6,q,fs)};
   for(int i=0;i<8;++i){auto&b=bs[i]; if(!std::isfinite(b.b0)||!std::isfinite(b.b1)||!std::isfinite(b.b2)){ ++cnt[i]; if(!shown[i]){shown[i]=1; printf("%-10s first bad: f=%.1f q=%.3f fs=%.0f  b=%g %g %g a=%g %g\n",names[i],f,q,fs,b.b0,b.b1,b.b2,b.a1,b.a2);} } } }
 for(int i=0;i<8;++i) printf("%-10s %ld bad\n",names[i],cnt[i]); }

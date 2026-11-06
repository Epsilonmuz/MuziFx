#include "../../Source/eq/MuziEqEngine.h"
#include "../../Source/glue/MuziGlueEngine.h"
#include <chrono>
#include <cstdio>
#include <vector>
#include <random>
using namespace muzi;
int main(){ const double fs=48000; const int n=48000*10, blk=256; std::vector<float> L(n),R(n); std::mt19937 r(1); std::normal_distribution<float> d(0,0.1f);
 for(int i=0;i<n;++i){L[i]=d(r);R[i]=d(r);}
 auto run=[&](const char* name, auto fn){ auto L2=L,R2=R; auto t0=std::chrono::steady_clock::now(); for(int s=0;s<n;s+=blk) fn(L2.data()+s,R2.data()+s,blk);
   double sec=std::chrono::duration<double>(std::chrono::steady_clock::now()-t0).count(); printf("%-46s %5.2f%% of one core (stereo, 48 kHz)\n",name,100*sec/10.0);};
 { MuziEqEngine e; e.prepare(fs); EqParams p; for(int b=0;b<8;++b) p.band[b]={true,Bell,(float)(100*std::pow(2,b)),3,1};
   run("EQ: 8 static bells",[&](float*a,float*b,int k){e.process(a,b,k,p);}); }
 { MuziEqEngine e; e.prepare(fs); EqParams p; p.band[0]={true,LowCut,30,0,0.71f,5}; p.band[7]={true,HighCut,18000,0,0.71f,5}; for(int b=1;b<7;++b) p.band[b]={true,Bell,(float)(100*std::pow(2,b)),-6,1,1,Stereo,true,-30};
   run("EQ: 6 dynamic bells + two 48 dB/oct cuts",[&](float*a,float*b,int k){e.process(a,b,k,p);}); }
 { MuziEqEngine e; e.prepare(fs); EqParams p; for(int b=0;b<8;++b) p.band[b]={true,Bell,1000,6,2};
   int c=0; run("EQ: 8 bells, all being automated",[&](float*a,float*b,int k){ for(int b2=0;b2<8;++b2) p.band[b2].freq=(float)(200+ (c%400)*10+b2*50); ++c; e.process(a,b,k,p);}); }
 { MuziGlueEngine g; g.prepare(fs); GlueParams p; p.thresholdDb=-30; run("Glue: auto release, SC HPF 80 Hz",[&](float*a,float*b,int k){p.scHpfHz=80; g.process(a,b,k,p);}); }
}

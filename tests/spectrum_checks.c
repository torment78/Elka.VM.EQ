#include "spectrum.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do{if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);return 1;}checks++;}while(0)
static int checks,reg_result,regs,starts,stops,unregs;
static SpecCallback callback;static void* context;
static long __stdcall reg(long mode,SpecCallback cb,void* user,char name[64]){regs++;if(mode!=4)return -1;callback=cb;context=user;if(reg_result==1)strcpy_s(name,64,"Other application");return reg_result;}
static long __stdcall start(void){starts++;return 0;}
static long __stdcall stop(void){stops++;return 0;}
static long __stdcall unreg(void){unregs++;return 0;}
static float input[128][512],output[128][512];
int main(void){
 CHECK(sizeof(SpecFrame)==1728);CHECK(sizeof(SpecAudioBuffer)==2064);
 SpecAudioBuffer b={48000,512,98,64,{0},{0}};
 for(int c=0;c<128;c++){b.r[c]=input[c];b.w[c]=output[c];for(int i=0;i<512;i++){unsigned bits=0x12345678u+(unsigned)c*987653u+i*17011u;memcpy(&input[c][i],&bits,4);}}
 int ins[]={28,62,98},outs[]={16,40,64};
 for(int e=0;e<3;e++){b.nbi=ins[e];b.nbo=outs[e];memset(output,0,sizeof(output));CHECK(spec_main_passthrough(&b));for(int c=0;c<b.nbo;c++)CHECK(!memcmp(output[c],input[b.nbi-b.nbo+c],sizeof(output[c])));}
 int channels;CHECK(spec_source_offset(&b,0,4,&channels)==8&&channels==2);CHECK(spec_source_offset(&b,1,7,&channels)==90&&channels==8);CHECK(spec_source_offset(&b,1,8,&channels)==-1);
 b.w[0]=b.r[34];CHECK(spec_main_passthrough(&b));b.w[0]=output[0];b.nbi=20;CHECK(!spec_main_passthrough(&b));b.nbi=98;
 SpecAnalysis* a=spec_analysis_create();CHECK(a!=NULL);float samples[SPEC_SAMPLES],bins[SPEC_BINS];int rates[]={44100,48000,96000};
 for(int r=0;r<3;r++){for(int i=0;i<SPEC_SAMPLES;i++)samples[i]=(float)(pow(10,-6./20)*sin(2*3.141592653589793*500*i/rates[r]));spec_analyze(a,samples,rates[r],bins);float peak=-120;int index=0;for(int i=0;i<SPEC_BINS;i++)if(bins[i]>peak){peak=bins[i];index=i;}printf("500 Hz / %d Hz: %.4f dBFS\n",rates[r],peak);CHECK(fabs(peak+6)<.1);CHECK(fabs(20*pow(1000,(index+.5)/SPEC_BINS)-500)<25);}
 memset(samples,0,sizeof(samples));spec_analyze(a,samples,48000,bins);for(int i=0;i<SPEC_BINS;i++)CHECK(bins[i]==-120);samples[0]=NAN;spec_analyze(a,samples,48000,bins);CHECK(isfinite(bins[0]));spec_analysis_destroy(a);
 Spectrum* s=spec_create();CHECK(s!=NULL);spec_bind(s,reg,start,stop,unreg);SpecFrame frame;spec_read(s,&frame);CHECK(frame.state==SPEC_OFF&&!frame.requested);spec_manage(s,NULL,1);CHECK(regs==0);
 reg_result=1;spec_request(s,1,1,0,-1);spec_manage(s,NULL,1);spec_read(s,&frame);CHECK(frame.state==SPEC_BUSY);spec_manage(s,NULL,1);CHECK(regs==1&&starts==0&&stops==0&&unregs==0);spec_request(s,0,1,0,-1);spec_manage(s,NULL,1);CHECK(stops==0&&unregs==0);
 reg_result=0;spec_request(s,1,1,0,0);spec_manage(s,NULL,1);CHECK(starts==1);
 memset(input,0,sizeof(input));for(int block=0;block<45;block++){for(int i=0;i<512;i++)input[34][i]=(float)(pow(10,-6./20)*sin(2*3.141592653589793*500*(i+block*512)/48000));callback(context,20,&b,0);CHECK(!memcmp(output[0],input[34],sizeof(output[0])));Sleep(5);}
 spec_read(s,&frame);CHECK(frame.state==SPEC_ACTIVE&&frame.audio_frames>0);float peak=-120;for(int i=0;i<SPEC_BINS;i++)if(frame.db[i]>peak)peak=frame.db[i];CHECK(fabs(peak+6)<.15);
 spec_request(s,1,1,1,0);spec_read(s,&frame);CHECK(frame.state==SPEC_WAITING&&frame.db[0]==-120);
 spec_request(s,0,1,1,0);spec_manage(s,NULL,1);CHECK(stops==1&&unregs==1);CHECK(spec_destroy(s));CHECK(stops==1&&unregs==1);
 printf("PASS %d spectrum checks\n",checks);return 0;
}

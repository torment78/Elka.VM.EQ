#include "spectrum.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#define PI 3.14159265358979323846
#define BLOCK 2048
#define SLOTS 8
typedef struct {int count,rate,selection,channels;float samples[8][BLOCK];} SpecBlock;
struct SpecAnalysis {
 double re[SPEC_FFT],im[SPEC_FFT],window[SPEC_SAMPLES],cosine[SPEC_FFT/2],sine[SPEC_FFT/2],peaks[SPEC_FFT/2+1];
 unsigned reverse[SPEC_FFT];double scale;
};
struct Spectrum {
 volatile LONG requested,selection,running,stream_changed,produced,consumed,frames,dropped;
 HANDLE thread,stop;CRITICAL_SECTION output_lock;SpecFrame view;
 SpecBlock blocks[SLOTS];
 float ring[8][SPEC_SAMPLES];int position,filled,channels,rate,last_selection;
 SpecAnalysis* analysis;float smoothed[SPEC_BINS];
 SpecRegister reg;SpecCommand start,halt,unreg;int registered,failed;
};
static LONG atom(volatile LONG* v){return InterlockedCompareExchange(v,0,0);}
static int packed(int kind,int index,int channel){return kind|(index<<1)|((channel+1)<<5);}
static void unpack(int selection,int* kind,int* index,int* channel){*kind=selection&1;*index=(selection>>1)&15;*channel=((selection>>5)&15)-1;}
static void floor_bins(float* db){for(int i=0;i<SPEC_BINS;i++)db[i]=-120;}
static void publish_status(Spectrum* s,int state,const char* message){
 EnterCriticalSection(&s->output_lock);s->view.state=state;s->view.requested=atom(&s->requested);snprintf(s->view.message,sizeof(s->view.message),"%s",message);
 if(state!=SPEC_ACTIVE){floor_bins(s->view.db);s->view.sequence++;}LeaveCriticalSection(&s->output_lock);
}
SpecAnalysis* spec_analysis_create(void){
 SpecAnalysis* a=calloc(1,sizeof(*a));if(!a)return NULL;double sum=0;
 for(int i=0;i<SPEC_SAMPLES;i++){a->window[i]=.5-.5*cos(2*PI*i/SPEC_SAMPLES);sum+=a->window[i];}a->scale=2/sum;
 for(unsigned i=0;i<SPEC_FFT;i++){unsigned value=i,reversed=0;for(int b=0;b<14;b++){reversed=(reversed<<1)|(value&1);value>>=1;}a->reverse[i]=reversed;}
 for(int i=0;i<SPEC_FFT/2;i++){a->cosine[i]=cos(-2*PI*i/SPEC_FFT);a->sine[i]=sin(-2*PI*i/SPEC_FFT);}return a;
}
void spec_analysis_destroy(SpecAnalysis* a){free(a);}
void spec_analyze(SpecAnalysis* a,const float* samples,int rate,float out[SPEC_BINS]){
 floor_bins(out);if(!a||!samples||rate<8000||rate>384000)return;
 memset(a->re,0,sizeof(a->re));memset(a->im,0,sizeof(a->im));double max=0;
 for(int i=0;i<SPEC_SAMPLES;i++){double v=isfinite(samples[i])?samples[i]:0;if(fabs(v)>max)max=fabs(v);a->re[a->reverse[i]]=v*a->window[i];}
 if(max<1e-9)return;
 for(int len=2;len<=SPEC_FFT;len*=2){int half=len/2,step=SPEC_FFT/len;
  for(int start=0;start<SPEC_FFT;start+=len)for(int j=0;j<half;j++){
   int p=start+j,q=p+half,k=j*step;double re=a->re[q]*a->cosine[k]-a->im[q]*a->sine[k],im=a->re[q]*a->sine[k]+a->im[q]*a->cosine[k];
   a->re[q]=a->re[p]-re;a->im[q]=a->im[p]-im;a->re[p]+=re;a->im[p]+=im;
  }
 }
 for(int k=0;k<=SPEC_FFT/2;k++)a->re[k]=hypot(a->re[k],a->im[k]);
 for(int k=0;k<=SPEC_FFT/2;k++){
  double magnitude=a->re[k];
  if(k>0&&k<SPEC_FFT/2&&magnitude>a->re[k-1]&&magnitude>=a->re[k+1]&&a->re[k-1]>0&&a->re[k+1]>0){
   double l=log(a->re[k-1]),m=log(magnitude),r=log(a->re[k+1]),denom=l-2*m+r;
   if(denom< -1e-12){double offset=fmax(-.5,fmin(.5,.5*(l-r)/denom));magnitude=exp(m-.25*(l-r)*offset);}
  }a->peaks[k]=magnitude*a->scale*(k==0||k==SPEC_FFT/2?.5:1);
 }
 for(int i=0;i<SPEC_BINS;i++){
  double lo=20*pow(1000,i/(double)SPEC_BINS),hi=20*pow(1000,(i+1)/(double)SPEC_BINS);if(lo>=rate*.5)continue;
  int first=(int)floor(lo*SPEC_FFT/rate),last=(int)ceil(hi*SPEC_FFT/rate);if(first<1)first=1;if(last>SPEC_FFT/2)last=SPEC_FFT/2;
  double peak=0;for(int k=first;k<=last;k++)if(a->peaks[k]>peak)peak=a->peaks[k];
  out[i]=(float)fmax(-120,fmin(36,20*log10(fmax(1e-12,peak))));
 }
}
/* MAIN has strips followed by buses in r[], but only buses in w[]. Never copy
   r[0] into w[0]: that would replace a bus with an unrelated input strip. */
int spec_main_passthrough(SpecAudioBuffer* b){
 if(!b||b->nbs<0||b->nbs>65536||b->nbi<0||b->nbi>128||b->nbo<0||b->nbo>128||b->nbi<b->nbo)return 0;
 int offset=b->nbi-b->nbo;
 for(int c=0;c<b->nbo;c++)if(b->w[c]){
  if(b->r[offset+c]){if(b->w[c]!=b->r[offset+c])memmove(b->w[c],b->r[offset+c],b->nbs*sizeof(float));}
  else memset(b->w[c],0,b->nbs*sizeof(float));
 }return 1;
}
int spec_source_offset(const SpecAudioBuffer* b,int kind,int index,int* channels){
 if(!b||!channels||kind<0||kind>1||index<0||index>7||b->nbi<0||b->nbi>128||b->nbo<0||b->nbo>128||b->nbi<b->nbo)return -1;
 int strips=b->nbi-b->nbo;
 if(kind==0){if(index>=5||index*2+2>strips)return -1;*channels=2;return index*2;}
 if(index*8+8>b->nbo)return -1;*channels=8;return strips+index*8;
}
long __stdcall spec_callback(void* user,long command,void* data,long sync){
 (void)sync;Spectrum* s=user;if(!s)return 0;
 if(command==2||command==3){if(atom(&s->running))InterlockedExchange(&s->stream_changed,1);return 0;}
 if(command!=20)return 0;SpecAudioBuffer* b=data;
 if(!spec_main_passthrough(b)){InterlockedExchange(&s->stream_changed,1);return 0;}
 InterlockedIncrement(&s->frames);if(!atom(&s->running)||!atom(&s->requested))return 0;
 int selection=atom(&s->selection),kind,index,channel,channels;unpack(selection,&kind,&index,&channel);
 int offset=spec_source_offset(b,kind,index,&channels);if(offset<0||channel>=channels)return 0;
 if(channel>=0){offset+=channel;channels=1;}
 for(int pos=0;pos<b->nbs;pos+=BLOCK){
  uint32_t written=(uint32_t)atom(&s->produced),read=(uint32_t)atom(&s->consumed);
  if(written-read>=SLOTS){InterlockedIncrement(&s->dropped);break;}
  SpecBlock* block=&s->blocks[written%SLOTS];int count=b->nbs-pos;if(count>BLOCK)count=BLOCK;
  block->count=count;block->rate=b->sr;block->selection=selection;block->channels=channels;
  for(int c=0;c<channels;c++){if(b->r[offset+c])memcpy(block->samples[c],b->r[offset+c]+pos,count*sizeof(float));else memset(block->samples[c],0,count*sizeof(float));}
  InterlockedExchange(&s->produced,(LONG)(written+1));
 }
 return 0;
}
static DWORD WINAPI analysis_worker(void* context){
 Spectrum* s=context;ULONGLONG last=0,last_audio=0;int new_samples=0,last_dropped=0;float samples[SPEC_SAMPLES],bins[SPEC_BINS],maximum[SPEC_BINS];
 /* A per-thread high resolution wait avoids globally changing Windows timer
    resolution. This worker, never the audio callback, owns the timer. */
 HANDLE tick=CreateWaitableTimerExW(NULL,NULL,0x00000002,TIMER_ALL_ACCESS);
 if(!tick)tick=CreateWaitableTimerW(NULL,FALSE,NULL);
 floor_bins(s->smoothed);
 for(;;){
  DWORD wait;
  if(tick&&atom(&s->running)&&atom(&s->requested)){
   LARGE_INTEGER due;due.QuadPart=-40000;HANDLE handles[2]={s->stop,tick};
   if(SetWaitableTimer(tick,&due,0,NULL,NULL,FALSE))wait=WaitForMultipleObjects(2,handles,FALSE,50);
   else wait=WaitForSingleObject(s->stop,4);
  }else wait=WaitForSingleObject(s->stop,50);
  if(wait==WAIT_OBJECT_0||wait==WAIT_FAILED)break;
  uint32_t read=(uint32_t)atom(&s->consumed),write=(uint32_t)atom(&s->produced);int wanted=atom(&s->selection);
  while(read!=write){
   SpecBlock* block=&s->blocks[read%SLOTS];
   if(block->selection==wanted&&atom(&s->running)&&atom(&s->requested)){
    if(s->last_selection!=block->selection||s->rate!=block->rate||s->channels!=block->channels||last_dropped!=atom(&s->dropped)){
     s->position=s->filled=new_samples=0;s->last_selection=block->selection;s->rate=block->rate;s->channels=block->channels;last_dropped=atom(&s->dropped);floor_bins(s->smoothed);
    }
    for(int i=0;i<block->count;i++){for(int c=0;c<s->channels;c++)s->ring[c][s->position]=block->samples[c][i];s->position=(s->position+1)%SPEC_SAMPLES;if(s->filled<SPEC_SAMPLES)s->filled++;}
    new_samples+=block->count;last_audio=GetTickCount64();
   }
   InterlockedExchange(&s->consumed,(LONG)++read);write=(uint32_t)atom(&s->produced);
  }
  ULONGLONG now=GetTickCount64();
  if(!atom(&s->running)||!atom(&s->requested)){s->filled=0;s->last_selection=-1;continue;}
  if(last_audio&&now-last_audio>500){publish_status(s,SPEC_WAITING,"No callback audio frames - check VoiceMeeter's audio engine");s->filled=0;last_audio=0;continue;}
  if(s->filled<SPEC_SAMPLES||new_samples<512||now-last<16)continue;
  double elapsed=last?(now-last)/1000.:.016;last=now;new_samples=0;floor_bins(maximum);
  for(int c=0;c<s->channels;c++){
   for(int i=0;i<SPEC_SAMPLES;i++)samples[i]=s->ring[c][(s->position+i)%SPEC_SAMPLES];
   spec_analyze(s->analysis,samples,s->rate,bins);
   for(int b=0;b<SPEC_BINS;b++)if(bins[b]>maximum[b])maximum[b]=bins[b];
  }
  if(wanted!=atom(&s->selection)||!atom(&s->running)||!atom(&s->requested))continue;
  int kind,index,channel;unpack(wanted,&kind,&index,&channel);
  EnterCriticalSection(&s->output_lock);
  if(wanted!=atom(&s->selection)||!atom(&s->running)||!atom(&s->requested)){LeaveCriticalSection(&s->output_lock);continue;}
  for(int b=0;b<SPEC_BINS;b++){double decay=exp(-elapsed/.25);s->smoothed[b]=maximum[b]>=s->smoothed[b]?maximum[b]:(float)(maximum[b]+(s->smoothed[b]-maximum[b])*decay);s->view.db[b]=s->smoothed[b];}
  s->view.state=SPEC_ACTIVE;s->view.requested=1;s->view.sample_rate=s->rate;s->view.kind=kind;s->view.index=index;s->view.channel=channel;s->view.sequence++;s->view.audio_frames=atom(&s->frames);
  snprintf(s->view.message,sizeof(s->view.message),"%s | %d Hz | %s | peak dBFS",kind?"MAIN bus: post-master":"MAIN strip: after input insert (not post-EQ)",s->rate,channel<0?"channel maximum":"selected channel");
  LeaveCriticalSection(&s->output_lock);
 }if(tick)CloseHandle(tick);return 0;
}
Spectrum* spec_create(void){
 Spectrum* s=calloc(1,sizeof(*s));if(!s)return NULL;s->selection=packed(0,0,-1);s->last_selection=-1;InitializeCriticalSection(&s->output_lock);
 s->analysis=spec_analysis_create();s->stop=CreateEventW(NULL,TRUE,FALSE,NULL);floor_bins(s->view.db);strcpy_s(s->view.message,sizeof(s->view.message),"Spectrum off - callback released");
 if(s->analysis&&s->stop)s->thread=CreateThread(NULL,0,analysis_worker,s,0,NULL);
 if(!s->thread){if(s->stop)CloseHandle(s->stop);spec_analysis_destroy(s->analysis);DeleteCriticalSection(&s->output_lock);free(s);return NULL;}return s;
}
void spec_request(Spectrum* s,int enabled,int kind,int index,int channel){
 if(!s||kind<0||kind>1||index<0||index>7||channel< -1||channel>7)return;
 int selection=packed(kind,index,channel);LONG previous=atom(&s->selection);
 if((previous&511)!=selection||(enabled&&!atom(&s->requested))){
  selection=(int)(((uint32_t)previous+512u)&~511u)|selection;InterlockedExchange(&s->selection,selection);
  EnterCriticalSection(&s->output_lock);floor_bins(s->view.db);s->view.state=enabled?SPEC_WAITING:SPEC_OFF;s->view.sequence++;LeaveCriticalSection(&s->output_lock);
 }
 InterlockedExchange(&s->requested,enabled?1:0);
}
void spec_read(Spectrum* s,SpecFrame* frame){
 if(!s){memset(frame,0,sizeof(*frame));floor_bins(frame->db);strcpy_s(frame->message,sizeof(frame->message),"Spectrum unavailable");return;}
 EnterCriticalSection(&s->output_lock);*frame=s->view;LeaveCriticalSection(&s->output_lock);frame->requested=atom(&s->requested);
}
int spec_detach(Spectrum* s){
 if(!s)return 1;InterlockedExchange(&s->running,0);
 if(s->registered){s->halt();long result=s->unreg();if(result<0){publish_status(s,SPEC_ERROR,"Callback release failed; close EQ before starting another callback app");return 0;}s->registered=0;}
 InterlockedExchange(&s->stream_changed,0);return 1;
}
void spec_bind(Spectrum* s,SpecRegister reg,SpecCommand start,SpecCommand halt,SpecCommand unreg){s->reg=reg;s->start=start;s->halt=halt;s->unreg=unreg;}
void spec_manage(Spectrum* s,HMODULE library,int connected){
 if(!s)return;
 if(!connected){InterlockedExchange(&s->requested,0);if(spec_detach(s))publish_status(s,SPEC_OFF,"Spectrum off - reconnect then enable manually");return;}
 if(!atom(&s->requested)){if(!spec_detach(s))return;s->failed=0;SpecFrame view;spec_read(s,&view);if(view.state!=SPEC_OFF||strcmp(view.message,"Spectrum off - callback released"))publish_status(s,SPEC_OFF,"Spectrum off - callback released");return;}
 if(atom(&s->stream_changed)){InterlockedExchange(&s->requested,0);spec_detach(s);publish_status(s,SPEC_ERROR,"Audio stream changed. Enable spectrum again when the engine is stable.");return;}
 if(s->registered||s->failed)return;
 #define LOAD(member,name) if(!s->member&&library) *(FARPROC*)&s->member=GetProcAddress(library,name)
 LOAD(reg,"VBVMR_AudioCallbackRegister");LOAD(start,"VBVMR_AudioCallbackStart");LOAD(halt,"VBVMR_AudioCallbackStop");LOAD(unreg,"VBVMR_AudioCallbackUnregister");
 #undef LOAD
 if(!s->reg||!s->start||!s->halt||!s->unreg){s->failed=1;publish_status(s,SPEC_ERROR,"Installed API does not support audio callbacks");return;}
 char name[64]="Elka VM EQ Spectrum";publish_status(s,SPEC_STARTING,"Requesting MAIN callback...");long result=s->reg(4,spec_callback,s,name);
 if(result!=0){char text[160];name[63]=0;s->failed=1;if(result==1)snprintf(text,sizeof(text),"Callback busy: %.63s. No callback was taken over.",name);else snprintf(text,sizeof(text),"Callback registration failed (%ld)",result);publish_status(s,result==1?SPEC_BUSY:SPEC_ERROR,text);return;}
 s->registered=1;InterlockedExchange(&s->running,1);result=s->start();
 if(result!=0){InterlockedExchange(&s->running,0);spec_detach(s);s->failed=1;publish_status(s,SPEC_ERROR,"Callback start failed. Turn spectrum off before retrying.");return;}
 publish_status(s,SPEC_WAITING,"Waiting for callback audio...");
}
int spec_destroy(Spectrum* s){
 if(!s)return 1;InterlockedExchange(&s->requested,0);
 if(!spec_detach(s)){
  /* Never unload code or free callback state if a driver refuses release. */
  HMODULE pin;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,(LPCWSTR)(void*)spec_callback,&pin);return 0;
 }
 SetEvent(s->stop);WaitForSingleObject(s->thread,INFINITE);CloseHandle(s->thread);CloseHandle(s->stop);spec_analysis_destroy(s->analysis);DeleteCriticalSection(&s->output_lock);free(s);return 1;
}

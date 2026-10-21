#include "eq_bridge.h"
#include "vm_api.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
typedef struct { VmApi vm; int demo; EqSnapshot memories[2][8][2]; } Engine;
int __cdecl ElkaEq_AbiVersion(void){return 1;}
int __cdecl ElkaEq_FrameSize(void){return (int)sizeof(EqSnapshot);}
int __cdecl ElkaEq_LevelsSize(void){return (int)sizeof(EqLevels);}
int __cdecl ElkaEq_ReadLevels(EqEngine engine,EqLevels* frame,int bytes){
 if(!engine||!frame||bytes!=sizeof(*frame))return 0;vm_read_levels(&((Engine*)engine)->vm,frame);return 1;
}
int __cdecl ElkaEq_SpectrumFrameSize(void){return (int)sizeof(SpecFrame);}
int __cdecl ElkaEq_SpectrumRequest(EqEngine engine,int enabled,int kind,int index,int channel){
 Engine* e=engine;if(!e||e->demo||!e->vm.spectrum)return 0;
 if(enabled){EqSnapshot s;vm_snapshot(&e->vm,&s);int count=kind==0?(s.edition==3?5:s.edition==2?3:2):(s.edition==3?8:s.edition==2?5:2);
  if(s.edition<1||kind<0||kind>1||index<0||index>=count||channel< -1||channel>=(kind?8:2))return 0;}
 spec_request(e->vm.spectrum,enabled,kind,index,channel);return 1;
}
int __cdecl ElkaEq_SpectrumRead(EqEngine engine,SpecFrame* frame,int bytes){
 if(!engine||!frame||bytes!=sizeof(*frame))return 0;spec_read(((Engine*)engine)->vm.spectrum,frame);return 1;
}
EqEngine __cdecl ElkaEq_Create(int demo){
 Engine* e=calloc(1,sizeof(*e));if(!e)return NULL;e->demo=demo!=0;
 if(!demo){if(!vm_start_ex(&e->vm,1)){free(e);return NULL;}return e;}
 InitializeCriticalSection(&e->vm.lock);
 for(int k=0;k<2;k++)for(int i=0;i<8;i++)for(int a=0;a<2;a++){
  EqSnapshot* s=&e->memories[k][i][a];eq_defaults(s);s->edition=3;s->kind=k;s->index=i;s->bank=a;
  s->channels=k==EQ_STRIP?2:8;s->ready=s->enabled=1;
  for(int c=0;c<s->channels;c++){s->bands[c][0].v[EQ_GAIN]=6;s->bands[c][0].v[EQ_FREQ]=90;s->bands[c][0].v[EQ_Q]=.8f;
   s->bands[c][2].v[EQ_GAIN]=-4;s->bands[c][2].v[EQ_Q]=1.2f;s->bands[c][4].v[EQ_GAIN]=5;s->bands[c][4].v[EQ_Q]=.7f;}
  strcpy_s(s->status,sizeof(s->status),"Demo - no VoiceMeeter connection");
 }e->vm.view=e->memories[0][0][0];return e;
}
void __cdecl ElkaEq_Destroy(EqEngine engine){Engine* e=engine;if(!e)return;if(e->demo)DeleteCriticalSection(&e->vm.lock);else vm_stop(&e->vm);free(e);}
int __cdecl ElkaEq_Read(EqEngine engine,EqSnapshot* frame,int bytes){if(!engine||!frame||bytes!=sizeof(*frame))return 0;vm_snapshot(&((Engine*)engine)->vm,frame);return 1;}
static void demo_save(Engine* e){if(e->demo){EqSnapshot* s=&e->vm.view;e->memories[s->kind][s->index][s->bank]=*s;e->vm.count=0;}}
int __cdecl ElkaEq_Select(EqEngine engine,int kind,int index){
 Engine* e=engine;if(!e||kind<0||kind>1)return 0;EqSnapshot s;vm_snapshot(&e->vm,&s);
 if(index<0||index>=eq_targets(s.edition,kind))return 0;
 if(e->demo){demo_save(e);e->vm.view=e->memories[kind][index][0];}else vm_select(&e->vm,kind,index);return 1;
}
int __cdecl ElkaEq_Edit(EqEngine engine,int channel,int band,int field,double value){
 Engine* e=engine;if(!e)return 0;int ok=vm_edit(&e->vm,channel,band,field,value);demo_save(e);return ok;
}
int __cdecl ElkaEq_Enabled(EqEngine engine,int enabled){Engine* e=engine;if(!e)return 0;int ok=vm_enabled(&e->vm,enabled);demo_save(e);return ok;}
int __cdecl ElkaEq_Bank(EqEngine engine,int bank){
 Engine* e=engine;if(!e||bank<0||bank>1)return 0;
 if(e->demo){demo_save(e);EqSnapshot s=e->vm.view;e->vm.view=e->memories[s.kind][s.index][bank];return 1;}return vm_bank(&e->vm,bank);
}
void __cdecl ElkaEq_Dragging(EqEngine engine,int dragging){if(engine)vm_dragging(&((Engine*)engine)->vm,dragging);}
int __cdecl ElkaEq_Apply(EqEngine engine,int channel,const float* values,int source_channels,int count,int only_band){
 Engine* e=engine;if(!e||!values||source_channels<1||source_channels>8||count!=source_channels*30)return 0;
 EqBand copy[8][6]={0};memcpy(copy,values,count*sizeof(float));int ok=vm_apply(&e->vm,channel,copy,source_channels,only_band);demo_save(e);return ok;
}
int __cdecl ElkaEq_Defaults(EqSnapshot* frame,int bytes){if(!frame||bytes!=sizeof(*frame))return 0;eq_defaults(frame);return 1;}
int __cdecl ElkaEq_TargetCount(int edition,int kind){return eq_targets(edition,kind);}
double __cdecl ElkaEq_Limit(int edition,int kind,int field,double value){return eq_limit(edition,kind,field,value);}
int __cdecl ElkaEq_Curve(const EqSnapshot* frame,int bytes,int channel,int solo,int count,double* output){
 if(!frame||!output||bytes!=sizeof(*frame)||frame->channels<1||frame->channels>8||channel<0||channel>=frame->channels||solo< -1||solo>=6||count<2||count>4096||!isfinite(frame->sample_rate))return 0;
 for(int b=0;b<6;b++)for(int f=0;f<5;f++)if(!isfinite(frame->bands[channel][b].v[f]))return 0;
 for(int i=0;i<count;i++)output[i]=eq_sum(frame,channel,solo,eq_plot_frequency(i/(double)(count-1)));
 return 1;
}

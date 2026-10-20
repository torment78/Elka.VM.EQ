#include "vm_api.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
static int load_api(VmApi* v){
 wchar_t path[MAX_PATH],base[MAX_PATH];HKEY key;DWORD size=sizeof(base);
 const wchar_t* reg=L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\VB:Voicemeeter {17359A74-1236-5467}";
 if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,reg,0,KEY_READ|KEY_WOW64_32KEY,&key)==ERROR_SUCCESS){
  if(RegQueryValueExW(key,L"UninstallString",NULL,NULL,(BYTE*)base,&size)==ERROR_SUCCESS){
   base[MAX_PATH-1]=0;wchar_t* start=base;if(*start==L'"')start++;
   wchar_t* last=wcsrchr(start,L'\\');if(last){*last=0;swprintf_s(path,MAX_PATH,L"%s\\VoicemeeterRemote64.dll",start);
    v->library=LoadLibraryExW(path,NULL,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);}
  }RegCloseKey(key);
 }
 if(!v->library){
  const wchar_t* vars[]={L"ProgramFiles(x86)",L"ProgramFiles"};
  for(int i=0;i<2&&!v->library;i++)if(GetEnvironmentVariableW(vars[i],base,MAX_PATH)){
   swprintf_s(path,MAX_PATH,L"%s\\VB\\Voicemeeter\\VoicemeeterRemote64.dll",base);
   v->library=LoadLibraryExW(path,NULL,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
  }
 }
 if(!v->library)return 0;
 #define FN(member,name) *(FARPROC*)&v->member=GetProcAddress(v->library,name)
 FN(login,"VBVMR_Login");FN(logout,"VBVMR_Logout");FN(dirty,"VBVMR_IsParametersDirty");
 FN(type,"VBVMR_GetVoicemeeterType");FN(version,"VBVMR_GetVoicemeeterVersion");FN(get,"VBVMR_GetParameterFloat");FN(set,"VBVMR_SetParameterFloat");
 #undef FN
 if(v->login&&v->logout&&v->dirty&&v->type&&v->version&&v->get&&v->set)return 1;
 FreeLibrary(v->library);v->library=NULL;return 0;
}
static void status(VmApi* v,const char* text,int reset){
 EnterCriticalSection(&v->lock);v->view.ready=0;snprintf(v->view.status,sizeof(v->view.status),"%s",text);
 if(reset){v->count=0;v->view.edition=0;v->generation++;}LeaveCriticalSection(&v->lock);
}
static int read_snapshot(VmApi* v,EqSnapshot* s){
 char key[128];float val;const char* prefix=s->kind==EQ_STRIP?"Strip":"Bus";
 snprintf(key,sizeof(key),"%s[%d].EQ.on",prefix,s->index);
 if(v->get(key,&val)!=0||!isfinite(val))return 0;s->enabled=val>.5;
 snprintf(key,sizeof(key),"%s[%d].EQ.AB",prefix,s->index);
 if(v->get(key,&val)!=0||!isfinite(val))return 0;s->bank=(int)val;if(s->bank<0||s->bank>1)return 0;
 s->channels=s->kind==EQ_STRIP?2:8;
 for(int c=0;c<s->channels;c++)for(int b=0;b<6;b++)for(int f=0;f<5;f++){
  if(!eq_key(key,sizeof(key),s->edition,s->kind,s->index,c,b,f)||v->get(key,&val)!=0||!isfinite(val))return 0;
  if((f==EQ_FREQ&&(val<20||val>20000))||(f==EQ_Q&&(val<.299||val>100))||
    (f==EQ_GAIN&&(val< -36||val>24))||(f==EQ_TYPE&&(val<0||val>6||val!=(int)val))||
    (f==EQ_ON&&val!=0&&val!=1))return 0;
  s->bands[c][b].v[f]=val;
 }
 // Reject a torn snapshot if native A/B changed during the read.
 snprintf(key,sizeof(key),"%s[%d].EQ.AB",prefix,s->index);
 if(v->get(key,&val)!=0||(int)val!=s->bank)return 0;
 if(v->get("Option.sr",&val)==0&&val>0)s->sample_rate=val<1000?val*1000:val;
 return 1;
}
static DWORD WINAPI worker(void* parameter){
 VmApi* v=parameter;ULONGLONG last_read=0;
 while(WaitForSingleObject(v->stop,15)==WAIT_TIMEOUT){
  if(!v->library&&!load_api(v)){status(v,"VoiceMeeter Remote API not found. Install VoiceMeeter x64.",1);WaitForSingleObject(v->stop,1000);continue;}
  if(!v->logged){if(v->login()<0){status(v,"VoiceMeeter login unavailable; retrying...",1);WaitForSingleObject(v->stop,1000);continue;}v->logged=1;}
  long edition=0,version=0;
  if(v->dirty()<0||v->type(&edition)<0||edition<1||edition>3){
   spec_manage(v->spectrum,v->library,0);status(v,"VoiceMeeter disconnected. Pending edits discarded.",1);v->logout();v->logged=0;WaitForSingleObject(v->stop,500);continue;
  }v->version(&version);
  spec_manage(v->spectrum,v->library,1);
  EqWrite pending[1024];int count,failed=0;EqSnapshot snap;unsigned gen;
  EnterCriticalSection(&v->lock);
  if(v->view.edition!=edition){v->view.edition=edition;v->view.ready=0;v->count=0;v->generation++;}
  if(v->view.index>=eq_targets(edition,v->view.kind)){v->view.index=0;v->view.ready=0;v->generation++;}
  v->view.version=version;count=v->count;memcpy(pending,v->queue,count*sizeof(EqWrite));v->count=0;
  snap=v->view;gen=v->generation;LeaveCriticalSection(&v->lock);
  for(int i=0;i<count;i++){
   EqWrite* w=&pending[i];char key[128],abkey[80];float bank;
   if(w->edition!=edition||!eq_supported(edition,w->kind)){failed=1;break;}
   snprintf(abkey,sizeof(abkey),"%s[%d].EQ.AB",w->kind==EQ_STRIP?"Strip":"Bus",w->index);
   if(v->get(abkey,&bank)!=0||(int)bank!=w->bank){failed=1;break;}
   if(w->field==100)snprintf(key,sizeof(key),"%s[%d].EQ.on",w->kind==EQ_STRIP?"Strip":"Bus",w->index);
   else if(w->field==101)snprintf(key,sizeof(key),"%s",abkey);
   else if(!eq_key(key,sizeof(key),edition,w->kind,w->index,w->channel,w->band,w->field)){failed=1;break;}
   if(v->set(key,w->value)!=0){failed=1;break;}
   if(w->field==101){WaitForSingleObject(v->stop,120);last_read=0;}
  }
  if(failed){
   status(v,"EQ write rejected or A/B changed. Readback restored; retry your edit.",0);
   EnterCriticalSection(&v->lock);v->count=0;v->edit_until=GetTickCount64()+700;LeaveCriticalSection(&v->lock);last_read=0;
  }
  if(GetTickCount64()-last_read<100)continue;last_read=GetTickCount64();
  if(!eq_supported(edition,snap.kind)){status(v,snap.kind==EQ_STRIP?"Six-band input EQ requires Potato. Select Bus on Banana.":"Six-band bus EQ requires Banana or Potato.",0);continue;}
  snap.ready=read_snapshot(v,&snap);
  snprintf(snap.status,sizeof(snap.status),"%s",snap.ready?"Connected | API controls | Calculated response preview":"EQ parameters unavailable in this VoiceMeeter version.");
  EnterCriticalSection(&v->lock);
  if(v->generation==gen&&!v->count&&!v->dragging&&GetTickCount64()>=v->edit_until)v->view=snap;
  LeaveCriticalSection(&v->lock);
 }
 int released=spec_destroy(v->spectrum);v->spectrum=NULL;
 if(released){if(v->logged)v->logout();if(v->library)FreeLibrary(v->library);}return 0;
}
int vm_start(VmApi* v){return vm_start_ex(v,0);}
int vm_start_ex(VmApi* v,int analyzer){
 memset(v,0,sizeof(*v));InitializeCriticalSection(&v->lock);eq_defaults(&v->view);
 strcpy_s(v->view.status,sizeof(v->view.status),"Connecting to VoiceMeeter...");
 v->stop=CreateEventW(NULL,TRUE,FALSE,NULL);if(!v->stop){DeleteCriticalSection(&v->lock);return 0;}
 if(analyzer)v->spectrum=spec_create();
 v->thread=CreateThread(NULL,0,worker,v,0,NULL);
 if(!v->thread){spec_destroy(v->spectrum);CloseHandle(v->stop);DeleteCriticalSection(&v->lock);return 0;}return 1;
}
void vm_stop(VmApi* v){SetEvent(v->stop);WaitForSingleObject(v->thread,INFINITE);CloseHandle(v->thread);CloseHandle(v->stop);DeleteCriticalSection(&v->lock);}
void vm_snapshot(VmApi* v,EqSnapshot* out){EnterCriticalSection(&v->lock);*out=v->view;LeaveCriticalSection(&v->lock);}
void vm_select(VmApi* v,int kind,int index){
 EnterCriticalSection(&v->lock);v->view.kind=kind;v->view.index=index;v->view.ready=0;v->view.channels=kind==EQ_STRIP?2:8;
 v->generation++;v->edit_until=0;v->dragging=0;strcpy_s(v->view.status,sizeof(v->view.status),"Reading selected EQ...");
 LeaveCriticalSection(&v->lock);
}
void vm_dragging(VmApi* v,int enabled){EnterCriticalSection(&v->lock);v->dragging=enabled;LeaveCriticalSection(&v->lock);}
static int enqueue(VmApi* v,int c,int b,int f,float value){
 EqSnapshot* s=&v->view;EqWrite w={s->kind,s->index,s->edition,s->bank,c,b,f,value};
 for(int i=v->count-1;i>=0;i--){EqWrite* p=&v->queue[i];if(p->field==101)break;
  if(p->kind==w.kind&&p->index==w.index&&p->bank==w.bank&&p->channel==c&&p->band==b&&p->field==f){*p=w;return 1;}}
 if(v->count>=1024)return 0;v->queue[v->count++]=w;return 1;
}
int vm_edit(VmApi* v,int channel,int band,int field,double value){
 int ok=0;EnterCriticalSection(&v->lock);EqSnapshot* s=&v->view;value=eq_limit(s->edition,s->kind,field,value);
 if(s->ready&&isfinite(value)&&channel>=-1&&channel<s->channels&&band>=0&&band<6&&v->count<1000){
  for(int c=0;c<s->channels;c++)if(channel<0||c==channel){
   EqBand* b=&s->bands[c][band];if(field==EQ_GAIN&&b->v[EQ_TYPE]>=1&&b->v[EQ_TYPE]<=4)continue;
   enqueue(v,c,band,field,(float)value);b->v[field]=(float)value;
   if(field==EQ_TYPE&&value>=1&&value<=4){enqueue(v,c,band,EQ_GAIN,0);b->v[EQ_GAIN]=0;}
  }v->generation++;v->edit_until=GetTickCount64()+500;ok=1;
 }LeaveCriticalSection(&v->lock);return ok;
}
static int master(VmApi* v,int field,int value){
 int ok=0;EnterCriticalSection(&v->lock);
 if(v->view.ready&&enqueue(v,0,0,field,(float)value)){
  if(field==101)v->view.ready=0;else v->view.enabled=value;
  v->generation++;v->edit_until=GetTickCount64()+300;ok=1;
 }LeaveCriticalSection(&v->lock);return ok;
}
int vm_enabled(VmApi* v,int enabled){return master(v,100,enabled?1:0);}
int vm_bank(VmApi* v,int bank){return bank>=0&&bank<=1?master(v,101,bank):0;}
int vm_apply(VmApi* v,int channel,const EqBand data[8][6],int source_channels,int only_band){
 if(!data||only_band< -1||only_band>=6||source_channels<1||source_channels>8)return 0;
 for(int c=0;c<source_channels;c++)for(int b=0;b<6;b++)for(int f=0;f<5;f++)if(!isfinite(data[c][b].v[f]))return 0;
 int ok=0;EnterCriticalSection(&v->lock);EqSnapshot* s=&v->view;
 if(s->ready&&channel>=-1&&channel<s->channels&&(source_channels==1||source_channels==s->channels)&&v->count<700){
  for(int c=0;c<s->channels;c++)if(channel<0||channel==c)for(int b=0;b<6;b++)if(only_band<0||only_band==b){
   EqBand band=data[source_channels==1?0:c][b];
   for(int f=0;f<5;f++)band.v[f]=(float)eq_limit(s->edition,s->kind,f,band.v[f]);
   if(band.v[EQ_TYPE]>=1&&band.v[EQ_TYPE]<=4)band.v[EQ_GAIN]=0;
   for(int f=0;f<5;f++)enqueue(v,c,b,f,band.v[f]);s->bands[c][b]=band;
  }v->generation++;v->edit_until=GetTickCount64()+700;ok=1;
 }LeaveCriticalSection(&v->lock);return ok;
}

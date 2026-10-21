#include "vm_api.h"
#include <stdio.h>
#include <math.h>
static int checks,failed;
static int level_calls,level_type,level_first,level_error,level_race;static VmApi* level_vm;
static long __stdcall fake_level(long type,long channel,float* value){
 if(!level_calls)level_first=(int)channel;level_type=(int)type;level_calls++;
 if(level_race){level_race=0;vm_select(level_vm,EQ_STRIP,1);}
 *value=(float)(channel+1)/100;return level_error?-3:0;
}
#define CHECK(x) do{checks++;if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);failed++;}}while(0)
int main(void){
 VmApi v={0};InitializeCriticalSection(&v.lock);eq_defaults(&v.view);
 v.view.edition=3;v.view.ready=1;
 CHECK(vm_edit(&v,-1,0,EQ_GAIN,5));CHECK(v.count==2);CHECK(v.view.bands[1][0].v[EQ_GAIN]==5);
 CHECK(vm_edit(&v,-1,0,EQ_GAIN,6));CHECK(v.count==2);CHECK(v.queue[0].value==6);
 CHECK(!vm_edit(&v,-1,0,EQ_GAIN,NAN));CHECK(!vm_edit(&v,2,0,EQ_GAIN,0));CHECK(!vm_edit(&v,0,6,EQ_GAIN,0));CHECK(!vm_edit(&v,0,0,90,0));
 CHECK(v.count==2);
 CHECK(vm_edit(&v,-1,0,EQ_TYPE,4));CHECK(v.view.bands[0][0].v[EQ_GAIN]==0);CHECK(v.count==4);
 vm_edit(&v,-1,0,EQ_GAIN,12);CHECK(v.view.bands[0][0].v[EQ_GAIN]==0);
 vm_select(&v,EQ_BUS,7);CHECK(v.count==4);CHECK(v.queue[0].kind==EQ_STRIP);CHECK(!v.view.ready);CHECK(!vm_edit(&v,0,0,EQ_FREQ,900));
 v.count=0;v.view.ready=1;v.view.bank=1;
 CHECK(vm_edit(&v,-1,1,EQ_GAIN,3));CHECK(v.count==8);
 for(int i=0;i<8;i++){CHECK(v.queue[i].kind==EQ_BUS);CHECK(v.queue[i].index==7);CHECK(v.queue[i].channel==i);CHECK(v.queue[i].bank==1);}
 CHECK(vm_bank(&v,0));CHECK(!v.view.ready);CHECK(v.queue[8].field==101);CHECK(v.queue[8].bank==1);
 v.count=0;v.view.ready=1;EqSnapshot defaults;eq_defaults(&defaults);
 CHECK(vm_apply(&v,-1,defaults.bands,1,-1));CHECK(v.count==240);
 defaults.bands[0][0].v[EQ_GAIN]=NAN;CHECK(!vm_apply(&v,-1,defaults.bands,1,-1));CHECK(v.count==240);
 CHECK(!vm_apply(&v,-1,defaults.bands,1,6));CHECK(!vm_apply(&v,-1,defaults.bands,2,-1));
 v.count=0;v.get_level=fake_level;level_vm=&v;EqLevels levels;
 CHECK(sizeof(EqLevels)==60);
 vm_select(&v,EQ_STRIP,4);v.dragging=1;v.edit_until=GetTickCount64()+5000;vm_poll_levels(&v);vm_read_levels(&v,&levels);
 CHECK(level_type==0&&level_first==8&&level_calls==2);CHECK(levels.valid_mask==3&&levels.channels==2&&levels.index==4);CHECK(levels.values[1]==.10f);CHECK(levels.age_ms<500);CHECK(v.count==0);
 vm_select(&v,EQ_BUS,7);vm_read_levels(&v,&levels);CHECK(levels.valid_mask==0);level_calls=0;vm_poll_levels(&v);vm_read_levels(&v,&levels);
 CHECK(level_type==3&&level_first==56&&level_calls==8);CHECK(levels.valid_mask==255&&levels.channels==8);CHECK(levels.values[7]==.64f);
 v.level_time=GetTickCount64()-1000;vm_read_levels(&v,&levels);CHECK(levels.age_ms>=1000);
 level_error=1;vm_poll_levels(&v);vm_read_levels(&v,&levels);CHECK(levels.valid_mask==0);level_error=0;
 level_race=1;vm_poll_levels(&v);vm_read_levels(&v,&levels);CHECK(levels.valid_mask==0);CHECK(v.view.kind==EQ_STRIP&&v.view.index==1);
 level_calls=0;vm_poll_levels(&v);vm_read_levels(&v,&levels);CHECK(level_first==2&&levels.index==1&&levels.valid_mask==3);
 v.get_level=NULL;vm_poll_levels(&v);vm_read_levels(&v,&levels);CHECK(levels.valid_mask==0);CHECK(v.count==0);
 DeleteCriticalSection(&v.lock);printf("%d checks, %d failures\n",checks,failed);return failed?1:0;
}

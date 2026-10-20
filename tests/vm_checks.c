#include "vm_api.h"
#include <stdio.h>
#include <math.h>
static int checks,failed;
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
 DeleteCriticalSection(&v.lock);printf("%d checks, %d failures\n",checks,failed);return failed?1:0;
}

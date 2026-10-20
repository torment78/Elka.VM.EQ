#include "eq_bridge.h"
#include <stdio.h>
#include <math.h>
#include <stddef.h>
static int checks,failed;
#define CHECK(x) do{checks++;if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);failed++;}}while(0)
int main(void){
 CHECK(ElkaEq_AbiVersion()==1);CHECK(ElkaEq_FrameSize()==1200);CHECK(offsetof(EqSnapshot,bands)==40);CHECK(offsetof(EqSnapshot,status)==1000);
 EqEngine e=ElkaEq_Create(1);CHECK(e!=NULL);EqSnapshot s;
 CHECK(!ElkaEq_Read(e,&s,1));CHECK(ElkaEq_Read(e,&s,sizeof(s)));CHECK(s.ready&&s.edition==3);
 CHECK(ElkaEq_Edit(e,-1,2,EQ_GAIN,8));CHECK(ElkaEq_Bank(e,1));CHECK(ElkaEq_Read(e,&s,sizeof(s)));CHECK(s.bands[0][2].v[EQ_GAIN]==-4);
 CHECK(ElkaEq_Bank(e,0));CHECK(ElkaEq_Read(e,&s,sizeof(s)));CHECK(s.bands[1][2].v[EQ_GAIN]==8);
 CHECK(ElkaEq_Select(e,EQ_BUS,7));CHECK(ElkaEq_Read(e,&s,sizeof(s)));CHECK(s.channels==8);
 CHECK(!ElkaEq_Select(e,EQ_BUS,8));CHECK(!ElkaEq_Select(e,9,0));
 double curve[1024];CHECK(ElkaEq_Curve(&s,sizeof(s),7,-1,1024,curve));for(int i=0;i<1024;i++)CHECK(isfinite(curve[i]));
 CHECK(!ElkaEq_Curve(&s,sizeof(s),8,-1,1024,curve));CHECK(!ElkaEq_Curve(&s,sizeof(s),0,-1,99999,curve));
 CHECK(ElkaEq_Defaults(&s,sizeof(s)));CHECK(!ElkaEq_Apply(e,-1,(float*)s.bands,1,240,-1));CHECK(ElkaEq_Apply(e,-1,(float*)s.bands,1,30,-1));
 CHECK(ElkaEq_Read(e,&s,sizeof(s)));CHECK(s.bands[7][2].v[EQ_GAIN]==0);
 CHECK(!ElkaEq_Edit(e,0,0,EQ_GAIN,NAN));ElkaEq_Destroy(e);ElkaEq_Destroy(NULL);
 printf("%d checks, %d failures\n",checks,failed);return failed?1:0;
}

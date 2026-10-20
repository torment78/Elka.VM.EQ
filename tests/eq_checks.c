#include "eq.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int checks,failed;
#define CHECK(x) do{checks++;if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);failed++;}}while(0)
#define NEAR(x,y) CHECK(fabs((x)-(y))<.0001)
int main(void){
 char name[80];EqSnapshot s;eq_defaults(&s);
 CHECK(eq_targets(2,EQ_STRIP)==3);CHECK(eq_targets(3,EQ_STRIP)==5);
 CHECK(eq_targets(2,EQ_BUS)==5);CHECK(eq_targets(3,EQ_BUS)==8);
 CHECK(!eq_supported(2,EQ_STRIP));CHECK(eq_supported(3,EQ_STRIP));CHECK(!eq_supported(1,EQ_BUS));
 eq_target_name(2,EQ_BUS,4,name,sizeof(name));CHECK(!strcmp(name,"B2"));
 eq_target_name(3,EQ_BUS,7,name,sizeof(name));CHECK(!strcmp(name,"B3"));
 CHECK(eq_key(name,sizeof(name),3,EQ_BUS,7,7,5,EQ_GAIN));CHECK(!strcmp(name,"Bus[7].EQ.channel[7].cell[5].gain"));
 CHECK(!eq_key(name,sizeof(name),2,EQ_STRIP,0,0,0,0));CHECK(!eq_key(name,sizeof(name),3,EQ_STRIP,5,0,0,0));
 CHECK(!eq_key(name,sizeof(name),3,EQ_STRIP,0,2,0,0));CHECK(!eq_key(name,sizeof(name),3,EQ_BUS,0,8,0,0));
 CHECK(!eq_key(name,4,3,EQ_BUS,0,0,0,0));CHECK(!eq_key(name,sizeof(name),3,EQ_BUS,0,0,6,0));
 NEAR(eq_plot_x(20),0);NEAR(eq_plot_x(20000),1);NEAR(eq_plot_y(24),0);NEAR(eq_plot_y(-54),1);NEAR(eq_plot_y(0),24./78);
 for(int db=-54;db<=24;db++)NEAR(eq_plot_gain(eq_plot_y(db)),db);
 for(int f=20;f<=20000;f+=111)NEAR(eq_plot_frequency(eq_plot_x(f)),f);
 EqBand b={{1,0,1000,12,3}};NEAR(eq_response(&b,1000,48000),12);
 b.v[EQ_ON]=0;NEAR(eq_response(&b,1000,48000),0);b.v[EQ_ON]=1;
 b.v[EQ_TYPE]=3;CHECK(eq_response(&b,10000,48000)<-30);
 b.v[EQ_TYPE]=4;CHECK(eq_response(&b,20,48000)<-60);
 b.v[EQ_TYPE]=1;CHECK(eq_response(&b,1000,48000)<-100);
 for(int t=0;t<7;t++)for(int sr=32000;sr<=192000;sr+=16000)for(int f=20;f<=20000;f+=127){b.v[EQ_TYPE]=(float)t;CHECK(isfinite(eq_response(&b,f,sr)));}
 NEAR(eq_limit(2,EQ_BUS,EQ_GAIN,99),12);NEAR(eq_limit(3,EQ_STRIP,EQ_GAIN,-99),-36);
 CHECK(isnan(eq_limit(3,EQ_STRIP,99,0)));CHECK(isnan(eq_limit(3,EQ_STRIP,EQ_GAIN,NAN)));
 NEAR(eq_sum(&s,0,-1,1000),0);
 printf("%d checks, %d failures\n",checks,failed);return failed?1:0;
}

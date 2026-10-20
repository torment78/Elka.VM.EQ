#include "eq.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define PI 3.14159265358979323846
double eq_clamp(double v,double l,double h){return v<l?l:v>h?h:v;}
int eq_targets(int e,int k){if(e<1||e>3||k<0||k>1)return 0;return k==EQ_STRIP?(e==3?5:e==2?3:2):(e==3?8:e==2?5:2);}
int eq_supported(int e,int k){return k==EQ_STRIP?e==3:k==EQ_BUS&&e>=2&&e<=3;}
const char* eq_edition(int e){return e==3?"Potato":e==2?"Banana":e==1?"Standard":"Disconnected";}
void eq_target_name(int e,int k,int i,char* out,size_t size){
 if(i<0||i>=eq_targets(e,k)){snprintf(out,size,"--");return;}
 if(k==EQ_STRIP)snprintf(out,size,"IN %d",i+1);
 else {int h=e==3?5:e==2?3:1;snprintf(out,size,"%c%d",i<h?'A':'B',i<h?i+1:i-h+1);}
}
int eq_key(char* out,size_t size,int e,int k,int i,int c,int b,int f){
 static const char* fields[]={"on","type","f","gain","q"};
 if(!eq_supported(e,k)||i<0||i>=eq_targets(e,k)||c<0||c>=(k==EQ_STRIP?2:8)||b<0||b>=6||f<0||f>=5)return 0;
 int n=snprintf(out,size,"%s[%d].EQ.channel[%d].cell[%d].%s",k==EQ_STRIP?"Strip":"Bus",i,c,b,fields[f]);
 return n>0&&(size_t)n<size;
}
void eq_defaults(EqSnapshot* s){
 static const float centers[]={50,200,800,2000,8000,12000};
 memset(s,0,sizeof(*s));s->sample_rate=48000;s->channels=2;
 for(int c=0;c<8;c++)for(int b=0;b<6;b++)s->bands[c][b]=(EqBand){{1,0,centers[b],0,3}};
}
double eq_limit(int e,int k,int f,double v){
 if(!isfinite(v))return NAN;
 switch(f){
 case EQ_ON:return v>.5?1:0;case EQ_TYPE:return eq_clamp(round(v),0,6);
 case EQ_FREQ:return eq_clamp(v,20,20000);case EQ_GAIN:return eq_clamp(v,e==2&&k==EQ_BUS?-12:-36,e==2&&k==EQ_BUS?12:18);
 case EQ_Q:return eq_clamp(v,e==2&&k==EQ_BUS?1:.3,100);default:return NAN;
 }
}
// RBJ preview, not a sample-perfect model of unpublished VoiceMeeter DSP.
// https://www.w3.org/TR/audio-eq-cookbook/
double eq_response(const EqBand* b,double f,double rate){
 if(b->v[EQ_ON]<.5)return 0;rate=eq_clamp(rate,32000,384000);
 double w=2*PI*fmin(b->v[EQ_FREQ],rate*.499)/rate,c=cos(w),s=sin(w);
 double a=pow(10,b->v[EQ_GAIN]/40),alpha=s/(2*eq_clamp(b->v[EQ_Q],.3,100));
 double b0,b1,b2,a0,a1,a2,l;
 switch((int)b->v[EQ_TYPE]){
 case 1:b0=1;b1=-2*c;b2=1;a0=1+alpha;a1=-2*c;a2=1-alpha;break;
 case 2:b0=alpha;b1=0;b2=-alpha;a0=1+alpha;a1=-2*c;a2=1-alpha;break;
 case 3:b0=(1-c)/2;b1=1-c;b2=b0;a0=1+alpha;a1=-2*c;a2=1-alpha;break;
 case 4:b0=(1+c)/2;b1=-(1+c);b2=b0;a0=1+alpha;a1=-2*c;a2=1-alpha;break;
 case 5:l=2*sqrt(a)*alpha;b0=a*((a+1)-(a-1)*c+l);b1=2*a*((a-1)-(a+1)*c);b2=a*((a+1)-(a-1)*c-l);
 a0=(a+1)+(a-1)*c+l;a1=-2*((a-1)+(a+1)*c);a2=(a+1)+(a-1)*c-l;break;
 case 6:l=2*sqrt(a)*alpha;b0=a*((a+1)+(a-1)*c+l);b1=-2*a*((a-1)+(a+1)*c);b2=a*((a+1)+(a-1)*c-l);
 a0=(a+1)-(a-1)*c+l;a1=2*((a-1)-(a+1)*c);a2=(a+1)-(a-1)*c-l;break;
 default:b0=1+alpha*a;b1=-2*c;b2=1-alpha*a;a0=1+alpha/a;a1=-2*c;a2=1-alpha/a;break;
 }
 double z=2*PI*fmin(f,rate*.499)/rate;
 double nr=b0+b1*cos(z)+b2*cos(2*z),ni=-b1*sin(z)-b2*sin(2*z);
 double dr=a0+a1*cos(z)+a2*cos(2*z),di=-a1*sin(z)-a2*sin(2*z);
 return 10*log10(fmax(1e-20,(nr*nr+ni*ni)/fmax(1e-30,dr*dr+di*di)));
}
double eq_sum(const EqSnapshot* s,int c,int solo,double f){
 double v=0;if(c<0||c>=s->channels)return 0;
 for(int b=0;b<6;b++)if(solo<0||b==solo)v+=eq_response(&s->bands[c][b],f,s->sample_rate);return v;
}
double eq_plot_x(double f){return log10(eq_clamp(f,20,20000)/20)/3;}
double eq_plot_y(double db){return (24-eq_clamp(db,-54,24))/78;}
double eq_plot_frequency(double x){return 20*pow(1000,eq_clamp(x,0,1));}
double eq_plot_gain(double y){return 24-eq_clamp(y,0,1)*78;}

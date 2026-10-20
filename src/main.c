#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <wchar.h>
#include "vm_api.h"
#define CW 1100
#define CH 890
#define PI 3.14159265358979323846
static const COLORREF cyan=RGB(3,235,241),ink=RGB(220,232,239),muted=RGB(131,173,190),grid=RGB(53,89,104),well=RGB(7,30,40);
typedef struct {RECT r;int id,enabled;} Hit;
static struct {
 HWND window,edit;WNDPROC edit_proc;VmApi vm;EqSnapshot s,old;
 int demo,band,channel,solo,focus,drag,drag_band,menu,clip_channels,edit_field;
 double scale_x,scale_y,drag_y,drag_value;EqBand clipboard[8][6];
 Hit hits[64];int hit_count;HBRUSH edit_brush;
} app;
static const RECT plot={46,464,1066,841};
static RECT box(int x,int y,int w,int h){RECT r={x,y,x+w,y+h};return r;}
static int active_channel(void){return app.channel<0?0:app.channel;}
static int gain_enabled(void){int t=(int)app.s.bands[active_channel()][app.band].v[EQ_TYPE];return t==0||t>=5;}
static void refresh(void){if(!app.demo)vm_snapshot(&app.vm,&app.s);if(app.channel>=app.s.channels)app.channel=-1;InvalidateRect(app.window,NULL,FALSE);}
static void end_drag(void){app.drag=0;if(!app.demo)vm_dragging(&app.vm,0);if(GetCapture()==app.window)ReleaseCapture();}
static void fill(HDC dc,RECT r,COLORREF color){HBRUSH b=CreateSolidBrush(color);FillRect(dc,&r,b);DeleteObject(b);}
static void line(HDC dc,double x,double y,double x2,double y2,COLORREF color,int width){
 HPEN pen=CreatePen(PS_SOLID,width,color),old=SelectObject(dc,pen);MoveToEx(dc,(int)round(x),(int)round(y),NULL);LineTo(dc,(int)round(x2),(int)round(y2));SelectObject(dc,old);DeleteObject(pen);
}
static void round_box(HDC dc,RECT r,COLORREF bg,COLORREF border,int radius){
 HBRUSH brush=CreateSolidBrush(bg),oldb=SelectObject(dc,brush);HPEN pen=CreatePen(PS_SOLID,1,border),oldp=SelectObject(dc,pen);
 RoundRect(dc,r.left,r.top,r.right,r.bottom,radius,radius);SelectObject(dc,oldp);SelectObject(dc,oldb);DeleteObject(pen);DeleteObject(brush);
}
static void text(HDC dc,const wchar_t* value,RECT r,int size,COLORREF color,UINT align){
 HFONT font=CreateFontW(-size,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Segoe UI");
 HFONT old=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,color);DrawTextW(dc,value,-1,&r,align|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
 SelectObject(dc,old);DeleteObject(font);
}
static void utf8(wchar_t* dest,int size,const char* source){MultiByteToWideChar(CP_UTF8,0,source,-1,dest,size);dest[size-1]=0;}
static void circle(HDC dc,int cx,int cy,int radius,COLORREF bg,COLORREF stroke){
 HBRUSH b=CreateSolidBrush(bg),ob=SelectObject(dc,b);HPEN p=CreatePen(PS_SOLID,1,stroke),op=SelectObject(dc,p);
 Ellipse(dc,cx-radius,cy-radius,cx+radius+1,cy+radius+1);SelectObject(dc,op);SelectObject(dc,ob);DeleteObject(p);DeleteObject(b);
}
static void arc(HDC dc,int cx,int cy,double radius,double sweep,COLORREF color,int width){
 if(sweep<=0)return;POINT points[181];int count=(int)ceil(sweep/2)+1;if(count>181)count=181;
 for(int i=0;i<count;i++){double a=(135+sweep*i/(count-1))*PI/180;points[i]=(POINT){(LONG)round(cx+radius*cos(a)),(LONG)round(cy+radius*sin(a))};}
 HPEN p=CreatePen(PS_SOLID,width,color),old=SelectObject(dc,p);Polyline(dc,points,count);SelectObject(dc,old);DeleteObject(p);
}
static void button(HDC dc,int id,RECT r,const wchar_t* label,int selected,int enabled){
 round_box(dc,r,selected?cyan:well,selected?cyan:enabled?muted:grid,7);
 text(dc,label,r,16,selected?well:enabled?ink:grid,DT_CENTER);
 if(app.hit_count<64)app.hits[app.hit_count++]=(Hit){r,id,enabled};
}
static void limits(int field,double* low,double* high){*low=eq_limit(app.s.edition,app.s.kind,field,-1e9);*high=eq_limit(app.s.edition,app.s.kind,field,1e9);}
static double norm(int f,double value){double lo,hi;limits(f,&lo,&hi);return eq_clamp(f==EQ_GAIN?(value-lo)/(hi-lo):log(value/lo)/log(hi/lo),0,1);}
static double denorm(int f,double n){double lo,hi;limits(f,&lo,&hi);n=eq_clamp(n,0,1);return f==EQ_GAIN?lo+n*(hi-lo):lo*pow(hi/lo,n);}
static int knob_field(int x,int y){if(y<115||y>377)return -1;if(x>=165&&x<425)return EQ_FREQ;if(x>=425&&x<685)return EQ_GAIN;if(x>=685&&x<945)return EQ_Q;return -1;}
static void knob(HDC dc,int f,int cx,const wchar_t* title,const wchar_t* unit){
 double value=app.s.bands[active_channel()][app.band].v[f];int editable=app.s.ready&&(f!=EQ_GAIN||gain_enabled());
 int radius=f==EQ_GAIN?72:63,cy=267;wchar_t label[48];double low,high;limits(f,&low,&high);
 text(dc,title,box(cx-85,99,170,28),16,ink,DT_CENTER);
 round_box(dc,box(cx-55,133,110,30),well,editable?cyan:grid,7);
 if(app.s.ready)swprintf_s(label,48,f==EQ_Q?L"%.3f":L"%.1f",f==EQ_GAIN&&!gain_enabled()?0:value);else wcscpy_s(label,48,L"--");
 text(dc,label,box(cx-55,133,110,30),18,ink,DT_CENTER);text(dc,unit,box(cx+61,133,35,30),13,muted,DT_LEFT);
 for(int i=0;i<=24;i++){double a=(135+i*11.25)*PI/180;double r0=radius+16,r1=radius+(i%4?21:25);
 line(dc,cx+r0*cos(a),cy+r0*sin(a),cx+r1*cos(a),cy+r1*sin(a),muted,i%4?1:2);}
 arc(dc,cx,cy,radius+9,270,grid,5);if(app.s.ready)arc(dc,cx,cy,radius+9,270*norm(f,value),editable?cyan:muted,5);
 circle(dc,cx,cy,radius,RGB(20,44,57),muted);circle(dc,cx,cy,radius-4,RGB(12,30,39),well);
 // Subtle radial face gradient made of nested native GDI ellipses.
 for(int r=radius-8;r>0;r--){int t=(radius-r)*12/radius;circle(dc,cx,cy-1,r,RGB(22+t,45+t,58+t),RGB(22+t,45+t,58+t));}
 if(app.s.ready){double a=(135+270*norm(f,value))*PI/180;circle(dc,cx+(int)((radius-15)*cos(a)),cy+(int)((radius-15)*sin(a)),6,ink,ink);}
 swprintf_s(label,48,L"%g",low);text(dc,label,box(cx-100,347,75,25),13,muted,DT_CENTER);
 swprintf_s(label,48,L"%g",high);text(dc,label,box(cx+35,347,75,25),13,muted,DT_CENTER);
 if(app.focus==f)line(dc,cx-55,166,cx+55,166,cyan,1);
}
static void filter_icon(HDC dc,int type,RECT r,int selected,int enabled){
 POINT pts[61];for(int i=0;i<=60;i++){double x=i/60.,h=exp(-pow((x-.5)*9,2)),s=1/(1+exp(-(x-.5)*20)),y;
 switch(type){case 0:y=12-10*h;break;case 1:y=3+10*h;break;case 2:y=15-13*h;break;case 3:y=3+12*s;break;case 4:y=15-12*s;break;case 5:y=5+7*s;break;default:y=12-7*s;break;}
 pts[i]=(POINT){r.left+20+i,r.top+5+(LONG)y};}
 HPEN p=CreatePen(PS_SOLID,2,selected?well:enabled?ink:grid),op=SelectObject(dc,p);Polyline(dc,pts,61);SelectObject(dc,op);DeleteObject(p);
}
static POINT handle(int b){
 double f=app.s.bands[active_channel()][b].v[EQ_FREQ];
 return (POINT){plot.left+(LONG)round(eq_plot_x(f)*(plot.right-plot.left)),plot.top+(LONG)round(eq_plot_y(eq_sum(&app.s,active_channel(),app.solo?app.band:-1,f))*(plot.bottom-plot.top))};
}
static void graph(HDC dc){
 static const int ticks[]={24,18,12,6,3,0,-3,-6,-12,-18,-24,-36,-48,-54};
 static const int frequencies[]={20,30,40,50,60,70,80,90,100,200,300,400,500,600,700,800,900,1000,2000,3000,4000,5000,6000,7000,8000,9000,10000,20000};
 fill(dc,plot,well);
 for(int i=0;i<(int)(sizeof(ticks)/sizeof(ticks[0]));i++){int y=plot.top+(int)round(eq_plot_y(ticks[i])*(plot.bottom-plot.top));wchar_t t[12];
 swprintf_s(t,12,ticks[i]>0?L"+%d":L"%d",ticks[i]);line(dc,plot.left,y,plot.right,y,ticks[i]==0?muted:grid,1);text(dc,t,box(3,y-10,37,20),11,ink,DT_RIGHT);}
 for(int i=0;i<(int)(sizeof(frequencies)/sizeof(frequencies[0]));i++){int f=frequencies[i],x=plot.left+(int)round(eq_plot_x(f)*(plot.right-plot.left));wchar_t t[12];
 int full=f==20||f==100||f==1000||f==10000;
 if(f==20000)wcscpy_s(t,12,L"20k");else if(f==10000)wcscpy_s(t,12,L"10k");else if(f==1000)wcscpy_s(t,12,L"1k");else swprintf_s(t,12,L"%d",full?f:(int)round(f/pow(10,floor(log10(f)))));
 line(dc,x,plot.top,x,plot.bottom,grid,1);text(dc,t,box(x-20,plot.bottom+6,40,20),10,muted,DT_CENTER);}
 if(!app.s.ready){text(dc,L"Waiting for a supported EQ",box(plot.left+20,plot.top+20,600,34),20,muted,DT_LEFT);return;}
 int saved=SaveDC(dc);IntersectClipRect(dc,plot.left,plot.top,plot.right+1,plot.bottom+1);
 int count=app.channel<0?app.s.channels:1;
 for(int n=count-1;n>=0;n--){
  int c=app.channel<0?n:app.channel;POINT points[1021];
  for(int i=0;i<=1020;i++){double f=eq_plot_frequency(i/1020.);
   points[i]=(POINT){plot.left+i,plot.top+(LONG)round(eq_plot_y(eq_sum(&app.s,c,app.solo?app.band:-1,f))*(plot.bottom-plot.top))};}
  COLORREF color=(app.channel<0&&c>0)?muted:app.s.enabled?cyan:muted;
  HPEN pen=CreatePen((app.channel<0&&c>0)?PS_DOT:PS_SOLID,(app.channel<0&&c>0)?1:2,color),op=SelectObject(dc,pen);
  Polyline(dc,points,1021);SelectObject(dc,op);DeleteObject(pen);
 }
 RestoreDC(dc,saved);
 for(int b=0;b<6;b++){POINT p=handle(b);wchar_t t[6];swprintf_s(t,6,L"%d",b+1);
 if(b==app.band){HBRUSH ob=SelectObject(dc,GetStockObject(HOLLOW_BRUSH));HPEN pen=CreatePen(PS_SOLID,1,cyan),op=SelectObject(dc,pen);Ellipse(dc,p.x-10,p.y-10,p.x+11,p.y+11);SelectObject(dc,op);SelectObject(dc,ob);DeleteObject(pen);}
 circle(dc,p.x,p.y,6,ink,app.s.bands[active_channel()][b].v[EQ_ON]>.5?cyan:muted);
 text(dc,t,box(p.x-12,(int)fmax(plot.top,p.y-31),24,20),12,cyan,DT_CENTER);}
}
static void paint(HDC dc){
 app.hit_count=0;
 TRIVERTEX vertices[2]={{0,0,32<<8,62<<8,77<<8,0},{CW,CH,8<<8,28<<8,37<<8,0}};GRADIENT_RECT gr={0,1};GradientFill(dc,vertices,2,&gr,1,GRADIENT_FILL_RECT_V);
 text(dc,L"VM EQ",box(18,6,175,35),22,cyan,DT_LEFT);
 button(dc,1,box(218,8,142,31),app.s.kind==EQ_STRIP?L"STRIP  \x25BE":L"BUS  \x25BE",0,1);
 char label[40];wchar_t wide[200];eq_target_name(app.s.edition,app.s.kind,app.s.index,label,sizeof(label));utf8(wide,200,label);
 wcscat_s(wide,200,L"  \x25BE");button(dc,2,box(372,8,128,31),wide,0,app.s.edition!=0);
 utf8(wide,200,eq_edition(app.s.edition));text(dc,wide,box(520,6,220,35),14,muted,DT_LEFT);
 button(dc,3,box(828,8,156,31),app.s.enabled?L"EQ ON":L"EQ OFF",app.s.enabled&&app.s.ready,app.s.ready);
 button(dc,4,box(994,8,40,31),L"\x2013",0,1);button(dc,5,box(1044,8,40,31),L"\x00D7",0,1);
 line(dc,10,46,1090,46,grid,1);
 button(dc,10,box(14,56,68,31),L"FLAT",0,app.s.ready);
 button(dc,11,box(92,56,44,31),L"A",app.s.bank==0,app.s.ready);
 button(dc,12,box(146,56,44,31),L"B",app.s.bank==1,app.s.ready);
 text(dc,L"Channel",box(206,56,65,31),14,ink,DT_LEFT);
 button(dc,20,box(278,56,44,31),L"All",app.channel<0,1);
 for(int c=0;c<app.s.channels;c++){swprintf_s(wide,200,L"%d",c+1);button(dc,21+c,box(332+c*48,56,40,31),wide,app.channel==c,1);}
 button(dc,31,box(816,56,82,31),L"Ch.Copy",0,app.s.ready);
 button(dc,32,box(906,56,86,31),L"Copy All",0,app.s.ready);
 button(dc,33,box(1000,56,84,31),L"Paste",0,app.s.ready&&app.clip_channels>0);
 text(dc,L"Filter Type",box(22,100,118,29),16,ink,DT_CENTER);
 for(int i=0;i<7;i++){RECT r=box(28,135+i*34,106,28);int active=app.s.ready&&(int)app.s.bands[active_channel()][app.band].v[EQ_TYPE]==i;
 button(dc,40+i,r,L"",active,app.s.ready);filter_icon(dc,i,r,active,app.s.ready);}
 line(dc,153,107,153,378,grid,1);
 knob(dc,EQ_FREQ,295,L"Frequency",L"Hz");knob(dc,EQ_GAIN,555,L"Gain",L"dB");knob(dc,EQ_Q,815,L"Q",L"");
 line(dc,956,107,956,378,grid,1);
 button(dc,50,box(975,137,102,33),L"On",app.s.ready&&app.s.bands[active_channel()][app.band].v[EQ_ON]>.5,app.s.ready);
 button(dc,51,box(975,184,102,33),L"Solo curve",app.solo,1);
 button(dc,52,box(975,231,102,33),L"Flat",0,app.s.ready);button(dc,53,box(975,278,102,33),L"Reset",0,app.s.ready);
 line(dc,10,389,1090,389,grid,1);
 for(int b=0;b<6;b++){swprintf_s(wide,200,L"%d",b+1);button(dc,60+b,box(14+b*180,402,170,34),wide,b==app.band,1);}
 graph(dc);
 utf8(wide,200,app.demo?"DEMO | No VoiceMeeter connection | Calculated response preview":app.s.status);
 text(dc,wide,box(46,865,1020,21),11,muted,DT_LEFT);
 if(app.menu){int x=app.menu==1?218:372,width=app.menu==1?142:128,n=app.menu==1?2:eq_targets(app.s.edition,app.s.kind);
  for(int i=0;i<n;i++){if(app.menu==1)wcscpy_s(wide,200,i==0?L"Strip":L"Bus");
   else {eq_target_name(app.s.edition,app.s.kind,i,label,sizeof(label));utf8(wide,200,label);}
   RECT r=box(x,42+i*34,width,34);round_box(dc,r,RGB(14,39,50),muted,0);text(dc,wide,r,16,ink,DT_CENTER);}}
}
static HBITMAP render_raw(HDC dc,int width,int height){
 BITMAPINFO info={0};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
 void* bits;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,NULL,0);if(!bitmap)return NULL;
 HDC memory=CreateCompatibleDC(dc);HBITMAP old=SelectObject(memory,bitmap);SetGraphicsMode(memory,GM_ADVANCED);
 XFORM xf={(FLOAT)width/CW,0,0,(FLOAT)height/CH,0,0};SetWorldTransform(memory,&xf);paint(memory);
 SelectObject(memory,old);DeleteDC(memory);return bitmap;
}
static HBITMAP render_bitmap(HDC dc,int width,int height){
 // Supersample vector edges, retaining native GDI and no framework dependency.
 if(width>2200||height>1780)return render_raw(dc,width,height);
 HBITMAP large=render_raw(dc,width*2,height*2);if(!large)return NULL;
 BITMAPINFO info={0};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;
 info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;void* bits;
 HBITMAP small=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,NULL,0);
 if(!small){DeleteObject(large);return NULL;}
 HDC source=CreateCompatibleDC(dc),dest=CreateCompatibleDC(dc);
 HBITMAP os=SelectObject(source,large),od=SelectObject(dest,small);
 SetStretchBltMode(dest,HALFTONE);SetBrushOrgEx(dest,0,0,NULL);StretchBlt(dest,0,0,width,height,source,0,0,width*2,height*2,SRCCOPY);
 SelectObject(source,os);SelectObject(dest,od);DeleteDC(source);DeleteDC(dest);DeleteObject(large);return small;
}
static void save_bitmap(const wchar_t* path){
 HDC dc=GetDC(NULL);HBITMAP bitmap=render_bitmap(dc,CW,CH);ReleaseDC(NULL,dc);if(!bitmap)return;
 BITMAP bm;GetObject(bitmap,sizeof(bm),&bm);
 BITMAPFILEHEADER file={0};BITMAPINFOHEADER info={0};info.biSize=sizeof(info);info.biWidth=CW;info.biHeight=-CH;info.biPlanes=1;info.biBitCount=32;info.biSizeImage=CW*CH*4;
 file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(info);file.bfSize=file.bfOffBits+info.biSizeImage;
 HANDLE out=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);DWORD written;
 if(out!=INVALID_HANDLE_VALUE){WriteFile(out,&file,sizeof(file),&written,NULL);WriteFile(out,&info,sizeof(info),&written,NULL);WriteFile(out,bm.bmBits,info.biSizeImage,&written,NULL);CloseHandle(out);}DeleteObject(bitmap);
}
static void edit(int field,double value){
 if(!app.s.ready)return;
 if(app.demo){value=eq_limit(app.s.edition,app.s.kind,field,value);if(field==EQ_GAIN&&!gain_enabled())return;
 for(int c=0;c<app.s.channels;c++)if(app.channel<0||app.channel==c){app.s.bands[c][app.band].v[field]=(float)value;if(field==EQ_TYPE&&value>=1&&value<=4)app.s.bands[c][app.band].v[EQ_GAIN]=0;}}
 else vm_edit(&app.vm,app.channel,app.band,field,value);refresh();
}
static void reset(int only_band){
 EqSnapshot def;eq_defaults(&def);
 if(app.demo){for(int c=0;c<app.s.channels;c++)if(app.channel<0||app.channel==c)for(int b=0;b<6;b++)if(only_band<0||b==only_band)app.s.bands[c][b]=def.bands[c][b];}
 else vm_apply(&app.vm,app.channel,def.bands,1,only_band);refresh();
}
static void select_target(int kind,int index){
 end_drag();app.channel=-1;app.menu=0;
 if(app.demo){app.s.kind=kind;app.s.index=index;app.s.channels=kind==EQ_STRIP?2:8;app.s.ready=eq_supported(app.s.edition,kind);}
 else vm_select(&app.vm,kind,index);refresh();
}
static void action(int id){
 end_drag();
 if(id==1||id==2){app.menu=app.menu==id?0:id;refresh();return;}
 app.menu=0;
 if(id==4){ShowWindow(app.window,SW_MINIMIZE);return;}if(id==5){DestroyWindow(app.window);return;}
 if(id==3){if(app.demo)app.s.enabled=!app.s.enabled;else vm_enabled(&app.vm,!app.s.enabled);}
 if(id==10)reset(-1);
 if(id==11||id==12){if(app.demo)app.s.bank=id-11;else if(app.s.bank!=id-11)vm_bank(&app.vm,id-11);}
 if(id>=20&&id<=28)app.channel=id-21;
 if(id==31||id==32){app.clip_channels=id==31?1:app.s.channels;
 if(id==31)memcpy(app.clipboard[0],app.s.bands[active_channel()],sizeof(app.clipboard[0]));else memcpy(app.clipboard,app.s.bands,sizeof(app.clipboard));}
 if(id==33){
 if(app.clip_channels!=1&&app.clip_channels!=app.s.channels)MessageBoxW(app.window,L"Copy All requires matching channel counts. Use Ch.Copy to paste one channel into any strip or bus.",L"Channel count",MB_OK);
 else if(app.demo){for(int c=0;c<app.s.channels;c++)if(app.channel<0||app.channel==c)memcpy(app.s.bands[c],app.clipboard[app.clip_channels==1?0:c],sizeof(app.s.bands[c]));}
 else vm_apply(&app.vm,app.channel,app.clipboard,app.clip_channels,-1);}
 if(id>=40&&id<=46)edit(EQ_TYPE,id-40);
 if(id==50)edit(EQ_ON,app.s.bands[active_channel()][app.band].v[EQ_ON]>.5?0:1);
 if(id==51)app.solo=!app.solo;
 if(id==52){edit(EQ_TYPE,0);edit(EQ_GAIN,0);}
 if(id==53)reset(app.band);
 if(id>=60&&id<=65)app.band=id-60;refresh();
}
static POINT logical(LPARAM lp){return (POINT){(LONG)(GET_X_LPARAM(lp)/app.scale_x),(LONG)(GET_Y_LPARAM(lp)/app.scale_y)};}
static void step(int field,int sign,int fine){
 double value=app.s.bands[active_channel()][app.band].v[field];
 if(field==EQ_GAIN)edit(field,value+sign*(fine?.1:1));
 else edit(field,denorm(field,norm(field,value)+sign*(fine?.001:.01)));
}
static void close_edit(int apply){
 if(!app.edit)return;
 if(apply){wchar_t s[80],*end;GetWindowTextW(app.edit,s,80);double v=wcstod(s,&end),low,high;limits(app.edit_field,&low,&high);
 while(*end==L' ')end++;
 if(end==s||*end||!isfinite(v)||v<low||v>high){MessageBeep(MB_ICONWARNING);return;}edit(app.edit_field,v);}
 HWND old=app.edit;app.edit=NULL;DestroyWindow(old);SetFocus(app.window);refresh();
}
static LRESULT CALLBACK edit_proc(HWND h,UINT m,WPARAM w,LPARAM l){
 if(m==WM_KEYDOWN&&w==VK_RETURN){close_edit(1);return 0;}if(m==WM_KEYDOWN&&w==VK_ESCAPE){close_edit(0);return 0;}
 return CallWindowProcW(app.edit_proc,h,m,w,l);
}
static void numeric(int field){
 if(!app.s.ready||(field==EQ_GAIN&&!gain_enabled()))return;end_drag();if(app.edit)close_edit(0);
 int cx=field==EQ_FREQ?295:field==EQ_GAIN?555:815;wchar_t value[48];
 swprintf_s(value,48,L"%.6g",(double)app.s.bands[active_channel()][app.band].v[field]);
 app.edit_field=field;app.edit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",value,WS_CHILD|WS_VISIBLE|ES_CENTER|ES_AUTOHSCROLL,
 (int)((cx-55)*app.scale_x),(int)(133*app.scale_y),(int)(110*app.scale_x),(int)(30*app.scale_y),app.window,NULL,GetModuleHandle(NULL),NULL);
 app.edit_proc=(WNDPROC)SetWindowLongPtrW(app.edit,GWLP_WNDPROC,(LONG_PTR)edit_proc);
 SendMessageW(app.edit,EM_SETSEL,0,-1);SetFocus(app.edit);
}
static LRESULT CALLBACK wndproc(HWND h,UINT m,WPARAM w,LPARAM l){
 switch(m){
 case WM_CREATE:app.window=h;SetTimer(h,1,33,NULL);return 0;
 case WM_SIZE:app.scale_x=LOWORD(l)/(double)CW;app.scale_y=HIWORD(l)/(double)CH;if(app.scale_x<=0)app.scale_x=1;if(app.scale_y<=0)app.scale_y=1;return 0;
 case WM_ERASEBKGND:return 1;
 case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);if(r.right>0&&r.bottom>0){
 HBITMAP bitmap=render_bitmap(dc,r.right,r.bottom);if(bitmap){HDC mem=CreateCompatibleDC(dc);HBITMAP old=SelectObject(mem,bitmap);BitBlt(dc,0,0,r.right,r.bottom,mem,0,0,SRCCOPY);SelectObject(mem,old);DeleteDC(mem);DeleteObject(bitmap);}}
 EndPaint(h,&ps);return 0;}
 case WM_TIMER:if(!app.demo){EqSnapshot next;vm_snapshot(&app.vm,&next);if(memcmp(&next,&app.s,sizeof(next))){app.s=next;if(app.channel>=next.channels)app.channel=-1;InvalidateRect(h,NULL,FALSE);}}return 0;
 case WM_NCHITTEST:{LRESULT result=DefWindowProcW(h,m,w,l);if(result!=HTCLIENT)return result;POINT p={GET_X_LPARAM(l),GET_Y_LPARAM(l)};ScreenToClient(h,&p);
 p.x=(LONG)(p.x/app.scale_x);p.y=(LONG)(p.y/app.scale_y);
 if(p.y<45&&p.x<205)return HTCAPTION;return HTCLIENT;}
 case WM_GETMINMAXINFO:((MINMAXINFO*)l)->ptMinTrackSize=(POINT){550,445};return 0;
 case WM_SIZING:{RECT* r=(RECT*)l;RECT win,client;GetWindowRect(h,&win);GetClientRect(h,&client);
 int dx=(win.right-win.left)-client.right,dy=(win.bottom-win.top)-client.bottom;
 int width=r->right-r->left,height=r->bottom-r->top;
 if(w==WMSZ_TOP||w==WMSZ_BOTTOM){width=(int)round((height-dy)*CW/(double)CH)+dx;r->right=r->left+width;}
 else {height=(int)round((width-dx)*CH/(double)CW)+dy;if(w==WMSZ_TOPLEFT||w==WMSZ_TOPRIGHT)r->top=r->bottom-height;else r->bottom=r->top+height;}return TRUE;}
 case WM_DPICHANGED:{RECT* r=(RECT*)l;SetWindowPos(h,NULL,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);return 0;}
 case WM_ACTIVATE:if(LOWORD(w)==WA_INACTIVE){app.menu=0;end_drag();if(app.edit)close_edit(0);InvalidateRect(h,NULL,FALSE);}return 0;
 case WM_LBUTTONDOWN:case WM_LBUTTONDBLCLK:{
 if(app.edit){close_edit(1);return 0;}SetFocus(h);POINT p=logical(l);
 if(app.menu){int x=app.menu==1?218:372,width=app.menu==1?142:128,n=app.menu==1?2:eq_targets(app.s.edition,app.s.kind),choice=(p.y-42)/34;
 if(p.x>=x&&p.x<x+width&&p.y>=42&&choice<n){if(app.menu==1)select_target(choice,0);else select_target(app.s.kind,choice);}
 app.menu=0;refresh();return 0;}
 for(int i=0;i<app.hit_count;i++)if(PtInRect(&app.hits[i].r,p)){if(app.hits[i].enabled)action(app.hits[i].id);return 0;}
 if(!app.s.ready)return 0;int field=knob_field(p.x,p.y);
 if(field>=0){app.focus=field;if(field==EQ_GAIN&&!gain_enabled())return 0;
 if(m==WM_LBUTTONDBLCLK){edit(field,field==EQ_FREQ?1000:field==EQ_Q?3:0);return 0;}
 app.drag=field;app.drag_y=p.y;app.drag_value=norm(field,app.s.bands[active_channel()][app.band].v[field]);
 if(!app.demo)vm_dragging(&app.vm,1);SetCapture(h);return 0;}
 if(PtInRect(&plot,p)){int nearest=-1;double distance=23;
 for(int b=0;b<6;b++){POINT pt=handle(b);double d=hypot(pt.x-p.x,pt.y-p.y);if(d<distance){distance=d;nearest=b;}}
 if(nearest>=0){app.band=nearest;if(m==WM_LBUTTONDBLCLK){reset(nearest);return 0;}
 app.drag=10;app.drag_y=p.y;app.drag_value=app.s.bands[active_channel()][nearest].v[EQ_GAIN];app.drag_band=nearest;
 if(!app.demo)vm_dragging(&app.vm,1);SetCapture(h);refresh();}}
 return 0;}
 case WM_MOUSEMOVE:if(app.drag){POINT p=logical(l);double fine=GetKeyState(VK_SHIFT)<0?.1:1;
 if(app.drag==10){edit(EQ_FREQ,eq_plot_frequency((p.x-plot.left)/(double)(plot.right-plot.left)));
 if(gain_enabled())edit(EQ_GAIN,app.drag_value+(app.drag_y-p.y)*78/(plot.bottom-plot.top)*fine);}
 else edit(app.drag,denorm(app.drag,app.drag_value+(app.drag_y-p.y)/240*fine));}return 0;
 case WM_LBUTTONUP:case WM_CAPTURECHANGED:end_drag();return 0;
 case WM_MOUSEWHEEL:{POINT p={GET_X_LPARAM(l),GET_Y_LPARAM(l)};ScreenToClient(h,&p);p.x=(LONG)(p.x/app.scale_x);p.y=(LONG)(p.y/app.scale_y);
 int f=knob_field(p.x,p.y);if(f<0&&PtInRect(&plot,p))f=EQ_Q;if(f>=0){app.focus=f;step(f,GET_WHEEL_DELTA_WPARAM(w)>0?1:-1,GetKeyState(VK_SHIFT)<0);}return 0;}
 case WM_RBUTTONUP:{POINT p=logical(l);int f=knob_field(p.x,p.y);if(f>=0)numeric(f);return 0;}
 case WM_KEYDOWN:
 if(w==VK_ESCAPE){app.menu=0;end_drag();refresh();return 0;}
 if(w==VK_TAB){app.focus=app.focus==EQ_FREQ?EQ_GAIN:app.focus==EQ_GAIN?EQ_Q:EQ_FREQ;refresh();return 0;}
 if(w==VK_RETURN){numeric(app.focus);return 0;}
 if(w==VK_HOME){edit(app.focus,app.focus==EQ_FREQ?1000:app.focus==EQ_Q?3:0);return 0;}
 if(w==VK_UP||w==VK_RIGHT||w==VK_DOWN||w==VK_LEFT){step(app.focus,w==VK_UP||w==VK_RIGHT?1:-1,GetKeyState(VK_SHIFT)<0);return 0;}break;
 case WM_CTLCOLOREDIT:SetBkColor((HDC)w,well);SetTextColor((HDC)w,ink);return (LRESULT)app.edit_brush;
 case WM_DESTROY:KillTimer(h,1);PostQuitMessage(0);return 0;
 }
 return DefWindowProcW(h,m,w,l);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE previous,PWSTR command,int show){
 (void)previous;(void)command;int argc;LPWSTR* argv=CommandLineToArgvW(GetCommandLineW(),&argc);const wchar_t* render=NULL;int probe=0;
 app.channel=-1;app.focus=EQ_FREQ;app.scale_x=app.scale_y=1;eq_defaults(&app.s);
 for(int i=1;i<argc;i++){if(!wcscmp(argv[i],L"--demo"))app.demo=1;if(!wcscmp(argv[i],L"--probe"))probe=1;
 if(!wcscmp(argv[i],L"--render")&&i+1<argc){render=argv[++i];app.demo=1;}}
 if(probe)app.demo=0;
 if(app.demo){app.s.edition=3;app.s.ready=app.s.enabled=1;app.s.bands[0][0].v[EQ_GAIN]=7;app.s.bands[0][1].v[EQ_GAIN]=-4;app.s.bands[0][3].v[EQ_GAIN]=5;
 memcpy(app.s.bands[1],app.s.bands[0],sizeof(app.s.bands[0]));}
 else if(!vm_start(&app.vm)){MessageBoxW(NULL,L"Could not start the API worker.",L"VM EQ",MB_ICONERROR);LocalFree(argv);return 1;}
 if(probe){Sleep(1200);vm_snapshot(&app.vm,&app.s);wchar_t output[MAX_PATH]=L"native-eq-probe.txt";FILE* file=NULL;_wfopen_s(&file,output,L"w");
 if(file){fprintf(file,"edition=%d (%s), version=%08lx\n",app.s.edition,eq_edition(app.s.edition),app.s.version);
 int edition=app.s.edition;
 for(int kind=0;kind<2;kind++)for(int index=0;index<eq_targets(edition,kind);index++){
 vm_select(&app.vm,kind,index);Sleep(350);vm_snapshot(&app.vm,&app.s);char name[30];eq_target_name(edition,kind,index,name,sizeof(name));
 fprintf(file,"%s %s: ready=%d channels=%d bank=%d status=%s\n",kind==EQ_STRIP?"Strip":"Bus",name,app.s.ready,app.s.channels,app.s.bank,app.s.status);}
 fclose(file);}vm_stop(&app.vm);LocalFree(argv);return file?0:1;}
 if(render){save_bitmap(render);LocalFree(argv);return 0;}
 WNDCLASSW wc={0};wc.style=CS_DBLCLKS;wc.lpfnWndProc=wndproc;wc.hInstance=instance;wc.hCursor=LoadCursor(NULL,IDC_ARROW);wc.lpszClassName=L"ElkaVmEqNative";
 RegisterClassW(&wc);app.edit_brush=CreateSolidBrush(well);
 DWORD style=WS_POPUP|WS_THICKFRAME|WS_MINIMIZEBOX|WS_SYSMENU;
 RECT r={0,0,CW,CH};AdjustWindowRectEx(&r,style,FALSE,0);
 HWND window=CreateWindowExW(0,wc.lpszClassName,L"Elka VM EQ Native",style,CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,NULL,NULL,instance,NULL);
 if(!window){if(!app.demo)vm_stop(&app.vm);LocalFree(argv);return 1;}
 ShowWindow(window,show);UpdateWindow(window);MSG msg;
 while(GetMessageW(&msg,NULL,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
 if(!app.demo)vm_stop(&app.vm);DeleteObject(app.edit_brush);LocalFree(argv);return 0;
}

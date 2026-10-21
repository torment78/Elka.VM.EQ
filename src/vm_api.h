#pragma once
#include <windows.h>
#include "eq.h"
#include "spectrum.h"
typedef struct {int kind,index,edition,bank,channel,band,field;float value;} EqWrite;
typedef struct {
 CRITICAL_SECTION lock;HANDLE thread,stop;EqSnapshot view;
 EqWrite queue[1024];int count,dragging;unsigned generation;ULONGLONG edit_until;
 HMODULE library;int logged;
 Spectrum* spectrum;
 EqLevels levels;unsigned level_epoch;ULONGLONG level_time;
 long (__stdcall *get_level)(long,long,float*);
 long (__stdcall *login)(void);long (__stdcall *logout)(void);long (__stdcall *dirty)(void);
 long (__stdcall *type)(long*);long (__stdcall *version)(long*);
 long (__stdcall *get)(char*,float*);long (__stdcall *set)(char*,float);
} VmApi;
int vm_start(VmApi*);
int vm_start_ex(VmApi*,int analyzer);
void vm_stop(VmApi*);
void vm_snapshot(VmApi*,EqSnapshot*);
void vm_read_levels(VmApi*,EqLevels*);
/* Called only by the existing API worker (or offline fake-API tests). */
void vm_poll_levels(VmApi*);
void vm_select(VmApi*,int kind,int index);
void vm_dragging(VmApi*,int enabled);
int vm_edit(VmApi*,int channel,int band,int field,double value);
int vm_enabled(VmApi*,int enabled);
int vm_bank(VmApi*,int bank);
int vm_apply(VmApi*,int channel,const EqBand data[8][6],int source_channels,int only_band);

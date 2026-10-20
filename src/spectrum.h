#pragma once
#include <windows.h>
#define SPEC_BINS 384
#define SPEC_SAMPLES 4096
#define SPEC_FFT 16384
/* ABI matches VB-Audio's public VBVMR_AUDIOBUFFER (Windows long is 32-bit).
   https://github.com/vburel2018/Voicemeeter-SDK/blob/main/VoicemeeterRemote.h */
typedef struct {long sr,nbs,nbi,nbo;float* r[128];float* w[128];} SpecAudioBuffer;
typedef long (__stdcall *SpecCallback)(void*,long,void*,long);
typedef long (__stdcall *SpecRegister)(long,SpecCallback,void*,char[64]);
typedef long (__stdcall *SpecCommand)(void);
enum {SPEC_OFF,SPEC_STARTING,SPEC_ACTIVE,SPEC_BUSY,SPEC_ERROR,SPEC_WAITING,SPEC_DEMO};
typedef struct {
 int state,requested,sample_rate,kind,index,channel;
 unsigned sequence;int audio_frames;
 float db[SPEC_BINS];char message[160];
} SpecFrame;
typedef struct Spectrum Spectrum;
typedef struct SpecAnalysis SpecAnalysis;
Spectrum* spec_create(void);
/* Returns false if a native driver failed to release our callback; retain memory. */
int spec_destroy(Spectrum* s);
void spec_request(Spectrum* s,int enabled,int kind,int index,int channel);
void spec_read(Spectrum* s,SpecFrame* frame);
void spec_manage(Spectrum* s,HMODULE library,int connected);
void spec_bind(Spectrum* s,SpecRegister reg,SpecCommand start,SpecCommand halt,SpecCommand unreg);
int spec_detach(Spectrum* s);
long __stdcall spec_callback(void* user,long command,void* data,long sync);
/* Pure processing entry points shared by offline tests and worker. */
int spec_main_passthrough(SpecAudioBuffer* b);
int spec_source_offset(const SpecAudioBuffer* b,int kind,int index,int* channels);
SpecAnalysis* spec_analysis_create(void);
void spec_analysis_destroy(SpecAnalysis* a);
void spec_analyze(SpecAnalysis* a,const float* samples,int sample_rate,float out[SPEC_BINS]);

#pragma once
#include "eq.h"
#include "spectrum.h"
#ifdef EQ_ENGINE_BUILD
#define EQ_API __declspec(dllexport)
#else
#define EQ_API __declspec(dllimport)
#endif
/* Opaque C ABI: no Win32 handles or callbacks cross into the frontend. */
typedef void* EqEngine;
EQ_API int __cdecl ElkaEq_AbiVersion(void);
EQ_API int __cdecl ElkaEq_FrameSize(void);
EQ_API int __cdecl ElkaEq_SpectrumFrameSize(void);
EQ_API int __cdecl ElkaEq_SpectrumRequest(EqEngine engine,int enabled,int kind,int index,int channel);
EQ_API int __cdecl ElkaEq_SpectrumRead(EqEngine engine,SpecFrame* frame,int bytes);
EQ_API EqEngine __cdecl ElkaEq_Create(int demo);
EQ_API void __cdecl ElkaEq_Destroy(EqEngine engine);
EQ_API int __cdecl ElkaEq_Read(EqEngine engine,EqSnapshot* frame,int bytes);
EQ_API int __cdecl ElkaEq_Select(EqEngine engine,int kind,int index);
EQ_API int __cdecl ElkaEq_Edit(EqEngine engine,int channel,int band,int field,double value);
EQ_API int __cdecl ElkaEq_Enabled(EqEngine engine,int enabled);
EQ_API int __cdecl ElkaEq_Bank(EqEngine engine,int bank);
EQ_API void __cdecl ElkaEq_Dragging(EqEngine engine,int dragging);
EQ_API int __cdecl ElkaEq_Apply(EqEngine engine,int channel,const float* values,int source_channels,int count,int only_band);
EQ_API int __cdecl ElkaEq_Defaults(EqSnapshot* frame,int bytes);
EQ_API int __cdecl ElkaEq_TargetCount(int edition,int kind);
EQ_API double __cdecl ElkaEq_Limit(int edition,int kind,int field,double value);
EQ_API int __cdecl ElkaEq_Curve(const EqSnapshot* frame,int bytes,int channel,int solo,int count,double* output);

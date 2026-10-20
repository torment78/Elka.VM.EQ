#pragma once
#include <stddef.h>
enum { EQ_STRIP, EQ_BUS };
enum { EQ_ON, EQ_TYPE, EQ_FREQ, EQ_GAIN, EQ_Q, EQ_FIELDS };
typedef struct { float v[EQ_FIELDS]; } EqBand;
typedef struct {
 int edition,kind,index,channels,ready,enabled,bank;
 unsigned long version;
 double sample_rate;
 EqBand bands[8][6];
 char status[200];
} EqSnapshot;
double eq_clamp(double v,double l,double h);
int eq_targets(int edition,int kind);
int eq_supported(int edition,int kind);
const char* eq_edition(int edition);
void eq_target_name(int edition,int kind,int index,char* out,size_t size);
int eq_key(char* out,size_t size,int edition,int kind,int index,int channel,int band,int field);
void eq_defaults(EqSnapshot* s);
double eq_limit(int edition,int kind,int field,double value);
double eq_response(const EqBand* b,double frequency,double rate);
double eq_sum(const EqSnapshot* s,int channel,int solo,double frequency);
double eq_plot_x(double frequency);
double eq_plot_y(double db);
double eq_plot_frequency(double x);
double eq_plot_gain(double y);

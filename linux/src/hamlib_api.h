#pragma once
// Public Hamlib 4 C ABI only. No RIG fields or private structures are accessed.
// Prefer distro SDK headers; runtime-only Ubuntu installations can use this
// small declaration subset, checked against upstream Hamlib 4.6.5 rig.h.
#if __has_include(<hamlib/rig.h>)
#include <hamlib/rig.h>
#else
#include <cstdint>
typedef struct s_rig RIG;
using rig_model_t=int;
using vfo_t=unsigned int;
using freq_t=double;
using rmode_t=std::uint64_t;
using setting_t=std::uint64_t;
using pbwidth_t=long;
using shortfreq_t=long;
using hamlib_token_t=long;
using tone_t=unsigned int;
enum ptt_t {RIG_PTT_OFF=0,RIG_PTT_ON=1,RIG_PTT_ON_MIC=2,RIG_PTT_ON_DATA=3};
enum split_t {RIG_SPLIT_OFF=0,RIG_SPLIT_ON=1};
enum rptr_shift_t {RIG_RPT_SHIFT_NONE=0,RIG_RPT_SHIFT_MINUS=1,RIG_RPT_SHIFT_PLUS=2};
enum rig_debug_level_e {RIG_DEBUG_NONE=0};
union value_t {int i;unsigned int u;float f;char* s;const char* cs;struct {int l;unsigned char* d;} b;};
extern "C" {
RIG* rig_init(rig_model_t);
int rig_open(RIG*);
int rig_close(RIG*);
int rig_cleanup(RIG*);
void rig_set_debug(rig_debug_level_e);
hamlib_token_t rig_token_lookup(RIG*,const char*);
int rig_set_conf(RIG*,hamlib_token_t,const char*);
int rig_get_freq(RIG*,vfo_t,freq_t*);
int rig_set_freq(RIG*,vfo_t,freq_t);
int rig_get_mode(RIG*,vfo_t,rmode_t*,pbwidth_t*);
int rig_set_mode(RIG*,vfo_t,rmode_t,pbwidth_t);
int rig_get_vfo(RIG*,vfo_t*);
int rig_set_vfo(RIG*,vfo_t);
int rig_get_ptt(RIG*,vfo_t,ptt_t*);
int rig_set_ptt(RIG*,vfo_t,ptt_t);
int rig_get_level(RIG*,vfo_t,setting_t,value_t*);
int rig_set_level(RIG*,vfo_t,setting_t,value_t);
int rig_get_rit(RIG*,vfo_t,shortfreq_t*);
int rig_set_rit(RIG*,vfo_t,shortfreq_t);
int rig_get_split_vfo(RIG*,vfo_t,split_t*,vfo_t*);
int rig_set_split_vfo(RIG*,vfo_t,split_t,vfo_t);
int rig_get_split_freq(RIG*,vfo_t,freq_t*);
int rig_set_split_freq(RIG*,vfo_t,freq_t);
int rig_get_rptr_shift(RIG*,vfo_t,rptr_shift_t*);
int rig_set_rptr_shift(RIG*,vfo_t,rptr_shift_t);
int rig_get_rptr_offs(RIG*,vfo_t,shortfreq_t*);
int rig_set_rptr_offs(RIG*,vfo_t,shortfreq_t);
int rig_get_ctcss_tone(RIG*,vfo_t,tone_t*);
int rig_set_ctcss_tone(RIG*,vfo_t,tone_t);
int rig_set_func(RIG*,vfo_t,setting_t,int);
int rig_get_func(RIG*,vfo_t,setting_t,int*);
rmode_t rig_parse_mode(const char*);
vfo_t rig_parse_vfo(const char*);
setting_t rig_parse_level(const char*);
setting_t rig_parse_func(const char*);
const char* rig_strrmode(rmode_t);
const char* rig_strvfo(vfo_t);
const char* rigerror(int);
const char* rigerror2(int);
}
#endif

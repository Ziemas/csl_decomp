#ifndef MODHSYN_H_
#define MODHSYN_H_

#include "csl.h"

#define HSyn_EnvPortIdx(port) (2 * (port) + 1)
#define HSyn_StrPortIdx(port) (2 * (port))
#define HSyn_GetEnv(ctx, port) \
	(struct HSynEnv *)((ctx)->buffGrp[0].buffCtx[HSyn_EnvPortIdx(port)].buff)

enum {
	HSynNoError = 0,
	HSynError = -1,

	HSynMinVelocity = 1,
	HSynNumVelocity = 128,

	HSynEnvSize = 1348,
	HSyn_NumCore = 2,
	HSyn_NumVoice = 24,
};

struct HSynUserLfoWave {
	unsigned char id;
	unsigned short waveLen;
	short *wave;
};

struct HSynUserVelocityMap {
	unsigned char velMap[HSynNumVelocity];
};

struct HSynEnv {
	unsigned char priority;
	unsigned char maxPolyphony;
	unsigned char portMode;
	unsigned char waveType;
	int lfoWaveNum;
	struct HSynUserLfoWave *lfoWaveTbl;
	int velocityMapNum;
	struct HSynUserVelocityMap *velocityMapTbl;
	unsigned char system[HSynEnvSize];
};

struct HSyn_VoiceStat {
	int pendingVoiceCount;
	int workVoiceCount;
	unsigned char voice_state[HSyn_NumCore][HSyn_NumVoice];
	unsigned short voice_env[HSyn_NumCore][HSyn_NumVoice];
};

struct HSyn_EffectAttr {
	int core;
	int mode;
	short depth_L, depth_R;
	int delay;
	int feedback;
	short vol_l, vol_r;
};

struct HSynChStat {
	unsigned char ch[16];
};

struct HSyn_SdCall {
	unsigned int func;
	unsigned int retVal;
	unsigned int arg[5];
};

struct HSyn_DebugInfo {
	unsigned int infoBlkNum;
	unsigned int readIndex;
	unsigned int writeIndex;
	struct HSyn_SdCall sdCall[0];
};

int HSyn_Init(struct CslCtx *ctx, unsigned int interval);
int HSyn_ATick(struct CslCtx *ctx);

#endif // MODHSYN_H_

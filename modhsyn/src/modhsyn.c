#include "modhsyn.h"

#include "hsynsys.h"
#include "libsd.h"
#include "sdwrap.h"
#include "thbase.h"

unsigned int gTickRate;
struct HSyn_VoiceStat *gVoiceStat;
struct CslIdMonitor *gIdMonitor;
unsigned int gReservedVoices[2];
unsigned int gOutputMode;
unsigned int gMaxVoices;

unsigned int gSec;
unsigned int gUsec;

unsigned char gFreeVoiceCount[2];
unsigned int gPendingKon[2];
unsigned int gPendingKoff[2];
unsigned int gNoiseVoices[2];
struct VMix gVmixDry[2];
struct VMix gVmixWet[2];

struct HSynVoice gVoiceList[HSyn_NumCore][HSyn_NumVoice];

struct list_head gUnkList[2] = {
	[0] = LIST_HEAD_INIT(gUnkList[0]),
	[1] = LIST_HEAD_INIT(gUnkList[1]),
};

struct list_head gActiveVoices[2] = {
	[0] = LIST_HEAD_INIT(gActiveVoices[0]),
	[1] = LIST_HEAD_INIT(gActiveVoices[1]),
};

struct list_head gPendingVoices[2] = {
	[0] = LIST_HEAD_INIT(gPendingVoices[0]),
	[1] = LIST_HEAD_INIT(gPendingVoices[1]),
};

struct list_head gFreeVoices[2] = {
	[0] = LIST_HEAD_INIT(gFreeVoices[0]),
	[1] = LIST_HEAD_INIT(gFreeVoices[1]),
};

static char sStatusExtraByte[8] = {
	0,
	1,
	1,
	1,
	0,
	0,
	1,
	0,
};

int
voiceGC()
{
	struct HSynVoice *voice;
	u32 endx, envx;

	for (int core = 0; core < 2; core++) {
		endx = HSyn_GetSwitch(SD_SWITCH_ENDX | core);

		list_for_each (voice, &gActiveVoices[core], voiceList) {
			envx = HSyn_GetParam(SD_VPARAM_ENVX | SD_VOICE(core, voice->voiceId));

			voice->lastENVX = envx;
		}
	}
}

struct HSynVoice *
voiceAlloc(char attr)
{
	struct HSynVoice *voice;
	int core;

	attr &= SCEHD_SPU_CORE_0 | SCEHD_SPU_CORE_1;
	if (attr == SCEHD_SPU_CORE_0) {
		core = 0;
	} else if (attr == SCEHD_SPU_CORE_1) {
		core = 1;
	} else {
		// if not specified pick the one with most free voices
		core = gFreeVoiceCount[0] < gFreeVoiceCount[1];
	}

	if (!gFreeVoiceCount[core]) {
		return NULL;
	}

	gFreeVoiceCount[core]--;
	voice = list_first_entry(&gFreeVoices[core], struct HSynVoice, voiceList);
	list_remove(&voice->voiceList);
	return voice;
}

static void
statusNoteOn(struct HSynEnv *env, struct HSynChannel *chp, int port, unsigned char note,
	unsigned char velocity, unsigned char unk)
{
}

static void
statusNoteOff(struct HSynChannel *chp, unsigned char note, unsigned char unk)
{
}

static void
statusController(struct HSynEnv *env, int port, struct HSynChannel *chp, unsigned char controller,
	unsigned char value)
{
}

static void
statusProgramChange(struct HSynSystem *sys, struct HSynChannel *chp, unsigned char program)
{
}

static void
statusPitch(struct HSynChannel *chp, unsigned char phi, unsigned char plo)
{
	unsigned int pitch = plo + (phi << 7) - 0x2000;
}

static void
doChanStatus(struct HSynEnv *env, int port, unsigned char channel, unsigned char b1,
	unsigned char b2)
{
	struct HSynSystem *system = HSyn_GetSystem(env);
	struct HSynChannel *chp = &system->channel[channel & 0xf];
	unsigned char status = channel & 0xf0;

	switch (status) {
	case 0x80:
		statusNoteOff(chp, b1, 255);
		break;
	case 0x90:
		statusNoteOn(env, chp, port, b1, b2, 255);
		break;
	case 0xb0:
		statusController(env, port, chp, b1, b2);
		break;
	case 0xc0:
		statusProgramChange(system, chp, b1);
		break;
	case 0xe0:
		statusPitch(chp, b2, b1);
		break;
	}
}

static void
runHSyn(struct HSynEnv *env, int port, unsigned char *pos, unsigned char *end)
{
	unsigned char status, b1, b2;
	for (status = *pos; pos != end; pos++) {

		b1 = *pos++;
		if (sStatusExtraByte[(status - 0x80) >> 4]) {
			b2 = *pos++;
		}

		doChanStatus(env, port, status, b1, b2);
	}
}

static int
selectPort(struct CslCtx *ctx, int port, struct HSynEnv **envOut, struct HSynSystem **sysOut)
{
	if (!ctx) {
		return 0;
	}

	if (HSyn_EnvPortIdx(port) >= ctx->buffGrp[0].buffNum) {
		return 0;
	}

	if (!envOut || !sysOut) {
		return 0;
	}

	*envOut = HSyn_GetEnv(ctx, port);
	*sysOut = HSyn_GetSystem(*envOut);

	return 1;
}

static void
resetVunk1(struct HSynVoice *voice)
{
	voice->unkcc = 0;
	voice->unkd0 = 0;
	voice->unkce = 0;
	voice->unkd2 = 0;
	voice->unkd4 = 0;
	voice->unkd8 = 0;
	voice->unkdc = 0;
}

static void
resetVunk2(struct HSynVoice *voice)
{
	voice->unke0 = 0;
	voice->unke4 = 0;
	voice->unke2 = 0;
	voice->unke6 = 0;
	voice->unke8 = 0;
	voice->unkec = 0;
	voice->unkf0 = 0;
}

static void
resetVunk3(struct HSynVoice *voice)
{
	voice->unkf8 = -1;
	voice->unk120 = 128;
	voice->unk124 = 128;
	voice->unk128 = 1;
	voice->unkf4 = 0;
	voice->unkfc = 0;
	voice->unk100 = 0;
	voice->unk104 = 0;
	voice->unk108 = 0;
	voice->unk10c = 0;
	voice->unk110 = 0;
	voice->unk114 = 0;
	voice->unk118 = 0;
	voice->unk11c = 0;
}

static void
resetVunk4(struct HSynUnkVstruct *unk)
{
	unk->byte0 = 0;
	unk->word4 = 0;
	unk->word2 = 0;
	unk->word6 = 0;
	unk->dword8 = 0;
	unk->dwordC = 0;
	unk->dword10 = 0;
}

static void
resetVoices()
{
	struct HSynVoice *voice;

	list_init(&gUnkList[0]);
	list_init(&gUnkList[1]);
	list_init(&gActiveVoices[0]);
	list_init(&gActiveVoices[1]);
	list_init(&gPendingVoices[0]);
	list_init(&gPendingVoices[1]);
	list_init(&gFreeVoices[0]);
	list_init(&gFreeVoices[1]);

	for (int i = 0; i < HSyn_NumCore; i++) {
		for (int j = 0; j < HSyn_NumVoice; j++) {
			voice = &gVoiceList[i][j];

			voice->unk28 = -1;
			voice->unk56 = 0x80;
			voice->unkFlags = 0;
			voice->system = NULL;
			voice->channel = NULL;
			voice->program = NULL;
			voice->splitBlock = NULL;
			voice->sample = NULL;
			voice->vagParam = NULL;
			voice->unk78 = 0;
			voice->lastENVX = 0;
			voice->unk38 = 0;
			voice->priority = 0;
			voice->coreId = i;
			voice->voiceId = j;
			voice->unk55 = -1;
			voice->unk53 = 1;
			voice->unk44 = 255;
			voice->unk5c = -1;
			voice->unk5d = -1;
			voice->pitch = 0;

			resetVunk4(&voice->unk12c[0]);
			resetVunk4(&voice->unk12c[1]);

			list_insert(&gFreeVoices[i], &voice->voiceList);
		}
	}

	gFreeVoiceCount[0] = 24;
	gFreeVoiceCount[1] = 24;
	gPendingKoff[0] = 0;
	gPendingKoff[1] = 0;
	gPendingKon[0] = 0;
	gPendingKon[1] = 0;
	gNoiseVoices[0] = 0;
	gNoiseVoices[1] = 0;
	gVmixDry[0].left = 0;
	gVmixDry[0].right = 0;
	gVmixDry[1].left = 0;
	gVmixDry[1].right = 0;
	gVmixWet[0].left = 0;
	gVmixWet[0].right = 0;
	gVmixWet[1].left = 0;
	gVmixWet[1].right = 0;
}

static void
resetChannel(struct HSynChannel *ch, struct HSynEnv *env)
{
	list_init(&ch->voiceList1);
	list_init(&ch->voiceList3);
	ch->volume = 128;
	ch->expression = 128;
	list_init(&ch->voiceList2);
	ch->env = env;
	ch->bank = NULL;
	ch->damper = 0;
	ch->pitch = 0;
	ch->unk7 = 0;
	ch->unk8 = -1;
	ch->bankSel = 0;
	ch->unk12 = 0;
	ch->unk14 = -1;
	ch->unk15 = -1;
	ch->unk13 = 0;
	ch->pan = 0;
	ch->portamentSwitch = -1;
	ch->unk20 = -1;
	ch->unk19 = -1;
	ch->unk18 = -1;
}

static void
silenceChannel(struct HSynChannel *chp)
{
	struct HSynVoice *voice;

	list_for_each_safe (voice, &chp->voiceList1, chanVoiceList) {
		gPendingKoff[voice->coreId] |= 1 << voice->voiceId;
		HSyn_SetParam(SD_VPARAM_ADSR2 | SD_VOICE(voice->coreId, voice->voiceId), 0xc021);
	}
}

int
sceHSyn_AllSoundOff(struct CslCtx *ctx, unsigned int port)
{
	struct HSynSystem *system;
	struct HSynEnv *env;

	if (!selectPort(ctx, port, &env, &system)) {
		return -1;
	}

	for (int i = 0; i < 16; i++) {
		silenceChannel(&system->channel[i]);
	}
}

int
HSyn_Init(struct CslCtx *ctx, unsigned int interval)
{
	struct HSynEnv *env;
	struct HSynSystem *system;

	gVoiceStat = NULL;
	gTickRate = (interval << 8) / 1000;

	if (!ctx || !ctx->buffGrpNum) {
		return HSynError;
	}

	if (!ctx->buffGrp || (ctx->buffGrp[0].buffNum & 1) != 0) {
		return HSynError;
	}

	for (int i = 1; i < ctx->buffGrp[0].buffNum; i += 2) {
		if (!ctx->buffGrp[0].buffCtx->buff) {
			continue;
		}
		env = ctx->buffGrp[0].buffCtx[i].buff;
		env->maxPolyphony = 48;
		env->priority = 0;
		env->portMode = 0;
		env->waveType = 0;
		env->lfoWaveNum = 0;
		env->lfoWaveTbl = NULL;
		env->velocityMapNum = 0;
		env->velocityMapTbl = NULL;

		system = HSyn_GetSystem(env);
		system->volume = 0;
		system->polyphony = 0;
		for (int i = 0; i < 16; i++) {
			system->bank[i].header = NULL;
			system->bank[i].prog = NULL;
			system->bank[i].sampleSet = NULL;
			system->bank[i].sample = NULL;
			system->bank[i].vagInfo = NULL;
			system->bank[i].timbre = NULL;
			system->bank[i].spuAddr = 0;
		}

		for (int i = 0; i < 16; i++) {
			resetChannel(&system->channel[i], env);
		}
	}

	resetVoices();
	// FIXME more
	gReservedVoices[0] = 0;
	gReservedVoices[1] = 0;
	gOutputMode = 0;
	gMaxVoices = 0;
	gIdMonitor = NULL;

	return HSynNoError;
}

static int
checkTimeOverflow()
{
	iop_sys_clock_t clock;
	u32 sec, usec;

	GetSystemTime(&clock);
	SysClock2USec(&clock, &sec, &usec);

	if (sec == gSec) {
		usec -= gUsec;
	} else {
		--sec;
		if (sec == gSec) {
			return 1;
		}
		usec = usec + 1000000 - gUsec;
	}

	return usec >= 21;
}

static void
procSpuPendingSE(struct HSynVoice *voice)
{
}

static void
procSpuPendingHS(struct HSynVoice *voice)
{
}

static int
procPendingVoices()
{
	struct HSynVoice *voice;
	int pending_count = 0;

	for (int c = 0; c < 2; c++) {
		HSyn_SetSwitch(SD_SWITCH_KOFF | c, gPendingKoff[c]);

		list_for_each (voice, &gPendingVoices[c], voiceList) {
			if (voice->unk5c == 1 && voice->note == 255) {
				pending_count++;
				continue;
			}

			if (HSyn_GetParam(SD_VPARAM_ENVX | SD_VOICE(c, voice->voiceId)) >= 1000) {
				pending_count++;
				continue;
			}

			switch (voice->waveType) {
			case sceHSynTypeHSyn:
				procSpuPendingHS(voice);
				break;
			case sceHSynTypeSESyn:
				procSpuPendingSE(voice);
				break;
			}

			list_remove(&voice->voiceList);
			list_insert(&gActiveVoices[2], &voice->voiceList);
		}

		HSyn_SetSwitch(SD_SWITCH_KON | c, gPendingKon[c]);
	}

	return pending_count;
}

int
HSyn_ATick(struct CslCtx *ctx)
{
	return HSynNoError;
}

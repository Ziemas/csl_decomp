#include "modhsyn.h"

#include "hsynsys.h"
#include "libsd.h"
#include "sdwrap.h"
#include "thbase.h"

#include <stdio.h>

unsigned int gTickRate;
struct HSyn_VoiceStat *gVoiceStat;
struct CslIdMonitor *gIdMonitor;
unsigned int gReservedVoices[2];
unsigned int gOutputMode;
unsigned int gMaxVoices;

unsigned int gSec;
unsigned int gUsec;

unsigned char gUnkLastCore;
unsigned char gUnkLastVoice;

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

static const char sStatusExtraByte[8] = {
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
hasMonitor()
{
	if (gIdMonitor) {
		return 0;
	}

	return -1;
}

static void
updateMonitor()
{
}

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

	// TODO
	return 0;
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
statusNoteOn(struct HSynEnv *env, struct HSynChannel *chan, int port, unsigned char note,
	unsigned char velocity, unsigned char unk)
{
	HardSynthSampleSetChunk *sset;
	HardSynthProgramParam *program;
	HardSynthSplitBlock *split;

	if (!chan->program) {
		return;
	}

	sset = chan->bank->sampleSet;
	program = chan->program;
	split = (HardSynthSplitBlock *)((char *)program + program->splitBlockAddr);

	if (!program->nSplit) {
		return;
	}

	for (int i = 0; i < program->nSplit; i++) {
		if (note < split[i].splitRangeLow || note >= split[i].splitBendRangeHigh) {
			continue;
		}

		if (split->sampleSetIndex > sset->maxSampleSetNumber) {
			continue;
		}

		if (sset->sampleSetOffsetAddr[split->sampleSetIndex] == -1) {
			continue;
		}
	}
}

static void
statusNoteOff(struct HSynChannel *chan, unsigned char note, unsigned char unk)
{
}

static void
statusController(struct HSynEnv *env, int port, struct HSynChannel *chan, unsigned char controller,
	unsigned char value)
{
	printf("controller %d %d\n", controller, value);
	switch (controller) {
	case 0:
		chan->bankSel = value;
		break;
	case 7:
		break;
	}
}

static void
statusProgramChange(struct HSynSystem *sys, struct HSynChannel *chan, unsigned char program)
{
	HardSynthProgramParam *prog;
	struct HSynBank *bank;
	unsigned int offset;

	prog = NULL;
	bank = NULL;

	if (chan->bankSel < 16) {
		bank = &sys->bank[chan->bankSel];
		if (bank->prog && bank->prog->maxProgramNumber >= program) {
			offset = bank->prog->programOffsetAddr[program];
			if (offset != -1) {
				prog = (HardSynthProgramParam *)(((char *)&bank->prog) + offset);
				if (!prog->nSplit) {
					prog = NULL;
				}
			}
		}
	}

	chan->program = prog;
	if (prog) {
		chan->bank = bank;
	} else {
		chan->bank = NULL;
	}
}

static void
statusPitch(struct HSynChannel *chp, unsigned char phi, unsigned char plo)
{
	unsigned int pitch = plo + (phi << 7) - 0x2000;
}

static void
doChanStatus(struct HSynEnv *env, int port, unsigned char status, unsigned char b1,
	unsigned char b2)
{
	struct HSynSystem *system = HSyn_GetSystem(env);
	struct HSynChannel *chp = &system->channel[status & 0xf];
	unsigned char cmd = status & 0xf0;

	switch (cmd) {
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
runSEyn(struct HSynEnv *env, int port, unsigned char *pos, unsigned char *end)
{
}

static void
runHSyn(struct HSynEnv *env, int port, unsigned char *pos, unsigned char *end)
{
	int status, b1, b2;
	while (pos < end) {
		if (*pos & 0x80) {
			status = *pos++;
		}
		printf("status %x\n", status);

		if (status < 0xf0) {
			b1 = *pos++;

			if (sStatusExtraByte[(status - 0x80) >> 4]) {
				b2 = *pos++;
			}

			if (pos >= end)
				return;

			doChanStatus(env, port, status, b1, b2);
		}
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
	voice->unkcc[0].byte0 = 0;
	voice->unkcc[0].word4 = 0;
	voice->unkcc[0].word2 = 0;
	voice->unkcc[0].word6 = 0;
	voice->unkcc[0].dword8 = 0;
	voice->unkcc[0].dwordC = 0;
	voice->unkcc[0].dword10 = 0;
}

static void
resetVunk2(struct HSynVoice *voice)
{
	voice->unkcc[1].byte0 = 0;
	voice->unkcc[1].word4 = 0;
	voice->unkcc[1].word2 = 0;
	voice->unkcc[1].word6 = 0;
	voice->unkcc[1].dword8 = 0;
	voice->unkcc[1].dwordC = 0;
	voice->unkcc[1].dword10 = 0;
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
			voice->sampleAddress = 0;
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
	list_init(&ch->workingVoices);
	list_init(&ch->deadVoices);
	ch->volume = 128;
	ch->expression = 128;
	list_init(&ch->dampedVoices);
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

	list_for_each_safe (voice, &chp->workingVoices, chanVoiceList) {
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

	return 0;
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
	gUnkLastVoice = -1;
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
voiceProcVolume(struct HSynVoice *voice)
{
}

static void
voiceProcPitch(struct HSynVoice *voice)
{
}

static void
procSpuPendingSE(struct HSynVoice *voice)
{
}

static unsigned int
calcADSR1(struct HSynVoice *voice, HardSynthSampleParam *sample)
{
}

static unsigned int
calcADSR2(struct HSynVoice *voice, HardSynthSampleParam *sample)
{
}

static void
procSpuPendingHS(struct HSynVoice *voice)
{
	HardSynthSampleParam *sample;
	iop_sys_clock_t clock;
	int entry;

	sample = voice->vagParam;
	entry = SD_VOICE(voice->coreId, voice->voiceId);

	voiceProcVolume(voice);

	if (voice->vagParam) {
		gNoiseVoices[voice->coreId] &= ~(1 << voice->voiceId);
		voiceProcPitch(voice);
		HSyn_SetAddr(SD_VADDR_SSA | entry, voice->sampleAddress + voice->vagParam->vagOffsetAddr);
		HSyn_SetParam(SD_VPARAM_ADSR1 | entry, calcADSR1(voice, voice->sample));
		HSyn_SetParam(SD_VPARAM_ADSR2 | entry, calcADSR2(voice, voice->sample));
		if ((voice->vagParam->vagAttribute & 1) == 0) {
			voice->unkFlags |= 1;
		}
	} else {
		HSyn_SetCoreAttr(SD_CORE_NOISE_CLK, max(voice->note, 63));
		gNoiseVoices[voice->coreId] |= BIT(voice->voiceId);
	}

	HSyn_SetSwitch(SD_SWITCH_NON | voice->coreId, gNoiseVoices[voice->coreId]);

	if (sample->sampleSpuAttr & SCEHD_SPU_DIRECTSEND_L) {
		gVmixDry[voice->coreId].left |= BIT(voice->voiceId);
	} else {
		gVmixDry[voice->coreId].left &= ~BIT(voice->voiceId);
	}
	if (sample->sampleSpuAttr & SCEHD_SPU_DIRECTSEND_R) {
		gVmixDry[voice->coreId].right |= BIT(voice->voiceId);
	} else {
		gVmixDry[voice->coreId].right &= ~BIT(voice->voiceId);
	}
	if (sample->sampleSpuAttr & SCEHD_SPU_EFFECTSEND_L) {
		gVmixWet[voice->coreId].left |= BIT(voice->voiceId);
	} else {
		gVmixWet[voice->coreId].left &= ~BIT(voice->voiceId);
	}
	if (sample->sampleSpuAttr & SCEHD_SPU_EFFECTSEND_R) {
		gVmixWet[voice->coreId].right |= BIT(voice->voiceId);
	} else {
		gVmixWet[voice->coreId].right &= ~BIT(voice->voiceId);
	}

	HSyn_SetSwitch(SD_SWITCH_VMIXL | voice->coreId, gVmixDry[voice->coreId].left);
	HSyn_SetSwitch(SD_SWITCH_VMIXR | voice->coreId, gVmixDry[voice->coreId].right);
	HSyn_SetSwitch(SD_SWITCH_VMIXEL | voice->coreId, gVmixWet[voice->coreId].left);
	HSyn_SetSwitch(SD_SWITCH_VMIXER | voice->coreId, gVmixWet[voice->coreId].right);
	gPendingKon[voice->coreId] |= BIT(voice->voiceId);
	gUnkLastCore = voice->voiceId;
	gUnkLastVoice = voice->voiceId;

	GetSystemTime(&clock);
	SysClock2USec(&clock, &gSec, &gUsec);

	voice->unk4b = 3;
	voice->unkFlags &= ~8;

	if (gVoiceStat) {
		gVoiceStat->voice_state[voice->coreId][voice->voiceId] &= VoiceStat_State;
		gVoiceStat->voice_state[voice->coreId][voice->voiceId] |= FIELD_PREP(VoiceStat_State,
			VoiceStat_State_KeyOn);
	}
}

static int
procWorkingVoices()
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

static void
procStream(struct CslBuffCtx *ctx, int bufCount)
{
	unsigned char *str_start, *str_end;
	struct CslMidiStream *stream;
	struct HSynEnv *env;

	if (bufCount < 2) {
		return;
	}

	for (int i = 0, port = 0; i < bufCount; i += 2, port++) {
		stream = (struct CslMidiStream *)ctx[HSyn_StrPortIdx(port)].buff;
		env = (struct HSynEnv *)ctx[HSyn_EnvPortIdx(port)].buff;

		if (!stream || !env) {
			continue;
		}

		str_start = &stream->data[0];
		str_end = &stream->data[stream->validsize];

		if (str_start >= str_end) {
			continue;
		}

		stream->validsize = 0;

		switch (env->portMode) {
		case sceHSynModeSESyn:
			runSEyn(env, port, str_start, str_end);
			break;
		case sceHSynModeHSyn:
			runHSyn(env, port, str_start, str_end);
			break;
		}
	}
}

int
HSyn_ATick(struct CslCtx *ctx)
{
	int pending, working;

	if (gUnkLastVoice == 0xff) {
		if (checkTimeOverflow()) {
			gUnkLastVoice = 0xff;
		}
	}

	// We want to update the vmix state, but not trample
	// user reserved bits. So get current state.
	for (int i = 0; i < 2; i++) {
		gVmixDry[i].left &= ~gReservedVoices[i];
		gVmixDry[i].right &= ~gReservedVoices[i];
		gVmixWet[i].left &= ~gReservedVoices[i];
		gVmixWet[i].right &= ~gReservedVoices[i];

		if (gReservedVoices[i]) {
			gVmixDry[i].left |= HSyn_GetSwitch(SD_SWITCH_VMIXL | i) & gReservedVoices[i];
			gVmixDry[i].right |= HSyn_GetSwitch(SD_SWITCH_VMIXR | i) & gReservedVoices[i];
			gVmixWet[i].left |= HSyn_GetSwitch(SD_SWITCH_VMIXEL | i) & gReservedVoices[i];
			gVmixWet[i].right |= HSyn_GetSwitch(SD_SWITCH_VMIXER | i) & gReservedVoices[i];
		}
	}

	procStream(ctx->buffGrp[0].buffCtx, ctx->buffGrp[0].buffNum);

	pending = procPendingVoices();
	working = procWorkingVoices();

	if (gVoiceStat) {
		gVoiceStat->pendingVoiceCount = pending;
		gVoiceStat->workVoiceCount = working;
	}

	if (hasMonitor() != 0) {
		updateMonitor();
	}

	return HSynNoError;
}

static int
validateChunk(unsigned int sig1, unsigned int sig2, const char *expected1, const char *expected2)
{
	char *s1;
	char *s2;

	s1 = &sig1;
	s2 = &sig2;

	for (int i = 0; i < 4; i++) {
		if (s1[3 - i] != expected1[i]) {
			return 0;
		}
		if (s2[3 - i] != expected2[i]) {
			return 0;
		}
	}

	return 1;
}

int
HSyn_Load(struct CslCtx *ctx, int port, unsigned spu_addr, void *bankdata, int bankidx)
{
	HardSynthHeaderChunk *header;
	HardSynthVersionChunk *ver;
	struct HSynSystem *sys;
	struct HSynBank *bank;
	struct HSynEnv *env;

	if (!selectPort(ctx, port, &env, &sys)) {
		return HSynError;
	}

	if (env->waveType == sceHSynTypeSESyn) {
		bankidx = 0;
	}

	bank = &sys->bank[bankidx];
	bank->header = NULL;
	bank->prog = NULL;
	bank->sampleSet = NULL;
	bank->sample = NULL;
	bank->vagInfo = NULL;
	bank->timbre = NULL;
	bank->spuAddr = 0;

	if (spu_addr && bankdata) {
		ver = bankdata;
		header = bankdata + ver->chunkSize;

		bank->header = header;
		if (header->programChunkAddr != -1) {
			bank->prog = bankdata + header->programChunkAddr;
		}

		if (header->sampleSetChunkAddr != -1) {
			bank->sampleSet = bankdata + header->sampleSetChunkAddr;
		}

		if (header->sampleChunkAddr != -1) {
			bank->sample = bankdata + header->sampleChunkAddr;
		}

		if (header->vagInfoChunkAddr != -1) {
			bank->vagInfo = bankdata + header->vagInfoChunkAddr;
		}

		if (ver->versionMajor >= 2) {
			if (header->seTimbreChunkAddr != -1) {
				bank->timbre = bankdata + header->seTimbreChunkAddr;
			}
		}

		if (!validateChunk(ver->Creator, ver->Type, "SCEI", "Vers")) {
			goto err;
		}

		if (!validateChunk(header->Creator, header->Type, "SCEI", "Head")) {
			goto err;
		}

		if (bank->prog && !validateChunk(bank->prog->Creator, bank->prog->Type, "SCEI", "Prog")) {
			goto err;
		}

		if (bank->sampleSet &&
			!validateChunk(bank->sampleSet->Creator, bank->sampleSet->Type, "SCEI", "Sset")) {
			goto err;
		}

		if (bank->sample &&
			!validateChunk(bank->sample->Creator, bank->sample->Type, "SCEI", "Smpl")) {
			goto err;
		}

		if (bank->vagInfo &&
			!validateChunk(bank->vagInfo->Creator, bank->vagInfo->Type, "SCEI", "Vagi")) {
			goto err;
		}

		if (bank->timbre &&
			!validateChunk(bank->timbre->Creator, bank->timbre->Type, "SCEI", "Setb")) {
			goto err;
		}

		bank->spuAddr = spu_addr;
	}

	for (int i = 0; i < 16; i++) {
		if (sys->channel[i].bank == bank) {
			sys->channel[i].bank = NULL;
			sys->channel[i].program = NULL;
		}
	}

	return HSynNoError;

err:
	printf("failed compare\n");

	bank->header = NULL;
	bank->prog = NULL;
	bank->sampleSet = NULL;
	bank->sample = NULL;
	bank->vagInfo = NULL;
	bank->timbre = NULL;
	bank->spuAddr = 0;

	return HSynError;
}

int
HSyn_VoiceTrans(short chan, unsigned char *iopaddr, unsigned int *spuaddr, unsigned int size)
{
	unsigned int sent, aligned;

	aligned = ALIGN_UP(size, 64);
	HSyn_VoiceTransStatus(chan, 1);
	sent = HSyn_VoiceTransW(chan, 0, iopaddr, spuaddr, size);
	HSyn_VoiceTransStatus(chan, 1);

	if (sent != aligned) {
		return HSynError;
	}

	return HSynNoError;
}

int
HSyn_SetVolume(struct CslCtx *ctx, unsigned int port, unsigned short volume)
{
	struct HSynSystem *sys;
	struct HSynEnv *env;

	if (!selectPort(ctx, port, &env, &sys)) {
		return HSynError;
	}

	sys->volume = volume;

	return HSynNoError;
}

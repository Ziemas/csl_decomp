#include "modhsyn.h"

#include <libsd.h>

static struct HSyn_DebugInfo *sDebugInfo;

int
HSyn_SetDebugInfoBuffer(struct HSyn_DebugInfo *buffer)
{
	sDebugInfo = buffer;
	if (buffer) {
		buffer->writeIndex = 0;
		buffer->readIndex = 0;
	}

	return 0;
}

static struct HSyn_SdCall *
getCallSlot()
{
	struct HSyn_SdCall *ret;
	if (!sDebugInfo) {
		return NULL;
	}

	ret = &sDebugInfo->sdCall[sDebugInfo->writeIndex++];
	if (sDebugInfo->writeIndex >= sDebugInfo->infoBlkNum) {
		sDebugInfo->writeIndex = 0;
	}

	return ret;
}

static void
logSdCall(struct HSyn_SdCall *call, int func, int arg1, int arg2, int arg3,
	int arg4, int arg5)
{
	if (call) {
		call->func = func | 0x80000000;
		call->arg[0] = arg1;
		call->arg[1] = arg2;
		call->arg[2] = arg3;
		call->arg[3] = arg4;
		call->arg[4] = arg5;
	}
}

static void
logSdReturn(struct HSyn_SdCall *call, unsigned int ret)
{
	if (call) {
		call->func &= 0x7fffffff;
		call->retVal = ret;
	}
}

unsigned int
HSyn_SdInit(int flag)
{
	struct HSyn_SdCall *call = getCallSlot();
	int ret;

	logSdCall(call, 0, flag, 0, 0, 0, 0);
	ret = sceSdInit(flag);
	logSdReturn(call, ret);

	return ret;
}

void
HSyn_SetParam(unsigned short entry, unsigned short value)
{
	struct HSyn_SdCall *call = getCallSlot();

	logSdCall(call, 1, entry, value, 0, 0, 0);
	sceSdSetParam(entry, value);
	logSdReturn(call, 0);
}

unsigned int
HSyn_GetParam(unsigned short entry)
{
	struct HSyn_SdCall *call = getCallSlot();
	unsigned int ret;

	logSdCall(call, 2, entry, 0, 0, 0, 0);
	ret = sceSdGetParam(entry);
	logSdReturn(call, ret);

	return ret;
}

void
HSyn_SetSwitch(unsigned short entry, unsigned int value)
{
	struct HSyn_SdCall *call = getCallSlot();

	logSdCall(call, 3, entry, value, 0, 0, 0);
	sceSdSetSwitch(entry, value);
	logSdReturn(call, 0);
}

unsigned int
HSyn_GetSwitch(unsigned short entry)
{
	struct HSyn_SdCall *call = getCallSlot();
	unsigned int ret;

	logSdCall(call, 4, entry, 0, 0, 0, 0);
	ret = sceSdGetSwitch(entry);
	logSdReturn(call, ret);

	return ret;
}

void
HSyn_SetAddr(unsigned short entry, unsigned int value)
{
	struct HSyn_SdCall *call = getCallSlot();

	logSdCall(call, 5, entry, (int)value, 0, 0, 0);
	sceSdSetAddr(entry, value);
	logSdReturn(call, 0);
}

unsigned int
HSyn_GetAddr(unsigned short entry)
{
	struct HSyn_SdCall *call = getCallSlot();
	unsigned int ret;

	logSdCall(call, 6, entry, 0, 0, 0, 0);
	ret = sceSdGetAddr(entry);
	logSdReturn(call, ret);

	return ret;
}

void
HSyn_SetCoreAttr(unsigned short entry, unsigned int value)
{
	struct HSyn_SdCall *call = getCallSlot();

	logSdCall(call, 7, entry, (int)value, 0, 0, 0);
	sceSdSetCoreAttr(entry, value);
	logSdReturn(call, 0);
}

unsigned int
HSyn_Note2Pitch(unsigned short center_note, unsigned short center_fine,
	unsigned short note, short fine)
{
	struct HSyn_SdCall *call = getCallSlot();
	unsigned int ret;

	logSdCall(call, 9, center_note, center_fine, note, fine, 0);
	ret = sceSdNote2Pitch(center_note, center_fine, note, fine);
	logSdReturn(call, ret);

	return ret;
}

unsigned short
HSyn_Pitch2Note(unsigned short center_note, unsigned short center_fine,
	unsigned short pitch)
{
	struct HSyn_SdCall *call = getCallSlot();
	unsigned int ret;

	logSdCall(call, 10, center_note, center_fine, pitch, 0, 0);
	ret = sceSdPitch2Note(center_note, center_fine, pitch);
	logSdReturn(call, ret);

	return ret;
}

int
HSyn_ProcBatch(sceSdBatch *batch, unsigned int *returns, unsigned int num)
{
	struct HSyn_SdCall *call = getCallSlot();
	int ret;

	logSdCall(call, 11, (int)batch, (int)returns, (int)num, 0, 0);
	ret = sceSdProcBatch(batch, (unsigned long *)returns, num);
	logSdReturn(call, ret);

	return ret;
}

int
HSyn_ProcBatchEx(sceSdBatch *batch, unsigned int *returns, unsigned int num,
	unsigned int voice)
{
	struct HSyn_SdCall *call = getCallSlot();
	int ret;

	logSdCall(call, 12, (int)batch, (int)returns, (int)num, (int)voice, 0);
	ret = sceSdProcBatchEx(batch, (unsigned long *)returns, num, voice);
	logSdReturn(call, ret);

	return ret;
}

int
HSyn_VoiceTransW(short chan, unsigned short mode, unsigned char *iopaddr,
	unsigned int *spuaddr, unsigned int size)
{
	struct HSyn_SdCall *call = getCallSlot();
	int ret;

	logSdCall(call, 13, chan, mode, (int)iopaddr, (int)spuaddr, (int)size);
	ret = sceSdVoiceTrans(chan, mode, iopaddr, (unsigned long *)spuaddr, size);
	logSdReturn(call, ret);

	return ret;
}

int
HSyn_BlockTrans(short chan, unsigned short mode, unsigned char *iopaddr,
	unsigned int size)
{
	struct HSyn_SdCall *call = getCallSlot();
	int ret;

	logSdCall(call, 14, chan, mode, (int)iopaddr, (int)size, 0);
	ret = sceSdBlockTrans(chan, mode, iopaddr, size);
	logSdReturn(call, ret);

	return ret;
}

unsigned int
HSyn_VoiceTransStatus(short channel, short flag)
{
	struct HSyn_SdCall *call = getCallSlot();
	unsigned int ret;

	logSdCall(call, 15, channel, flag, 0, 0, 0);
	ret = sceSdVoiceTransStatus(channel, flag);
	logSdReturn(call, ret);

	return ret;
}

unsigned int
HSyn_BlockTransStatus(short channel, short flag)
{
	struct HSyn_SdCall *call = getCallSlot();
	unsigned int ret;

	logSdCall(call, 16, channel, flag, 0, 0, 0);
	ret = sceSdBlockTransStatus(channel, flag);
	logSdReturn(call, ret);

	return ret;
}

int
HSyn_SetEffectAttr(int core, sceSdEffectAttr *attr)
{
	struct HSyn_SdCall *call = getCallSlot();
	int ret;

	logSdCall(call, 19, core, (int)attr, 0, 0, 0);
	ret = sceSdSetEffectAttr(core, attr);
	logSdReturn(call, ret);

	return ret;
}

void
HSyn_GetEffectAttr(int core, sceSdEffectAttr *attr)
{
	struct HSyn_SdCall *call = getCallSlot();

	logSdCall(call, 20, core, (int)attr, 0, 0, 0);
	sceSdGetEffectAttr(core, attr);
	logSdReturn(call, 0);
}

int
HSyn_ClearEffectWorkArea(int core, int channel, int effect_mode)
{
	struct HSyn_SdCall *call = getCallSlot();
	int ret;

	logSdCall(call, 21, core, channel, effect_mode, 0, 0);
	ret = sceSdClearEffectWorkArea(core, channel, effect_mode);
	logSdReturn(call, ret);

	return ret;
}

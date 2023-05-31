#ifndef SDWRAP_H_
#define SDWRAP_H_

int HSyn_SdInit(int flag);
void HSyn_SetParam(unsigned short entry, unsigned short value);
unsigned int HSyn_GetParam(unsigned short entry);
void HSyn_SetSwitch(unsigned short entry, unsigned short value);
unsigned int HSyn_GetSwitch(unsigned short entry);
void HSyn_SetAddr(unsigned short entry, unsigned short value);
unsigned int HSyn_GetAddr(unsigned short entry);
void HSyn_SetCoreAttr(unsigned short entry, unsigned int value);
unsigned int HSyn_Note2Pitch(unsigned short center_note,
	unsigned short center_fine, unsigned short note, short fine);
unsigned short HSyn_Pitch2Note(unsigned short center_note,
	unsigned short center_fine, unsigned short pitch);
int HSyn_ProcBatch(sceSdBatch *batch, unsigned int returns[], unsigned int num);
int HSyn_ProcBatchEx(sceSdBatch *batch, unsigned int returns[],
	unsigned int num, unsigned int voice);
int HSyn_VoiceTransW(short chan, unsigned short mode, unsigned char *iopaddr,
	unsigned int *spuaddr, unsigned int size);
int HSyn_BlockTrans(short chan, unsigned short mode, unsigned char *iopaddr,
	unsigned int size);
unsigned int HSyn_VoiceTransStatus(short channel, short flag);
unsigned int HSyn_BlockTransStatus(short channel, short flag);
int HSyn_SetEffectAttr(int core, sceSdEffectAttr *attr);
void HSyn_GetEffectAttr(int core, sceSdEffectAttr *attr);
int HSyn_ClearEffectWorkArea(int core, int channel, int effect_mode);

#endif // SDWRAP_H_

#ifndef MODHSYNSYS_H_
#define MODHSYNSYS_H_

#include "hd.h"
#include "list.h"
#include "modhsyn.h"

#define HSyn_GetSystem(env) (struct HSynSystem *)((env)->system)

enum {
	sceHSynModeHSyn = 0,
	sceHSynModeSESyn = 1,
	sceHSynTypeHSyn = 0,
	sceHSynTypeSESyn = 1,
	sceHSynTypeProgram = 0,
	sceHSynTypeTimbre = 1,
	sceHSynModeNone = 0xff,
};

struct VMix {
	unsigned int left;
	unsigned int right;
};

struct HSynUnkVstruct {
	char byte0;
	short word2;
	short word4;
	short word6;
	int dword8;
	int dwordC;
	int dword10;
};

struct HSynVoice {
	struct list_head voiceList;
	struct list_head chanVoiceList;
	unsigned short unkFlags;
	unsigned short lastENVX;
	unsigned short priority;
	short unk28;
	unsigned short pitch;
	unsigned int unk38;
	unsigned char coreId;
	unsigned char voiceId;
	unsigned char note;
	unsigned int unk44;
	unsigned char unk53;
	char unk55;
	unsigned char unk56;
	char unk5c;
	char unk5d;
	char waveType;
	struct HSynSystem *system;
	struct channelParams *channel;
	struct sceHardSynthProgramParam *program;
	struct sceHardSynthSplitBlock *splitBlock;
	struct sceHardSynthSampleParam *sample;
	struct sceHardSynthVagParam *vagParam;
	int unk78;

	//----- substruct array?
	unsigned char unkcc;
	unsigned short unkce;
	unsigned short unkd0;
	unsigned short unkd2;
	unsigned int unkd4;
	unsigned int unkd8;
	unsigned int unkdc;

	// ---
	unsigned char unke0;
	unsigned short unke2;
	unsigned short unke4;
	unsigned short unke6;
	unsigned int unke8;
	unsigned int unkec;
	unsigned int unkf0;

	// ---
	unsigned int unkf4;
	unsigned int unkf8;
	unsigned int unkfc;
	unsigned int unk100;
	unsigned int unk104;
	unsigned int unk108;
	unsigned int unk10c;
	unsigned int unk110;
	unsigned int unk114;
	unsigned int unk118;
	unsigned int unk11c;
	unsigned int unk120;
	unsigned int unk124;
	unsigned int unk128;
	struct HSynUnkVstruct unk12c[2];
};

struct HSynChannel {
	struct list_head voiceList1;
	struct list_head voiceList2;
	struct list_head voiceList3;
	sceHardSynthProgramParam *program;
	struct HSynEnv *env;
	struct HSynBank *bank;
	short unk7;
	short pitch;
	short unk8;
	char bankSel;
	unsigned char volume;
	unsigned char expression;
	char unk12;
	char unk13;
	char unk14;
	char unk15;
	char pan;
	char damper;
	unsigned char portamentSwitch;
	char portamentCtrl;
	char unk18;
	char unk19;
	char unk20;
};

struct HSynBank {
	struct HardSynthHeaderChunk *header;
	struct HardSynthProgramChunk *prog;
	struct HardSynthSampleSetChunk *sampleSet;
	struct HardSynthSampleChunk *sample;
	struct HardSynthVagInfoChunk *vagInfo;
	struct HardSynthSeTimbreChunk *timbre;
	unsigned int spuAddr;
};

struct HSynSystem {
	unsigned short volume;
	unsigned char polyphony;
	struct HSynChannel channel[16];
	struct HSynBank bank[16];
};

#endif // MODHSYNSYS_H_

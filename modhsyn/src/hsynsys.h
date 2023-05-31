#ifndef MODHSYNSYS_H_
#define MODHSYNSYS_H_

#include "hd.h"
#include "list.h"
#include "modhsyn.h"

#define HSyn_GetSystem(env) (struct HSynSystem *)((env)->system)

#define ALIGN_UP(x, a) (((x) + ((a)-1)) & ~((a)-1))
#define ALIGN_DOWN(x, a) ((x) & ~((a)-1))

#define BIT(b) (1 << (b))
#define MASK(x) (BIT(x) - 1)
#define GENMASK(msb, lsb) ((BIT((msb + 1) - (lsb)) - 1) << (lsb))
#define _FIELD_LSB(field) ((field) & ~(field - 1))
#define FIELD_PREP(field, val) ((val) * (_FIELD_LSB(field)))
#define FIELD_GET(field, val) (((val) & (field)) / _FIELD_LSB(field))

#define VoiceStat_Valid BIT(7)
#define VoiceStat_State GENMASK(6, 4)
#define VoiceStat_Port GENMASK(3, 0)

#define VoiceStat_State_Free 0
#define VoiceStat_State_Pending 1
#define VoiceStat_State_KeyOn 2
#define VoiceStat_State_KeyOff 3

#define max(a, b)               \
	({                          \
		__typeof__(a) _a = (a); \
		__typeof__(b) _b = (b); \
		_a > _b ? _a : _b;      \
	})

#define min(a, b)               \
	({                          \
		__typeof__(a) _a = (a); \
		__typeof__(b) _b = (b); \
		_a < _b ? _a : _b;      \
	})

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
	unsigned char unk4b;
	unsigned char unk53;
	char unk55;
	unsigned char unk56;
	char unk5c;
	char unk5d;
	char waveType;
	struct HSynSystem *system;
	struct channelParams *channel;
	HardSynthProgramParam *program;
	HardSynthSplitBlock *splitBlock;
	HardSynthSampleParam *sample;
	HardSynthVagParam *vagParam;
	int sampleAddress;

	struct HSynUnkVstruct unkcc[2];

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
	union {
		struct {
			struct list_head workingVoices;
			struct list_head dampedVoices;
			struct list_head deadVoices;
		};
		struct list_head voiceLists[3];
	};
	HardSynthProgramParam *program;
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
	HardSynthHeaderChunk *header;
	HardSynthProgramChunk *prog;
	HardSynthSampleSetChunk *sampleSet;
	HardSynthSampleChunk *sample;
	HardSynthVagInfoChunk *vagInfo;
	HardSynthSeTimbreChunk *timbre;
	unsigned int spuAddr;
};

struct HSynSystem {
	unsigned short volume;
	unsigned char polyphony;
	struct HSynChannel channel[16];
	struct HSynBank bank[16];
};

#endif // MODHSYNSYS_H_

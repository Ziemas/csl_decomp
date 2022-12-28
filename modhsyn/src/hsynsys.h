#ifndef MODHSYNSYS_H_
#define MODHSYNSYS_H_

#include "list.h"

#define HSyn_GetSystem(env) (struct HSynSystem *)((env)->system)

struct HSynVoice {
	struct ListElement voiceList;
	struct ListElement chanVoiceList;
	unsigned char coreId;
	unsigned char voiceId;
};

struct HSynChannel {
	struct ListElement unk;
	struct ListElement voices;
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

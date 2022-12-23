#ifndef MIDISYS_H_
#define MIDISYS_H_

#include "modmidi.h"
#include "sq.h"

#define Midi_GetSystem(env) (struct MidiSystem *)((env)->system)

struct loopStackEntry {
	unsigned char loopID;
	unsigned char loopCount;
	unsigned char runningStatus;
	unsigned char unk2;
	unsigned int unk3;
	unsigned char *seqPos;
};

struct MidiSystem {
	struct SeqHeaderChunk *sqHeader;
	struct SeqMidiChunk *sqMidi;
	struct SeqSongChunk *sqSong;
	char *sequenceData;
	unsigned char masterVolume;

	struct SeqMidiCompBlock *sqCompBlock;

	unsigned int usecPerQuarter;
	unsigned int relativeTempo;
	int Division;

	unsigned short unk211;
	unsigned char channelParams[MidiNumMidiCh];
	struct loopStackEntry loopStack[8];
};

#endif // MIDISYS_H_

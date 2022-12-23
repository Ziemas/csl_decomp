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

struct markEntry {
	unsigned char type;
	unsigned char data;
	unsigned char dataMode;
	unsigned char count;
};

struct channelParams {
	unsigned char program;
	unsigned char bank;
	unsigned char pitchModDepth;
	unsigned char ampModDepth;
	unsigned char portamentTime;
	char volume;
	unsigned char pan;
	unsigned char expression;
	unsigned char damper;
	unsigned char portamentSwitch;
	unsigned short pitchBend;
};

struct MidiSystem {
	struct SeqHeaderChunk *sqHeader;
	struct SeqMidiChunk *sqMidi;
	struct SeqSongChunk *sqSong;
	struct SeqMidiCompBlock *sqCompBlock;
	char *sequenceData;
	char *seqPosition;
	unsigned int time;
	int currentTick;
	int usecPerPPQN;
	int Division;
	unsigned int usecPerQuarter;
	unsigned int relativeTempo;
	unsigned char runningStatus;
	char unk;
	unsigned char masterVolume;
	struct markEntry markEntry;

	unsigned char unkPerChanVolume[MidiNumMidiCh];
	unsigned char chanelVolume[MidiNumMidiCh];
	struct channelParams chParams[MidiNumMidiCh];

	unsigned short unk211;
	unsigned char channelParams[MidiNumMidiCh];
	struct loopStackEntry loopStack[8];
};

#endif // MIDISYS_H_

#ifndef MIDISYS_H_
#define MIDISYS_H_

#include "modmidi.h"
#include "sq.h"

#define Midi_GetSystem(env) (struct MidiSystem *)((env)->system)

enum {
	LoopFree = 0xFF,

	MarkType_LoopStart = 0x0,
	MarkType_LoopEnd = 0x1,
	MarkType_MSB7 = 0x11,
	MarkType_MSB14 = 0x12,
};

struct loopTableEntry {
	unsigned char loopID;
	unsigned char loopCount;
	unsigned char runningStatus;
	unsigned char skipDelta;
	unsigned int tick;
	unsigned char *seqPos;
};

struct markState {
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
	unsigned char volume;
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
	unsigned char *sequenceData;
	unsigned char *seqPosition;
	unsigned int tick;
	int tickRemainder;
	unsigned int usecPerPPQN;
	unsigned int Division;
	unsigned int usecPerQuarter;
	unsigned int relativeTempo;
	unsigned char runningStatus;
	unsigned char skipDelta;
	unsigned char masterVolume;
	struct markState mark;

	unsigned short activeChan;

	unsigned char unkPerChanVolume[MidiNumMidiCh];
	unsigned char chanelVolume[MidiNumMidiCh];
	struct channelParams chParams[MidiNumMidiCh];

	unsigned short unk211;
	unsigned char channelParams[MidiNumMidiCh];
	struct loopTableEntry loopTable[8];
};

#endif // MIDISYS_H_

#ifndef MIDISYS_H_
#define MIDISYS_H_

#include "modmidi.h"
#include "sq.h"

#define Midi_GetSystem(env) (struct MidiSystem *)(env->system)

struct MidiSystem {
	struct SeqHeaderChunk *sqHeader;
	struct SeqMidiChunk *sqMidi;
	struct SeqSongChunk *sqSong;
	unsigned char masterVolume;
};

#endif // MIDISYS_H_

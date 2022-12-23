#include "modmidi.h"

#include "midisys.h"

#include <stddef.h>
#include <stdio.h>

int gTickInterval;
int gDebug = 1;
int gVerbose = 1;
unsigned short channelMasks[16] = {
	1 << 0,
	1 << 1,
	1 << 2,
	1 << 3,
	1 << 4,
	1 << 5,
	1 << 6,
	1 << 7,
	1 << 8,
	1 << 9,
	1 << 10,
	1 << 11,
	1 << 12,
	1 << 13,
	1 << 14,
	1 << 15,
};
struct MidiLoopInfo loopInfo = { 0 };

int
Midi_ATick(struct CslCtx *ctx)
{
	return 0;
}

static int
verifySeqChunk(unsigned int a, unsigned int b, const char *comp_a,
    const char *comp_b)
{
	char *pa = (char *)&a, *pb = (char *)&b;

	// clang-format off
	if (gDebug) {
	    printf("[%c%c%c%c] [%c%c%c%c]\n",
		   (a >> 24) & 0xff,
		   (a >> 16) & 0xff,
		   (a >> 8) & 0xff,
		   (a >> 0) & 0xff,

		   (b >> 24) & 0xff,
		   (b >> 16) & 0xff,
		   (b >> 8) & 0xff,
		   (b >> 0) & 0xff);
	}
	// clang-format on

	for (int i = 0; i < 4; i++) {
		if (pa[i] != comp_a[-i + 3]) {
			return 0;
		}
		if (pb[i] != comp_b[-i + 3]) {
			return 0;
		}
	}

	return 1;
}

int
Midi_Load(struct CslCtx *ctx, int port)
{
	struct SeqHeaderChunk *header;
	struct SeqVersionChunk *seq;
	struct SeqSongChunk *song;
	struct SeqMidiChunk *midi;
	struct MidiSystem *system;
	struct MidiEnv *env;

	if (!ctx) {
		return -1;
	}

	if (Midi_EnvPortIdx(port) >= ctx->buffGrp->buffNum) {
		// out of bounds port
		return -1;
	}

	env = Midi_GetEnv(ctx, port);

	if (!env) {
		return -1;
	}

	system = Midi_GetSystem(env);
	env->songNum = -1;
	env->midiNum = -1;
	env->position = 0;
	env->status = 0;
	system->sqHeader = NULL;
	system->sqMidi = NULL;
	system->sqSong = NULL;
	// FIXME unk here
	// FIXME unk loop

	system->masterVolume = 0x80;
	seq = ctx->buffGrp[0].buffCtx[Midi_SqPortIdx(port)].buff;

	if (!seq) {
		return -1;
	}

	if (!verifySeqChunk(seq->Creator, seq->Type, "SCEI", "Vers")) {
		return -1;
	}

	header = (struct SeqHeaderChunk *)((char *)seq + seq->chunkSize);

	if (!verifySeqChunk(header->Creator, header->Type, "SCEI", "Sequ")) {
		return -1;
	}

	song = (struct SeqSongChunk *)((char *)seq + header->seSongChunkAddr);

	if (header->seSongChunkAddr == -1 ||
	    !verifySeqChunk(header->Creator, header->Type, "SCEI", "Song")) {
		song = NULL;
	}

	midi = (struct SeqMidiChunk *)((char *)seq + header->midiChunkAddr);
	if (header->midiChunkAddr == -1 ||
	    !verifySeqChunk(midi->Creator, midi->Type, "SCEI", "Midi")) {
		return -1;
	}

	system->sqHeader = header;
	system->sqMidi = midi;
	system->sqSong = song;
	env->status = MidiStatus_Loaded;

	return 0;
}

int
Midi_Init(struct CslCtx *ctx, int interval)
{
	struct CslMidiStream *stream;
	struct MidiEnv *env;

	gTickInterval = interval << 7;
	if (!ctx) {
		return -1;
	}

	if (ctx->buffGrpNum < 2) {
		return -1;
	}

	if (!ctx->buffGrp) {
		return -1;
	}

	// Even number of input ports required
	if ((ctx->buffGrp[0].buffNum & 1) != 0) {
		return -1;
	}

	if (!ctx->buffGrp[0].buffCtx) {
		return -1;
	}

	if (ctx->buffGrp[0].buffNum > 1) {
		for (int i = 1, port = 0; i < ctx->buffGrp[0].buffNum;
		     i += 2, port++) {
			env = ctx->buffGrp[0].buffCtx[i].buff;

			if (env) {
				for (int j = 0; j < 16; j++) {
					env->outPort[j] = 1;
				}

				env->excOutPort = 1;
				env->chMsgCallBack = NULL;
				env->metaMsgCallBack = NULL;
				env->excMsgCallBack = NULL;
				env->repeatCallBack = NULL;
				env->markCallBack = NULL;
				Midi_Load(ctx, port);
			}
		}
	}

	if (ctx->buffGrp[1].buffNum && ctx->buffGrp[1].buffCtx) {
		stream = ctx->buffGrp[1].buffCtx->buff;
		for (int i = 0; i < ctx->buffGrp[1].buffNum; i++) {
			if (stream) {
				stream->validsize = 0;
			}
		}
	}

	return 0;
}

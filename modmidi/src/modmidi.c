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

static int
selectPort(struct CslCtx *ctx, int port, struct MidiEnv **envOut,
    struct MidiSystem **sysOut)
{
	if (!ctx) {
		return 0;
	}

	if (Midi_EnvPortIdx(port) >= ctx->buffGrp[MidiInBufGroup].buffNum) {
		return 0;
	}

	if (!envOut || !sysOut) {
		return 0;
	}

	*envOut = Midi_GetEnv(ctx, port);
	*sysOut = Midi_GetSystem(*envOut);

	return 1;
}

int
Midi_ATick(struct CslCtx *ctx)
{
	return 0;
}

static struct CslBuffCtx *
getOutPortCtx(unsigned int port, struct CslBuffGrp *bufGrp)
{
	if (port >= bufGrp->buffNum) {
		return NULL;
	}

	return &bufGrp->buffCtx[port];
}

static void
sendChMsg(struct MidiEnv *env, struct MidiSystem *system,
    struct CslBuffGrp *outGroup, unsigned int msg, unsigned int msgLen)
{
	struct CslBuffCtx *bufCtx;
	struct CslMidiStream *stream;
	unsigned int chan, portBits;
	unsigned char *out;
	msg &= 0x7F7FFF;

	if (env->chMsgCallBack) {
		msg = env->chMsgCallBack(msg, env->chMsgCallBackPrivateData);
		if (msg == 0) {
			return;
		}
	}

	chan = msg & 0xf;
	system->activeChan |= channelMasks[chan];

	if (!outGroup) {
		return;
	}

	portBits = env->outPort[chan];
	if (!portBits) {
		return;
	}

	for (int port = 0; portBits != 0; port++, portBits >>= 1) {
		if ((portBits & 1) == 0) {
			continue;
		}

		bufCtx = getOutPortCtx(port, outGroup);
		if (!bufCtx) {
			continue;
		}

		stream = (struct CslMidiStream *)bufCtx->buff;
		if (!stream) {
			continue;
		}

		if (stream->buffsize < stream->validsize + msgLen + 8) {
			if (gVerbose) {
				printf("sendChMsg Buffer OverRun\n");
			}

			continue;
		}

		out = &stream->data[stream->validsize];
		stream->validsize += msgLen;

		for (int j = 0; j < msgLen; j++) {
			*out++ = msg & 0xff;
			msg >>= 8;
		}
	}
}

static void
allNoteOff(struct MidiEnv *env, struct CslBuffGrp *out_group)
{
	struct MidiSystem *system = Midi_GetSystem(env);
	for (int i = 0; i < MidiNumMidiCh; i++) {
		if ((system->activeChan & channelMasks[i]) != 0) {
			// FIXME macro for constructing these?
			sendChMsg(env, system, out_group, i | 0x40B0, 3);
			sendChMsg(env, system, out_group, i | 0x7BB0, 3);
		}
	}
	system->activeChan = 0;
}

static unsigned int
getChanVol(struct MidiSystem *system, int ch)
{
	// clang-format off
	s32 ret = (system->unkPerChanVolume[ch] * system->chanelVolume[ch] *
		      system->masterVolume) >> 14;
	// clang-format on

	if (ret >= 0x80) {
		ret = 0x7f;
	}
	return ret;
}

static void
sendChVolume(struct MidiEnv *env, struct MidiSystem *system,
    struct CslBuffGrp *outGroup, int ch)
{

	unsigned int chanVol = getChanVol(system, ch);
	sendChMsg(env, system, outGroup, ch | (chanVol << 16) | 0x7B0, 3);
}

static void
channelSetup(struct MidiEnv *env, struct CslBuffGrp *outGroup)
{
	struct MidiSystem *system = Midi_GetSystem(env);
	struct channelParams *ch;
	for (int i = 0; i < MidiNumMidiCh; i++) {
		ch = &system->chParams[i];

		if ((ch->bank & 0x80) == 0) {
			sendChMsg(env, system, outGroup,
			    i | (ch->bank << 16) | 0xB0, 3);
		}
		if ((ch->program & 0x80) == 0) {
			sendChMsg(env, system, outGroup,
			    i | (ch->program << 8) | 0xC0, 2);
		}
		if ((ch->pitchModDepth & 0x80) == 0) {
			sendChMsg(env, system, outGroup,
			    i | (ch->pitchModDepth << 16) | 0x1B0, 3);
		}
		if ((ch->ampModDepth & 0x80) == 0) {
			sendChMsg(env, system, outGroup,
			    i | (ch->ampModDepth << 16) | 0x2B0, 3);
		}
		if ((ch->portamentTime & 0x80) == 0) {
			sendChMsg(env, system, outGroup,
			    i | (ch->portamentTime << 16) | 0x3B0, 3);
		}
		if ((ch->volume & 0x80) == 0) {
			system->unkPerChanVolume[i] = ch->volume;
			sendChVolume(env, system, outGroup, i);
		}
		if ((ch->pan & 0x80) == 0) {
			sendChMsg(env, system, outGroup,
			    i | (ch->pan << 16) | 0xAB0, 3);
		}
		if ((ch->expression & 0x80) == 0) {
			sendChMsg(env, system, outGroup,
			    i | (ch->expression << 16) | 0xBB0, 3);
		}
		if ((ch->damper & 0x80) == 0) {
			sendChMsg(env, system, outGroup,
			    i | (ch->damper << 16) | 0x40B0, 3);
		}
		if ((ch->portamentSwitch & 0x80) == 0) {
			sendChMsg(env, system, outGroup,
			    i | (ch->portamentSwitch << 16) | 0x41B0, 3);
		}
		if ((ch->pitchBend & 0x8080) == 0) {
			sendChMsg(env, system, outGroup,
			    i | (ch->pitchBend << 8) | 0xE0, 3);
		}
	}
}

int
Midi_MidiPlaySwitch(struct CslCtx *ctx, int port, int command)
{
	struct MidiEnv *env;
	if (!ctx) {
		return -1;
	}

	if (Midi_EnvPortIdx(port) >= ctx->buffGrp[MidiInBufGroup].buffNum) {
		// out of bounds port
		return -1;
	}

	env = Midi_GetEnv(ctx, port);
	if (!env) {
		return -1;
	}

	if (command == MidiPlay_Stop) {
		if ((env->status & MidiStatus_Playing) == 0) {
			return 0;
		}

		allNoteOff(env, &ctx->buffGrp[MidiOutBufGroup]);
		env->status &= ~MidiStatus_Playing;
	}

	if (command == MidiPlay_Start) {
		if ((env->status & (MidiStatus_Unk | MidiStatus_Loaded)) !=
		    MidiStatus_Loaded) {
			return -1;
		}

		if (env->midiNum == -1) {
			return -1;
		}

		channelSetup(env, &ctx->buffGrp[MidiOutBufGroup]);
		env->status |= MidiStatus_Playing;
	}

	return 0;
}

static void
updateTempo(struct MidiSystem *system)
{
	system->usecPerPPQN =
	    (((system->usecPerQuarter << 7) / system->Division) << 8) /
	    system->relativeTempo;
}

static int
readVLQ(struct MidiSystem *system)
{
	unsigned char *seqPos = system->seqPosition;
	s32 tmp, out = 0;

	do {
		tmp = *seqPos++;
		out = (out << 7) | (tmp & 0x7f);
	} while ((tmp & 0x80) != 0);

	system->seqPosition = seqPos;

	return out;
}

static void
readDelta(struct MidiSystem *system)
{
	s32 delta;
	if (system->unk) {
		system->unk = 0;
	} else {
		delta = readVLQ(system);
		system->tick += delta;
	}
}

static int
systemReset(struct MidiEnv *env)
{
	struct MidiSystem *system = Midi_GetSystem(env);

	env->position = 0;

	system->tick = 0;
	system->currentTick = 0;
	system->usecPerPPQN = 0;
	system->runningStatus = 0;
	system->unk = 0;
	system->markEntry.data = 0;
	system->markEntry.type = 0;
	system->markEntry.count = 0;
	system->markEntry.dataMode = 0;
	system->seqPosition = system->sequenceData;

	for (int i = 0; i < MidiNumMidiCh; i++) {
		system->unkPerChanVolume[i] = 0;
		system->chParams[i].program = -1;
		system->chParams[i].bank = -1;
		system->chParams[i].pitchModDepth = -1;
		system->chParams[i].ampModDepth = -1;
		system->chParams[i].portamentTime = -1;
		system->chParams[i].volume = -1;
		system->chParams[i].pan = -1;
		system->chParams[i].expression = -1;
		system->chParams[i].damper = -1;
		system->chParams[i].portamentSwitch = -1;
		system->chParams[i].pitchBend = -1;
	}

	updateTempo(system);
	readDelta(system);

	return 1;
}

static int
playEnv(struct MidiEnv *env, struct MidiSystem *system,
    struct CslBuffGrp *output)
{
	return 0;
}

int
Midi_MidiSetLocation(struct CslCtx *ctx, int port, unsigned int position)
{
	struct MidiSystem *system;
	struct MidiEnv *env;

	if (!selectPort(ctx, port, &env, &system)) {
		return -1;
	}

	// Reset to start if we need to go backwards
	if (position < env->position) {
		if (!systemReset(env)) {
			return -1;
		}
	}

	while (env->position < position) {
		if ((env->status & MidiStatus_Unk) != 0) {
			return -1;
		}

		if (system->tick >= position) {
			env->position = position;
		}

		env->position = system->tick;

		if (!playEnv(env, system, NULL)) {
			break;
		}
	}

	if (env->position != position) {
		return -1;
	}

	return 0;
}

int
Midi_MidiSetVolume(struct CslCtx *ctx, int port, unsigned int channel,
    char volume)
{
	struct MidiSystem *system;
	struct MidiEnv *env;

	if (!selectPort(ctx, port, &env, &system)) {
		return -1;
	}

	if (channel >= 16) {
		if (channel != 255) {
			return -1;
		}

		system->masterVolume = volume;
		if ((env->status & MidiStatus_Playing) != 0) {
			for (int i = 0; i < MidiNumMidiCh; i++) {
				sendChVolume(env, system,
				    &ctx->buffGrp[MidiOutBufGroup], i);
			}
		}

		return 0;
	}

	system->chanelVolume[channel] = volume;
	sendChVolume(env, system, &ctx->buffGrp[MidiOutBufGroup], channel);

	return 0;
}

int
Midi_SelectMidi(struct CslCtx *ctx, int port, int block)
{
	struct SeqMidiDataBlock *midiData;
	struct SeqMidiChunk *midi;
	struct MidiSystem *system;
	struct MidiEnv *env;

	if (!selectPort(ctx, port, &env, &system)) {
		return -1;
	}

	if ((env->status & MidiStatus_Playing) != 0) {
		Midi_MidiPlaySwitch(ctx, port, MidiPlay_Stop);
	}

	env->status &= ~(MidiStatus_Playing | MidiStatus_Unk);
	if ((env->status & MidiStatus_Loaded) == 0) {
		return -1;
	}

	midi = system->sqMidi;
	if (!midi) {
		return -1;
	}

	if (block > midi->maxMidiNumber) {
		return -1;
	}

	if (midi->midiOffsetAddr[block] == -1) {
		return -1;
	}

	midiData = (struct SeqMidiDataBlock *)((char *)midi +
	    midi->midiOffsetAddr[block]);

	if (midiData->sequenceDataOffset == -1) {
		return -1;
	}

	system->sqCompBlock = midiData->compBlock;
	if (midiData->compBlock[0].compOption != 1 ||
	    !midiData->compBlock[0].compTableSize ||
	    midiData->sequenceDataOffset < 0xC) {
		system->sqCompBlock = NULL;
	}

	for (int i = 0; i < 8; i++) {
		system->loopStack[i].loopID = -1;
		system->loopStack[i].loopCount = -1;
	}

	system->sequenceData = (unsigned char *)midiData +
	    midiData->sequenceDataOffset;
	system->usecPerQuarter = 500000;
	system->relativeTempo = 256;
	system->Division = midiData->Division;

	if (!systemReset(env)) {
		return -1;
	}

	env->midiNum = block;

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
	system->unk211 = 0;

	// TODO verify (dumb compiler strength reduction)
	for (int i = 0; i < MidiNumMidiCh; i++) {
		system->channelParams[i] = 0x80;
	}

	system->masterVolume = 0x80;
	seq = ctx->buffGrp[MidiInBufGroup].buffCtx[Midi_SqPortIdx(port)].buff;

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
	    !verifySeqChunk(song->Creator, song->Type, "SCEI", "Song")) {
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
	if ((ctx->buffGrp[MidiInBufGroup].buffNum & 1) != 0) {
		return -1;
	}

	if (!ctx->buffGrp[MidiInBufGroup].buffCtx) {
		return -1;
	}

	if (ctx->buffGrp[MidiInBufGroup].buffNum > 1) {
		for (int i = 1, port = 0;
		     i < ctx->buffGrp[MidiInBufGroup].buffNum; i += 2, port++) {
			env = ctx->buffGrp[MidiInBufGroup].buffCtx[i].buff;

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

	if (ctx->buffGrp[MidiOutBufGroup].buffNum &&
	    ctx->buffGrp[MidiOutBufGroup].buffCtx) {
		stream = ctx->buffGrp[MidiOutBufGroup].buffCtx->buff;
		for (int i = 0; i < ctx->buffGrp[MidiOutBufGroup].buffNum;
		     i++) {
			if (stream) {
				stream->validsize = 0;
			}
		}
	}

	return 0;
}

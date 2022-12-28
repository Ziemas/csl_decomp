#ifndef MODMIDI_H_
#define MODMIDI_H_

#include "csl.h"

#define Midi_EnvPortIdx(port) (2 * (port) + 1)
#define Midi_SqPortIdx(port) (2 * (port))
#define Midi_GetEnv(ctx, port) \
	(struct MidiEnv *)((ctx)->buffGrp[0].buffCtx[Midi_EnvPortIdx(port)].buff)

enum {
	MidiStatus_Loaded = 1,
	MidiStatus_Playing = 2,
	MidiStatus_Ended = 4,

	MidiNoError = 0,

	MidiNumMidiCh = 16,

	MidiPlay_Stop = 0,
	MidiPlay_Start = 1,

	MidiInBufGroup = 0,
	MidiOutBufGroup = 1,
};
struct MidiLoopInfo {
	unsigned char type;
	unsigned char loopTimes;
	unsigned char loopCount;
	unsigned int loopId;
};

#define MIDI_SYSTEM_MAX_SIZE (472)

struct MidiEnv {
	unsigned int songNum;
	unsigned int midiNum;
	unsigned int position;
	unsigned int status;
	unsigned short outPort[16];
	unsigned short excOutPort;
	unsigned int (*chMsgCallBack)(unsigned int, unsigned int);
	unsigned int chMsgCallBackPrivateData;
	int (*metaMsgCallBack)(unsigned char, unsigned char *, unsigned int,
		unsigned int);
	unsigned int metaMsgCallBackPrivateData;
	int (*excMsgCallBack)(unsigned char *, unsigned int, unsigned int);
	unsigned int excMsgCallBackPrivateData;
	int (*repeatCallBack)(struct MidiLoopInfo *, unsigned int);
	unsigned int repeatCallBackPrivateData;
	int (*markCallBack)(unsigned int, unsigned int, unsigned char *,
		unsigned int);
	unsigned int markCallBackPrivateData;
	unsigned char system[MIDI_SYSTEM_MAX_SIZE];
};

int Midi_Load(struct CslCtx *ctx, int port);
int Midi_Init(struct CslCtx *ctx, int interval);
int Midi_ATick(struct CslCtx *ctx);
int Midi_SelectMidi(struct CslCtx *ctx, int port, int midi_block);
int Midi_MidiSetLocation(struct CslCtx *ctx, int port, unsigned int position);
int Midi_MidiPlaySwitch(struct CslCtx *ctx, int port, int command);
int Midi_MidiSetVolume(struct CslCtx *ctx, int port, unsigned int channel,
	char volume);

#endif // MODMIDI_H_

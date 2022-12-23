#ifndef MODMIDI_H_
#define MODMIDI_H_

#define Midi_EnvPortIdx(port) (2 * (port) + 1)
#define Midi_SqPortIdx(port) (2 * (port))
#define Midi_GetEnv(ctx, port) \
	(struct MidiEnv        \
		*)((ctx)->buffGrp[0].buffCtx[Midi_EnvPortIdx(port)].buff)

enum {
	MidiStatus_Loaded = 1,

	MidiNoError = 0,

	MidiNumMidiCh = 15,
};

struct CslBuffCtx {
	int sema;
	void *buff;
};

struct CslBuffGrp {
	int buffNum;
	struct CslBuffCtx *buffCtx;
};

struct CslCtx {
	int buffGrpNum;
	struct CslBuffGrp *buffGrp;
	void *conf;
	void *callBack;
	char **extmod;
};

struct MidiLoopInfo {
	unsigned char type;
	unsigned char loopTimes;
	unsigned char loopCount;
	unsigned int loopId;
};

struct CslMidiStream {
	unsigned int buffsize;
	unsigned int validsize;
	unsigned char data[];
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

#endif // MODMIDI_H_

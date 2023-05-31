#include "csl.h"

#include "cslmidi.h"
#include "modhsyn/include/modhsyn.h"
#include "modmidi.h"
#include "sce_csl.h"
#include "sce_modhsyn.h"
#include "sce_modmidi.h"

#include <ioman.h>
#include <libsd.h>
#include <stdio.h>
#include <sysmem.h>
#include <thbase.h>
#include <thsemap.h>
#include <xtimrman.h>

static const int tickrate = 240;
static const int usec_per_tick = (1000 * 1000) / tickrate;
#define STREAMBUF_SIZE (1024)

struct tick_params {
	s32 semaphore;
	u32 count;
};

static struct tick_params tick_state;

static union {
	struct CslMidiStream stream;
	u8 buf[STREAMBUF_SIZE + sizeof(struct CslMidiStream)];
} stream_buf = { .stream.buffsize = STREAMBUF_SIZE + sizeof(struct CslMidiStream),
	.stream.validsize = 0 };

static struct MidiEnv midi_env = {};
static struct CslBuffCtx m_in_buf_ctx[2] = { { .buff = NULL }, { .buff = &midi_env } };
static struct CslBuffCtx m_out_buf_ctx[1] = { { .buff = stream_buf.buf } };
static struct CslBuffGrp midi_grp[2] = { { .buffNum = 2, .buffCtx = m_in_buf_ctx },
	{ .buffNum = 1, m_out_buf_ctx } };
static struct CslCtx midi_ctx = { .buffGrpNum = 2, .buffGrp = midi_grp };

// decompiled
static struct HSynEnv synth_env = {};
static struct CslBuffCtx s_in_buf_ctx[2] = { { .buff = stream_buf.buf }, { .buff = &synth_env } };
static struct CslBuffGrp synth_grp = { .buffNum = 2, .buffCtx = s_in_buf_ctx };
static struct CslCtx synth_ctx = { .buffGrpNum = 1, .buffGrp = &synth_grp };

static union {
	sceCslMidiStream stream;
	u8 buf[STREAMBUF_SIZE + sizeof(sceCslMidiStream)];
} sce_stream_buf = { .stream.buffsize = STREAMBUF_SIZE + sizeof(struct CslMidiStream),
	.stream.validsize = 0 };

// sce module
static sceMidiEnv scemidi_env = {};
static sceCslBuffCtx scem_in_buf_ctx[2] = { { .buff = NULL }, { .buff = &scemidi_env } };
static sceCslBuffCtx scem_out_buf_ctx[1] = { { .buff = sce_stream_buf.buf } };
static sceCslBuffGrp scemidi_grp[2] = { { .buffNum = 2, .buffCtx = scem_in_buf_ctx },
	{ .buffNum = 1, scem_out_buf_ctx } };
static sceCslCtx scemidi_ctx = { .buffGrpNum = 2, .buffGrp = scemidi_grp };

// sce module
static sceHSynEnv scesynth_env = {};
static sceCslBuffCtx sces_in_buf_ctx[2] = { { .buff = stream_buf.buf }, { .buff = &scesynth_env } };
static sceCslBuffGrp scesynth_grp = { .buffNum = 2, .buffCtx = sces_in_buf_ctx };
static sceCslCtx scesynth_ctx = { .buffGrpNum = 1, .buffGrp = &scesynth_grp };

static s32 *hd, *sq, *bd;

void
tick_thread(void *param)
{
	struct tick_params *p = param;

	while (1) {
		WaitSema(p->semaphore);
		Midi_ATick(&midi_ctx);
		HSyn_ATick(&synth_ctx);
	}
}

unsigned int
timer_tick(void *param)
{
	struct tick_params *p = param;

	iSignalSema(p->semaphore);
	return p->count;
}

int
init()
{
	iop_sys_clock_t clock = {};
	iop_thread_t thread = {};
	iop_sema_t sema = {};
	s32 thread_id;
	s32 timer_id;
	s32 ret;

	if (sceSdInit(0) < 0) {
		printf("SD INIT FAILED");
		return -1;
	}

	sceSdSetParam(SD_PARAM_MVOLL, 0x3fff);
	sceSdSetParam(SD_PARAM_MVOLR, 0x3fff);
	sceSdSetParam(SD_PARAM_MVOLL | 1, 0x3fff);
	sceSdSetParam(SD_PARAM_MVOLR | 1, 0x3fff);

	sema.attr = SA_THFIFO;
	sema.initial = 0;
	sema.max = 1;

	tick_state.semaphore = CreateSema(&sema);

	thread.attr = TH_C;
	thread.thread = &tick_thread;
	thread.priority = 50;
	thread.stacksize = 0x800;

	thread_id = CreateThread(&thread);
	if (thread_id > 0) {
		printf("starting tick thread\n");
		StartThread(thread_id, &tick_state);
	}

	USec2SysClock(usec_per_tick, &clock);
	tick_state.count = clock.lo;

#define TC_SYSCLOCK (1)
	timer_id = AllocHardTimer(TC_SYSCLOCK, 32, 1);
	SetTimerHandler(timer_id, tick_state.count, timer_tick, &tick_state);
#define TM_NO_GATE (0)
	SetupHardTimer(timer_id, TC_SYSCLOCK, TM_NO_GATE, 1);

	// ret = sceHSyn_Init(&scesynth_ctx, usec_per_tick);
	ret = HSyn_Init(&synth_ctx, usec_per_tick);
	if (ret != sceHSynNoError) {
		printf("hsyn init err %d\n", ret);
	}

	ret = Midi_Init(&midi_ctx, usec_per_tick);
	if (ret != sceMidiNoError) {
		printf("midi init err %d\n", ret);
	}

	StartHardTimer(timer_id);

	return 0;
}

u8 *
read_file(const char *filename)
{
	io_stat_t stat;
	u8 *buffer;
	s32 fd;

	getstat(filename, &stat);

	buffer = AllocSysMemory(ALLOC_FIRST, (int)stat.size, NULL);
	if (buffer == NULL) {
		printf("Alloc failed\n");
		return (u8 *)-1;
	}

	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		printf("Opening testfile failed\n");
		return (u8 *)-1;
	}

	read(fd, buffer, stat.size);
	close(fd);

	return buffer;
}

struct entry_header {
	char name[64];
	u32 header_byte_count;
	u32 entry_byte_count;
	u32 unk0;
	u32 unk1;
};

int
_start()
{
	struct entry_header *header;
	s32 fd, size, total = 0;
	u8 *buffer;
	s32 ret;
	buffer = AllocSysMemory(0, 1024 * 20, NULL);

	init();

	buffer = (u8 *)read_file("host:BG_116.snd");
	header = (struct entry_header *)buffer;

	printf("name %s\n", header->name);
	hd = (u8 *)header + header->header_byte_count;

	ret = HSyn_Load(&scesynth_ctx, 0, 0x6000, hd, 0);
	if (ret != sceHSynNoError) {
		printf("hsyn load err %d\n", ret);
	}

	header = (struct entry_header *)((u8 *)header + header->entry_byte_count +
		header->header_byte_count);
	printf("name %s\n", header->name);
	bd = (u8 *)header + header->header_byte_count;

	ret = HSyn_VoiceTrans(1, bd, 0x6000 + total, header->entry_byte_count);
	if (ret != sceHSynNoError) {
		printf("hsyn err %d\n", ret);
	}

	header = (struct entry_header *)((u8 *)header + header->entry_byte_count +
		header->header_byte_count);
	printf("name %s\n", header->name);
	sq = (s32 *)((u8 *)header + header->header_byte_count);

	m_in_buf_ctx[0].buff = sq;
	ret = Midi_Load(&midi_ctx, 0);
	if (ret != MidiNoError) {
		printf("midi load err %d\n", ret);
	}

	for (int ch = 0; ch < MidiNumMidiCh; ch++) {
		midi_env.outPort[ch] = 1 << 0;
	}

	ret = Midi_SelectMidi(&midi_ctx, 0, 0);
	if (ret != MidiNoError) {
		printf("midi 1 err %d\n", ret);
	}
	ret = Midi_MidiSetLocation(&midi_ctx, 0, 0);
	if (ret != MidiNoError) {
		printf("midi err %d\n", ret);
	}
	ret = Midi_MidiPlaySwitch(&midi_ctx, 0, sceMidi_MidiPlayStart);
	if (ret != MidiNoError) {
		printf("midi err %d\n", ret);
	}

	Midi_MidiSetVolume(&midi_ctx, 0, sceMidi_MidiSetVolume_MasterVol, sceMidi_Volume0db);
	HSyn_SetVolume(&scesynth_ctx, 0, sceHSyn_Volume_0db);

	// fd = open("host:my_stream_dump", O_CREAT | O_RDWR);
	// write(fd, stream_buf, sizeof(stream_buf));
	// close(fd);

	// -------------------------------------

	return 0;
}

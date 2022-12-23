#include "csl.h"

#include "cslmidi.h"
#include "modhsyn.h"
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

static sceCslCtx midi_ctx = {};
static sceCslBuffGrp midi_grp[2] = {};
static sceCslBuffCtx m_in_buf_ctx[2] = {}, m_out_buf_ctx[1] = {};

static u8 stream_buf[STREAMBUF_SIZE + sizeof(sceCslMidiStream)];
static sceMidiEnv midi_env = {};

static sceCslCtx synth_ctx = {};
static sceCslBuffGrp synth_grp = {};
static sceCslBuffCtx s_in_buf_ctx[2] = {};
static sceHSynEnv synth_env = {};

static s32 *hd, *sq, *bd;

void
tick_thread(void *param)
{
	struct tick_params *p = param;

	while (1) {
		WaitSema(p->semaphore);
		sceMidi_ATick(&midi_ctx);
		sceHSyn_ATick(&synth_ctx);
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

	midi_ctx.buffGrpNum = 2; // 2 for modmidi
	midi_ctx.buffGrp = midi_grp;

	midi_grp[0].buffNum = 1 * 2; // 1 input port
	midi_grp[0].buffCtx = m_in_buf_ctx;
	m_in_buf_ctx[0].buff = NULL;	  // SEQ data
	m_in_buf_ctx[1].buff = &midi_env; // midi env

	midi_grp[1].buffNum = 1; // 1 output port
	midi_grp[1].buffCtx = m_out_buf_ctx;
	m_out_buf_ctx[0].buff = stream_buf; // output stream for hsyn
	((sceCslMidiStream *)stream_buf)->buffsize = STREAMBUF_SIZE +
	    sizeof(sceCslMidiStream);
	((sceCslMidiStream *)stream_buf)->validsize = 0;

	synth_ctx.buffGrpNum = 1;
	synth_ctx.buffGrp = &synth_grp;
	synth_grp.buffNum = 1 * 2; // 1 input port
	synth_grp.buffCtx = s_in_buf_ctx;
	s_in_buf_ctx[0].buff = stream_buf; // input from midi
	s_in_buf_ctx[1].buff = &synth_env; // synth_env

	ret = sceHSyn_Init(&synth_ctx, usec_per_tick);
	if (ret != sceHSynNoError) {
		printf("hsyn init err %d\n", ret);
	}
	ret = sceMidi_Init(&midi_ctx, usec_per_tick);
	if (ret != sceMidiNoError) {
		printf("midi init err %d\n", ret);
	}

	StartHardTimer(timer_id);

	return 0;
}

s32 *
read_file(const char *filename)
{
	io_stat_t stat;
	getstat(filename, &stat);

	u8 *buffer = AllocSysMemory(ALLOC_FIRST, stat.size, NULL);
	if (buffer == NULL) {
		printf("Alloc failed\n");
		return -1;
	}

	u32 fd = open(filename, O_RDONLY);
	if (fd < 0) {
		printf("Opening testfile failed\n");
		return -1;
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
	s32 fd, size, total = 0;
	u8 *buffer;
	s32 ret;
	buffer = AllocSysMemory(0, 1024 * 20, NULL);

	init();

	buffer = (u8 *)read_file("host:BG_116.snd");
	struct entry_header *header = (struct entry_header *)buffer;

	printf("name %s\n", header->name);
	hd = (u8 *)header + header->header_byte_count;

	ret = sceHSyn_Load(&synth_ctx, 0, 0x6000, hd, 0);
	if (ret != sceHSynNoError) {
		printf("hsyn load err %d\n", ret);
	}

	header = (struct entry_header *)((u8 *)header +
	    header->entry_byte_count + header->header_byte_count);
	printf("name %s\n", header->name);
	bd = (u8 *)header + header->header_byte_count;

	ret = sceHSyn_VoiceTrans(1, bd, 0x6000 + total,
	    header->entry_byte_count);
	if (ret != sceHSynNoError) {
		printf("hsyn err %d\n", ret);
	}

	header = (struct entry_header *)((u8 *)header +
	    header->entry_byte_count + header->header_byte_count);
	printf("name %s\n", header->name);
	sq = (u8 *)header + header->header_byte_count;

	m_in_buf_ctx[0].buff = sq;
	ret = sceMidi_Load(&midi_ctx, 0);
	if (ret != sceMidiNoError) {
		printf("midi load err %d\n", ret);
	}

	for (int ch = 0; ch < sceMidiNumMidiCh; ch++) {
		midi_env.outPort[ch] = 1 << 0;
	}

	ret = sceMidi_SelectMidi(&midi_ctx, 0, 0);
	if (ret != sceMidiNoError) {
		printf("midi err %d\n", ret);
	}
	ret = sceMidi_MidiSetLocation(&midi_ctx, 0, 0);
	if (ret != sceMidiNoError) {
		printf("midi err %d\n", ret);
	}
	ret = sceMidi_MidiPlaySwitch(&midi_ctx, 0, sceMidi_MidiPlayStart);
	if (ret != sceMidiNoError) {
		printf("midi err %d\n", ret);
	}

	sceMidi_MidiSetVolume(&midi_ctx, 0, sceMidi_MidiSetVolume_MasterVol,
	    sceMidi_Volume0db);
	sceHSyn_SetVolume(&synth_ctx, 0, sceHSyn_Volume_0db);

	return 0;
}

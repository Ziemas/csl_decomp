#include "csl.h"
#include "cslmidi.h"
#include "modhsyn.h"
#include "modmidi.h"
#include <libsd.h>
#include <stdio.h>
#include <thbase.h>
#include <thsemap.h>
#include <xtimrman.h>

static const int tickrate = 240;
static const int usec_per_tick = (1000 * 1000) / tickrate;
static const int streambuf_size = 1024;

struct tick_params {
    s32 semaphore;
    u32 count;
};

static struct tick_params tick_state;

static sceCslCtx midi_ctx = {};
static sceCslBuffGrp midi_grp[2] = {};
static sceCslBuffCtx m_in_buf_ctx[2] = {}, m_out_buf_ctx[1] = {};

static u8 stream_buf[streambuf_size + sizeof(sceCslMidiStream)] = {};
static sceMidiEnv midi_env = {};

static sceCslCtx synth_ctx = {};
static sceCslBuffGrp synth_grp = {};
static sceCslBuffCtx s_in_buf_ctx[2] = {};
static sceHSynEnv synth_env = {};

void tick_thread(void* param)
{
    struct tick_params* p = param;

    while (1) {
        WaitSema(p->semaphore);
        sceMidi_ATick(&midi_ctx);
        sceHSyn_ATick(&synth_ctx);
    }
}

unsigned int timer_tick(void* param)
{
    struct tick_params* p = param;

    iSignalSema(p->semaphore);
    return p->count;
}

int init()
{
    iop_sys_clock_t clock = {};
    iop_thread_t thread = {};
    iop_sema_t sema = {};
    s32 thread_id;
    s32 timer_id;

    if (sceSdInit(0) < 0) {
        printf("SD INIT FAILED");
        return -1;
    }

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
    m_in_buf_ctx[0].buff = NULL; // SEQ data
    m_in_buf_ctx[1].buff = &midi_env; // midi env

    midi_grp[1].buffNum = 1; // 1 output port
    midi_grp[1].buffCtx = m_out_buf_ctx;
    m_out_buf_ctx[0].buff = stream_buf; // output stream for hsyn

    synth_ctx.buffGrpNum = 1;
    synth_ctx.buffGrp = &synth_grp;
    synth_grp.buffNum = 1 * 2; // 1 input port
    synth_grp.buffCtx = s_in_buf_ctx;
    s_in_buf_ctx[0].buff = stream_buf; // input from midi
    s_in_buf_ctx[1].buff = &synth_env; // synth_env

    sceHSyn_Init(&synth_ctx, usec_per_tick);
    sceMidi_Init(&midi_ctx, usec_per_tick);

    StartHardTimer(timer_id);

    return 0;
}

int _start()
{
    init();
    return 0;
}

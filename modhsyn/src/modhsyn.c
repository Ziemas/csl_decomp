#include "modhsyn.h"

#include "hsynsys.h"

struct HSynVoice gVoiceList[HSyn_NumCore][HSyn_NumVoice];

static int
selectPort(struct CslCtx *ctx, int port, struct HSynEnv **envOut,
	struct HSynSystem **sysOut)
{
	if (!ctx) {
		return 0;
	}

	if (HSyn_EnvPortIdx(port) >= ctx->buffGrp[0].buffNum) {
		return 0;
	}

	if (!envOut || !sysOut) {
		return 0;
	}

	*envOut = HSyn_GetEnv(ctx, port);
	*sysOut = HSyn_GetSystem(*envOut);

	return 1;
}

static void
resetVoiceState()
{
	for (int i = 0; i < HSyn_NumCore; i++) {
		for (int j = 0; j < HSyn_NumVoice; j++) {
			gVoiceList[i][j].coreId = i;
			gVoiceList[i][j].voiceId = j;
		}
	}
}

int
HSyn_Init(struct CslCtx *ctx, unsigned int interval)
{
	return 0;
}

int
HSyn_ATick(struct CslCtx *ctx)
{
	return 0;
}

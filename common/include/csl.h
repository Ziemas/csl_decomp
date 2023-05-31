#ifndef CSL_H_
#define CSL_H_

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

struct CslMidiStream {
	unsigned int buffsize;
	unsigned int validsize;
	unsigned char data[];
};

struct CslIdMonitor {
	int system[48];
};

#endif // CSL_H_

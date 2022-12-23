#ifndef SQ_H_
#define SQ_H_

struct SeqSongChunk {
	unsigned int Creator;
	unsigned int Type;
	unsigned int chunkSize;

	unsigned int maxSongNumber;
	unsigned int songOffsetAddr[0];
};

struct SeqMidiCompBlock {
	unsigned short compOption;
	unsigned short compTableSize;
	unsigned char compTable[0];
};

struct sceSeqMidiDataBlock {
	unsigned int sequenceDataOffset;
	unsigned short Division;
	struct SeqMidiCompBlock compBlock[0];
};

struct SeqMidiChunk {
	unsigned int Creator;
	unsigned int Type;
	unsigned int chunkSize;
	unsigned int maxMidiNumber;
	unsigned int midiOffsetAddr[0];
};

struct SeqHeaderChunk {
	unsigned int Creator;
	unsigned int Type;
	unsigned int chunkSize;
	unsigned int fileSize;
	unsigned int songChunkAddr;
	unsigned int midiChunkAddr;
	unsigned int seSequenceChunkAddr;
	unsigned int seSongChunkAddr;
};

struct SeqVersionChunk {
	unsigned int Creator;
	unsigned int Type;
	unsigned int chunkSize;
	unsigned short reserved;
	unsigned char versionMajor;
	unsigned char versionMinor;
};

#endif // SQ_H_

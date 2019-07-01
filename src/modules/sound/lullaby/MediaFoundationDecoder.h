// Media Foundation decoder

#ifndef LOVE_SOUND_LULLABY_MEDIAFOUNDATION_DECODER_H
#define LOVE_SOUND_LULLABY_MEDIAFOUNDATION_DECODER_H

// LOVE
#include "common/Data.h"
#include "sound/Decoder.h"

#if defined(LOVE_WINDOWS) && !defined(LOVE_NOMEDIAFOUNDATION)

namespace love
{
namespace sound
{
namespace lullaby
{

class MFDecoder: public Decoder
{
public:
	MFDecoder(Data *data, int bufferSize);
	virtual ~MFDecoder();

	static bool accepts(const std::string &ext);
	static void quit();
	Decoder *clone();
	int decode();
	bool seek(double s);
	bool rewind();
	bool isSeekable();
	int getChannelCount() const;
	int getBitDepth() const;
	double getDuration();

private:
	struct MFInitData;
	struct MFData;

	static bool initialize();
	static MFInitData *initData;

	// non-exposed struct to prevent cluttering LOVE
	// includes with Windows.h
	MFData *mfData;
	// channel count
	int channels;
	// temporary buffer
	std::vector<char> tempBuffer;
	// amount of temporary PCM buffer
	int tempBufferCount;
	// byte depth
	int byteDepth;
	// duration
	double duration;
};

}
}
}

#endif // defined(LOVE_WINDOWS) && !defined(LOVE_NOMEDIAFOUNDATION)
#endif // LOVE_SOUND_LULLABY_MEDIAFOUNDATION_DECODER_H

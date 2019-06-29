// MP3 decoder using minimp3 instead of Mpg123

#ifdef LOVE_NOMPG123

#ifndef LOVE_SOUND_LULLABY_MINIMP3_DECODER_H
#define LOVE_SOUND_LULLABY_MINIMP3_DECODER_H

// STL
#include <vector>

// LOVE
#include "common/Data.h"
#include "sound/Decoder.h"

#include "dr_mp3/dr_mp3.h"

namespace love
{
namespace sound
{
namespace lullaby
{

class MP3Decoder: public love::sound::Decoder
{
public:
	MP3Decoder(Data *data, int bufsize);
	virtual ~MP3Decoder();

	static bool accepts(const std::string &ext);
	love::sound::Decoder *clone();
	int decode();
	bool seek(double s);
	bool rewind();
	bool isSeekable();
	int getChannelCount() const;
	int getBitDepth() const;
	double getDuration();

private:
	drmp3 mp3;
	double duration;
	std::vector<drmp3_seek_point> seekTable;
};

}
}
}

#endif // LOVE_SOUND_LULLABY_MINIMP3_DECODER_H
#endif // LOVE_NOMPG123

// MP3 decoder using minimp3 instead of Mpg123

#include "common/config.h"

#ifdef LOVE_NOMPG123

#define DR_MP3_IMPLEMENTATION
#include "MP3Decoder.h"

namespace love
{
namespace sound
{
namespace lullaby
{

MP3Decoder::MP3Decoder(Data *data, int bufferSize)
: Decoder(data, bufferSize)
{
	if(drmp3_init_memory(&mp3, data->getData(), data->getSize(), nullptr) == 0)
		throw love::Exception("Could not read mp3 data.");

	sampleRate = mp3.sampleRate;

	// calculate duration
	drmp3_uint64 pcmCount, mp3FrameCount;
	if (!drmp3_get_mp3_and_pcm_frame_count(&mp3, &mp3FrameCount, &pcmCount))
	{
		drmp3_uninit(&mp3);
		throw love::Exception("Could not calculate duration.");
	}
	duration = ((double) pcmCount) / ((double) mp3.sampleRate);

	// create seek table
	drmp3_seek_point seekPointDummy = {0, 0, 0, 0};
	uint32_t mp3FrameInt = mp3FrameCount;
	seekTable.resize(mp3FrameCount, seekPointDummy);
	if (!drmp3_calculate_seek_points(&mp3, &mp3FrameInt, seekTable.data()))
	{
		drmp3_uninit(&mp3);
		throw love::Exception("Could not calculate seek table");
	}
	mp3FrameInt = mp3FrameInt > mp3FrameCount ? mp3FrameCount : mp3FrameInt;

	// bind seek table
	if (!drmp3_bind_seek_table(&mp3, mp3FrameInt, seekTable.data()))
	{
		drmp3_uninit(&mp3);
		throw love::Exception("Could not bind seek table");
	}
}

MP3Decoder::~MP3Decoder()
{
	drmp3_uninit(&mp3);
}

bool MP3Decoder::accepts(const std::string &ext)
{
	static const std::string supported[] =
	{
		"mp3", ""
	};

	for (int i = 0; !(supported[i].empty()); i++)
	{
		if (supported[i].compare(ext) == 0)
			return true;
	}

	return false;
}

love::sound::Decoder *MP3Decoder::clone()
{
	return new MP3Decoder(data, bufferSize);
}

int MP3Decoder::decode()
{
	// bufferSize is in char
	int maxRead = bufferSize / sizeof(int16_t) / mp3.channels;
	int read = (int) drmp3_read_pcm_frames_s16(&mp3, maxRead, (drmp3_int16 *) buffer);

	if (read < maxRead)
		eof = true;

	return read * sizeof(int16_t) * mp3.channels;
}

bool MP3Decoder::seek(double s)
{
	drmp3_uint64 targetSample = s * mp3.sampleRate;
	drmp3_bool32 success = drmp3_seek_to_pcm_frame(&mp3, targetSample);

	if (success)
		eof = false;

	return success;
}

bool MP3Decoder::rewind()
{
	return seek(0.0);
}

bool MP3Decoder::isSeekable()
{
	return true;
}

int MP3Decoder::getChannelCount() const
{
	return mp3.channels;
}

int MP3Decoder::getBitDepth() const
{
	return 16;
}

double MP3Decoder::getDuration()
{
	return duration;
}

}
}
}

#endif // LOVE_NOMPG123

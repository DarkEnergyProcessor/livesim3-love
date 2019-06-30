// Media Foundation decoder

#include "MediaFoundationDecoder.h"

#if defined(LOVE_WINDOWS) && !defined(LOVE_NOMEDIAFOUNDATION)

// Windows things
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <propvarutil.h>

// Base COM
#include <Objbase.h>
#include <Shlwapi.h>

// Media Foundation
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

namespace MFID
{

// 73647561-0000-0010-8000-00aa00389b71
const GUID MediaType_Audio = {0x73647561, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71}};
// 00000001-0000-0010-8000-00aa00389b71
const GUID AudioFormat_PCM = {0x00000001, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
// 48eba18e-f8c9-4687-bf11-0a74c9f96a8f
const GUID MT_MAJOR_TYPE = {0x48eba18e, 0xf8c9, 0x4687, {0xbf, 0x11, 0x0a, 0x74, 0xc9, 0xf9, 0x6a, 0x8f}};
// f7e34c9a-42e8-4714-b74b-cb29d72c35e5
const GUID MT_SUBTYPE = {0xf7e34c9a, 0x42e8, 0x4714, {0xb7, 0x4b, 0xcb, 0x29, 0xd7, 0x2c, 0x35, 0xe5}};
// 37e48bf5-645e-4c5b-89de-ada9e29b696a
const GUID MT_AUDIO_NUM_CHANNELS = {0x37e48bf5, 0x645e, 0x4c5b, {0x89, 0xde, 0xad, 0xa9, 0xe2, 0x9b, 0x69, 0x6a}};
// 5faeeae7-0290-4c31-9e8a-c534f68d9dba
const GUID MT_AUDIO_SAMPLES_PER_SECOND = {0x5faeeae7, 0x0290, 0x4c31, {0x9e, 0x8a, 0xc5, 0x34, 0xf6, 0x8d, 0x9d, 0xba}};
// f2deb57f-40fa-4764-aa33-ed4f2d1ff669
const GUID MT_AUDIO_BITS_PER_SAMPLE = {0xf2deb57f, 0x40fa, 0x4764, {0xaa, 0x33, 0xed, 0x4f, 0x2d, 0x1f, 0xf6, 0x69}};
// fc358289-3cb6-460c-a424-b6681260375a
const GUID BYTESTREAM_CONTENT_TYPE = {0xfc358289, 0x3cb6, 0x460c, {0xa4, 0x24, 0xb6, 0x68, 0x12, 0x60, 0x37, 0x5a}};
// 2cd2d921-c447-44a7-a13c-4adabfc247e3
const IID MFAttributes = {0x2cd2d921, 0xc447, 0x44a7, {0xa1, 0x3c, 0x4a, 0xda, 0xbf, 0xc2, 0x47, 0xe3}};

}

namespace
{

typedef decltype(SHCreateMemStream) CreateMemStreamFunc;
typedef decltype(MFCreateMFByteStreamOnStream) CreateByteStreamFunc;
typedef decltype(MFCreateMediaType) CreateMediaTypeFunc;
typedef decltype(MFCreateSourceReaderFromByteStream) CreateSourceReaderFunc;
typedef decltype(MFStartup) StartupFunc;
typedef decltype(MFShutdown) ShutdownFunc;

struct MFInitData
{
	bool success;
	HMODULE shlwapi, mfPlat, mfReadWrite;

	// shlwapi.dll
	CreateMemStreamFunc *createMemoryStream;
	// mfplat.dll
	CreateByteStreamFunc *createByteStream;
	CreateMediaTypeFunc *createMediaType;
	StartupFunc *startup;
	ShutdownFunc *shutdown;
	// mfreadwrite.dll
	CreateSourceReaderFunc *createSourceReader;
};

struct MFData
{
	IStream *memoryStream;
	IMFByteStream *mfByteStream;
	IMFSourceReader *mfSourceReader;
	IMFMediaType *mfPCMAudio;
};

}

namespace love
{
namespace sound
{
namespace lullaby
{

void *MFDecoder::initData = nullptr;

MFDecoder::MFDecoder(Data *data, int bufferSize)
: Decoder(data, bufferSize)
, tempBufferCount(0)
, duration(-2.0)
{
	if (initialize() == false)
		throw love::Exception("Media Foundation API cannot be initialized or not supported.");

	MFData mfData = {
		nullptr, nullptr, nullptr, nullptr
	};
	MFInitData &init = *((MFInitData *) MFDecoder::initData);
	IMFMediaType *partialType = nullptr;

	try
	{
		// Create memory stream
		mfData.memoryStream = init.createMemoryStream((BYTE *) data->getData(), data->getSize());
		if (mfData.memoryStream == nullptr)
			throw love::Exception("Cannot create IStream interface.");

		// Create IMFByteStream
		if (FAILED(init.createByteStream(mfData.memoryStream, &mfData.mfByteStream)))
			throw love::Exception("Cannot create IMFByteStream from IStream.");

		// Create SourceReader
		IMFAttributes *attrs = nullptr;
		if (SUCCEEDED(mfData.mfByteStream->QueryInterface(MFID::MFAttributes, (void**)& attrs)))
		{
			HRESULT hr = S_OK;
			// Okay unfortunately we don't have any extension information
			// so we try all possible combination of mimes.
			static const std::wstring supported[] =
			{
				L"audio/x-ms-wma", L"audio/mpeg", L"audio/mp4", L"audio/wav", L"audio/aac", L""
			};

			for (int i = 0; !(supported[i].empty()); i++)
			{
				attrs->SetString(MFID::BYTESTREAM_CONTENT_TYPE, supported[i].c_str());
				hr = init.createSourceReader(mfData.mfByteStream, nullptr, &mfData.mfSourceReader);

				if (SUCCEEDED(hr))
					break;
			}

			if (FAILED(hr))
			{
				if (hr == HRESULT_FROM_WIN32(ERROR_MORE_DATA))
					throw love::Exception("Cannot create MediaFoundation audio decoder.");
				else
					throw love::Exception("Cannot create IMFSourceReader: HRESULT 0x%08X", (int) hr);
			}
		}
		else
			throw love::Exception("Cannot create IMFSourceReader (no IMFAttributes in IMFByteStream).");

		// Only select first audio stream
		if (FAILED(mfData.mfSourceReader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE)))
			throw love::Exception("IMFSourceReader->SetStreamSelection failed (1).");
		if (FAILED(mfData.mfSourceReader->SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE)))
			throw love::Exception("IMFSourceReader->SetStreamSelection failed (2).");

		// Create temporary type
		if (FAILED(init.createMediaType(&partialType)))
			throw love::Exception("Cannot create media type.");
		if (FAILED(partialType->SetGUID(MFID::MT_MAJOR_TYPE, MFID::MediaType_Audio)))
			throw love::Exception("IMFMediaType->SetGUID failed (1).");
		if (FAILED(partialType->SetGUID(MFID::MT_SUBTYPE, MFID::AudioFormat_PCM)))
			throw love::Exception("IMFMediaType->SetGUID failed (2).");

		// Set source reader output to PCM
		if (FAILED(mfData.mfSourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, partialType)))
			throw love::Exception("Cannot set source reader output type.");
		if (FAILED(mfData.mfSourceReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &mfData.mfPCMAudio)))
			throw love::Exception("Cannot get source reader output object.");

		// Just to be make sure
		if (FAILED(mfData.mfSourceReader->SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE)))
			throw love::Exception("IMFSourceReader->SetStreamSelection failed (3).");

		partialType->Release();

		UINT32 dummy;
		// Get sample rate
		if (FAILED(mfData.mfPCMAudio->GetUINT32(MFID::MT_AUDIO_SAMPLES_PER_SECOND, &dummy)))
			throw love::Exception("Cannot get sample rate.");
		sampleRate = dummy;
		// Get channel count
		if (FAILED(mfData.mfPCMAudio->GetUINT32(MFID::MT_AUDIO_NUM_CHANNELS, &dummy)))
			throw love::Exception("Cannot get channel count.");
		channels = dummy;
		// Get byte depth;
		if (FAILED(mfData.mfPCMAudio->GetUINT32(MFID::MT_AUDIO_BITS_PER_SAMPLE, &dummy)))
			throw love::Exception("Cannot get bit depth.");
		byteDepth = dummy / 8;
	}
	catch (love::Exception&)
	{
		if (partialType) partialType->Release();
		if (mfData.mfPCMAudio) mfData.mfPCMAudio->Release();
		if (mfData.mfSourceReader) mfData.mfSourceReader->Release();
		if (mfData.mfByteStream) mfData.mfByteStream->Release();
		if (mfData.memoryStream) mfData.memoryStream->Release();
		throw;
	}

	MFData *newMFData = new MFData(mfData);
	this->mfData = newMFData;
	tempBuffer.resize(bufferSize);
}

MFDecoder::~MFDecoder()
{
	MFData *mfData = (MFData *) this->mfData;
	if (mfData->mfPCMAudio) mfData->mfPCMAudio->Release();
	if (mfData->mfSourceReader) mfData->mfSourceReader->Release();
	if (mfData->mfByteStream) mfData->mfByteStream->Release();
	if (mfData->memoryStream) mfData->memoryStream->Release();
	
	delete mfData;
}

bool MFDecoder::accepts(const std::string& ext)
{
	if (!initialize())
		return false;

	// https://docs.microsoft.com/en-us/windows/desktop/medfound/supported-media-formats-in-media-foundation
	static const std::string supported[] =
	{
		"wma", "mp3", "m4a", "wav", "aac", ""
	};

	for (int i = 0; !(supported[i].empty()); i++)
	{
		if (supported[i].compare(ext) == 0)
			return true;
	}

	return false;
}

int MFDecoder::decode()
{
	MFData &mfData = *((MFData *) this->mfData);
	// both maxRead and decoded is in PCM frames
	int pcmFrameSize = byteDepth * channels;
	int maxRead = bufferSize / pcmFrameSize;
	int decoded = 0;
	char *buffer = (char *) this->buffer;
	IMFSample *sample = nullptr;
	IMFMediaBuffer *mfBuffer = nullptr;
	HRESULT hr;

	if (tempBufferCount > 0)
	{
		// Assume we still have buffers here
		int copied = tempBufferCount > maxRead ? maxRead : tempBufferCount;
		memcpy(buffer, tempBuffer.data(), copied * pcmFrameSize);
		decoded += copied;
		tempBufferCount -= copied;

		if (tempBufferCount > 0)
		{
			// move memory
			int copiedBytes = copied * pcmFrameSize;
			char *bufData = tempBuffer.data();
			memmove(bufData, bufData + copiedBytes, tempBuffer.size() - copiedBytes);
		}
	}

	while (decoded < maxRead)
	{
		DWORD flags = 0, maxBufLen = 0;
		BYTE *rawBuf = nullptr;

		// Read sample
		hr = mfData.mfSourceReader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags, nullptr, &sample);

		if (FAILED(hr))
			break;
		if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
			eof = true;
		if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED)
			// Type change - not supported by WAVE file format. What?
			break;
		if (sample == nullptr)
			// No sample
			break;

		if (FAILED(sample->ConvertToContiguousBuffer(&mfBuffer)))
			break;
		if (FAILED(mfBuffer->Lock(&rawBuf, nullptr, &maxBufLen)))
			break;

		// Copy decoded data to buffer
		int frameCount = (int) maxBufLen / pcmFrameSize;
		int needed = maxRead - decoded;
		int copied = frameCount > needed ? needed : frameCount;
		memcpy(buffer + decoded * pcmFrameSize, rawBuf, copied * pcmFrameSize);
		decoded += copied;

		if (decoded >= maxRead)
		{
			// If not all of it is consumed, put it to temporary buffer.
			int remainBytes = (frameCount - copied) * pcmFrameSize;
			tempBufferCount = frameCount - copied;
			if (remainBytes > tempBuffer.size())
				// resize
				tempBuffer.resize(remainBytes);

			// Copy data
			memcpy(tempBuffer.data(), rawBuf + copied * pcmFrameSize, remainBytes);
		}

		mfBuffer->Unlock();
		mfBuffer->Release(); mfBuffer = nullptr;
		sample->Release(); sample = nullptr;
	}

	if (sample) sample->Release();
	if (mfBuffer) mfBuffer->Release();

	return decoded * pcmFrameSize;
}

Decoder* MFDecoder::clone()
{
	return new MFDecoder(data, bufferSize);
}

bool MFDecoder::seek(double s)
{
	MFData &mfData = *((MFData *) this->mfData);
	PROPVARIANT seekDest;
	InitPropVariantFromInt64(s * 1e7, &seekDest);

	bool result = SUCCEEDED(mfData.mfSourceReader->SetCurrentPosition(GUID_NULL, seekDest));
	if (result)
		eof = false;

	return result;
}

bool MFDecoder::rewind()
{
	return seek(0);
}

bool MFDecoder::isSeekable()
{
	return true;
}

int MFDecoder::getChannelCount() const
{
	return channels;
}

int MFDecoder::getBitDepth() const
{
	return byteDepth * 8;
}

double MFDecoder::getDuration()
{
	if (duration == -2.0)
	{
		MFData &mfData = *((MFData *) this->mfData);
		PROPVARIANT prop;

		if (SUCCEEDED(mfData.mfSourceReader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &prop)))
			duration = (double) prop.uhVal.QuadPart * 1e-7;
		else
			duration = -1.0;
	}

	return duration;
}

bool MFDecoder::initialize()
{
	static MFInitData init = {false};

	if (MFDecoder::initData != nullptr)
		return init.success;

	MFDecoder::initData = &init;
	init.success = false;

	if ((init.shlwapi = LoadLibraryA("Shlwapi.dll")) == nullptr)
		return false;

	// Load SHCreateMemStream
	init.createMemoryStream = (CreateMemStreamFunc *) GetProcAddress(init.shlwapi, "SHCreateMemStream");
	if (init.createMemoryStream == nullptr)
	{
		// https://docs.microsoft.com/en-us/windows/desktop/api/shlwapi/nf-shlwapi-shcreatememstream
		// Prior to Windows Vista, this function was not included in the public Shlwapi.h file, nor
		// was it exported by name from Shlwapi.dll. To use it on earlier systems, you must call it
		// directly from the Shlwapi.dll file as ordinal 12.
		init.createMemoryStream = (CreateMemStreamFunc *) GetProcAddress(init.shlwapi, MAKEINTRESOURCEA(12));
		if (init.createMemoryStream == nullptr)
		{
			// Ok failed
			FreeLibrary(init.shlwapi);
			return false;
		}
	}

	if ((init.mfPlat = LoadLibraryA("mfplat.dll")) == nullptr)
	{
		FreeLibrary(init.shlwapi);
		return false;
	}

	init.createByteStream = (CreateByteStreamFunc *) GetProcAddress(init.mfPlat, "MFCreateMFByteStreamOnStream");
	if (init.createByteStream == nullptr)
	{
		FreeLibrary(init.mfPlat);
		FreeLibrary(init.shlwapi);
		return false;
	}
	init.createMediaType = (CreateMediaTypeFunc *) GetProcAddress(init.mfPlat, "MFCreateMediaType");
	if (init.createMediaType == nullptr)
	{
		FreeLibrary(init.mfPlat);
		FreeLibrary(init.shlwapi);
		return false;
	}
	init.startup = (StartupFunc *) GetProcAddress(init.mfPlat, "MFStartup");
	if (init.startup == nullptr)
	{
		FreeLibrary(init.mfPlat);
		FreeLibrary(init.shlwapi);
		return false;
	}
	init.shutdown = (ShutdownFunc *) GetProcAddress(init.mfPlat, "MFShutdown");
	if (init.shutdown == nullptr)
	{
		FreeLibrary(init.mfPlat);
		FreeLibrary(init.shlwapi);
		return false;
	}

	if ((init.mfReadWrite = LoadLibraryA("mfreadwrite.dll")) == nullptr)
	{
		FreeLibrary(init.mfPlat);
		FreeLibrary(init.shlwapi);
		return false;
	}

	init.createSourceReader = (CreateSourceReaderFunc *) GetProcAddress(init.mfReadWrite, "MFCreateSourceReaderFromByteStream");
	if (init.createSourceReader == nullptr)
	{
		FreeLibrary(init.mfReadWrite);
		FreeLibrary(init.mfPlat);
		FreeLibrary(init.shlwapi);
		return false;
	}

	if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)))
	{
		FreeLibrary(init.mfReadWrite);
		FreeLibrary(init.mfPlat);
		FreeLibrary(init.shlwapi);
		return false;
	}

	if (FAILED(init.startup(MF_VERSION, MFSTARTUP_NOSOCKET)))
	{
		CoUninitialize();
		FreeLibrary(init.mfReadWrite);
		FreeLibrary(init.mfPlat);
		FreeLibrary(init.shlwapi);
		return false;
	}

	init.success = true;
	return true;
}

void MFDecoder::quit()
{
	if (MFDecoder::initData)
	{
		MFInitData &init = *(MFInitData *) MFDecoder::initData;
		init.shutdown();
		CoUninitialize();
		FreeLibrary(init.mfReadWrite); init.mfReadWrite = nullptr;
		FreeLibrary(init.mfPlat); init.mfPlat = nullptr;
		FreeLibrary(init.shlwapi); init.shlwapi = nullptr;
		init.success = false;
		MFDecoder::initData = nullptr;
	}
}

}
}
}

#endif // defined(LOVE_WINDOWS) && !defined(LOVE_NOMEDIAFOUNDATION)

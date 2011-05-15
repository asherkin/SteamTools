#define STEAMVOICE_INTERFACE "STEAMVOICE_INTERFACE_001"

class ISteamVoice
{
public:
	// Error codes for use with the voice functions
	enum EVoiceResult
	{
		k_EVoiceResultOK = 0,
		k_EVoiceResultNotInitialized = 1,
		k_EVoiceResultNotRecording = 2,
		k_EVoiceResultNoData = 3,
		k_EVoiceResultBufferTooSmall = 4,
		k_EVoiceResultDataCorrupted = 5,
		k_EVoiceResultRestricted = 6,
	};

public:
	// Decompresses a chunk of compressed data produced by GetVoice().
	// nBytesWritten is set to the number of bytes written to pDestBuffer unless the return value is k_EVoiceResultBufferTooSmall.
	// In that case, nBytesWritten is set to the size of the buffer required to decompress the given
	// data. The suggested buffer size for the destination buffer is 22 kilobytes.
	// The output format of the data is 16-bit signed at 11025 samples per second.
	virtual ISteamVoice::EVoiceResult DecompressVoice( const void *pCompressed, uint32 cbCompressed, void *pDestBuffer, uint32 cbDestBufferSize, uint32 *nBytesWritten ) = 0;
};
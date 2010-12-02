#ifndef tickets_h__
#define tickets_h__

#define NO_CSTEAMID_STL
#include "SteamTypes.h"

class Section_t
{
public:
	Section_t(
		uint32 length,
		unsigned char unknown[8], 
		CSteamID steamid, 
		__time32_t generation)
	{
		this->length = length;
		memcpy(this->unknown, unknown, 8);
		this->steamid = steamid;
		this->generation = generation;
	}

	//private:
	uint32 length;
	unsigned char unknown[8];
	CSteamID steamid;
	__time32_t generation;
};

class OwnershipTicket_t
{
public:
	OwnershipTicket_t(
		uint32 length, 
		uint32 version, 
		CSteamID steamid, 
		AppId_t appid, 
		uint32 externalip, 
		uint32 internalip, 
		uint32 ownershipflags, 
		__time32_t generation, 
		__time32_t expiration, 
		uint16 numlicenses, 
		uint32 licenses[], 
		uint32 filler)
	{
		this->length = length;
		this->version = version;
		this->steamid = steamid;
		this->appid = appid;
		this->externalip = externalip;
		this->internalip = internalip;
		this->ownershipflags = ownershipflags;
		this->generation = generation;
		this->expiration = expiration;
		this->numlicenses = numlicenses;
		this->licenses = licenses;
		this->filler = filler;
	}

	~OwnershipTicket_t()
	{
		delete this->licenses;
	}

	//private:
	uint32 length;
	uint32 version;
	CSteamID steamid;
	AppId_t appid;
	uint32 externalip;
	uint32 internalip;
	uint32 ownershipflags;
	__time32_t generation;
	__time32_t expiration;
	uint16 numlicenses;
	uint32 *licenses;
	uint32 filler;
};

class OwnershipSection_t
{
public:
	OwnershipSection_t(
		uint32 length, 
		OwnershipTicket_t *ticket, 
		unsigned char signature[128])
	{
		this->length = length;
		this->ticket = ticket;
		memcpy(this->signature, signature, 128);
	}

	~OwnershipSection_t()
	{
		delete ticket;
	}

	//private:
	uint32 length;
	OwnershipTicket_t *ticket;
	unsigned char signature[128];
};

class AuthBlob_t 
{
public:
	AuthBlob_t(const void *pvAuthBlob)
	{
		uint32 pos = 0;

		uint32 sectionlength = *(uint32 *)((char *)pvAuthBlob + pos);
		pos += sizeof(uint32);

		if (sectionlength != 20)
		{
			pos += sectionlength;
			section = NULL;
		} else {
			unsigned char unknown[8];
			memcpy(&unknown, ((char *)pvAuthBlob + pos), 8);
			pos += 8;
			CSteamID steamid = *(CSteamID *)((char *)pvAuthBlob + pos);
			pos += sizeof(CSteamID);
			__time32_t generation = *(__time32_t *)((char *)pvAuthBlob + pos);
			pos += sizeof(__time32_t);

			section = new Section_t(
				sectionlength,
				unknown,
				steamid,
				generation
				);
		}

		uint32 section2length = *(uint32 *)((char *)pvAuthBlob + pos);
		pos += sizeof(uint32);

		if (section2length == 0)
		{
			pos += section2length;
			ownership = NULL;
		} else {
			uint32 length = *(uint32 *)((char *)pvAuthBlob + pos);
			pos += sizeof(uint32);
			uint32 version = *(uint32 *)((char *)pvAuthBlob + pos);
			pos += sizeof(uint32);
			CSteamID steamid = *(CSteamID *)((char *)pvAuthBlob + pos);
			pos += sizeof(CSteamID);
			AppId_t appid = *(AppId_t *)((char *)pvAuthBlob + pos);
			pos += sizeof(AppId_t);
			uint32 externalip = *(uint32 *)((char *)pvAuthBlob + pos);
			pos += sizeof(uint32);
			uint32 internalip = *(uint32 *)((char *)pvAuthBlob + pos);
			pos += sizeof(uint32);
			uint32 ownershipflags = *(uint32 *)((char *)pvAuthBlob + pos);
			pos += sizeof(uint32);
			__time32_t generation = *(__time32_t *)((char *)pvAuthBlob + pos);
			pos += sizeof(__time32_t);
			__time32_t expiration = *(__time32_t *)((char *)pvAuthBlob + pos);
			pos += sizeof(__time32_t);
			uint16 numlicenses = *(uint16 *)((char *)pvAuthBlob + pos);
			pos += sizeof(uint16);

			uint32 *licenses = new uint32[numlicenses];
			for (int i = 0; i < numlicenses; i++)
			{
				licenses[i] = *(uint32 *)((char *)pvAuthBlob + pos);
				pos += sizeof(uint32);
			}

			uint32 filler = *(uint32 *)((char *)pvAuthBlob + pos);
			pos += sizeof(uint32);

			unsigned char signature[128];
			memcpy(&signature, ((char *)pvAuthBlob + pos), 128);
			pos += 128;

			ownership = new OwnershipSection_t(
				section2length,
				new OwnershipTicket_t(
				length,
				version,
				steamid,
				appid,
				externalip,
				internalip,
				ownershipflags,
				generation,
				expiration,
				numlicenses,
				licenses,
				filler
				),
				signature
				);
		}

		this->length = pos;

		this->blob = new unsigned char[pos];
		memcpy(this->blob, pvAuthBlob, pos);
	}

	~AuthBlob_t()
	{
		delete section;
		delete ownership;
		delete blob;
	}

	//private:
	uint32 length;
	Section_t *section;
	OwnershipSection_t *ownership;
	unsigned char *blob;
};

#endif // tickets_h__

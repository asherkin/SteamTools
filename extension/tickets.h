#ifndef tickets_h__
#define tickets_h__

#define NO_CSTEAMID_STL
#include "SteamTypes.h"

#include "blob.h"

class Section_t
{
public:
	Section_t(
		uint32 length,
		unsigned char unknown[8], 
		CSteamID steamid, 
		uint32 generation)
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
	uint32 generation;
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
		uint32 generation, 
		uint32 expiration, 
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

	/*
	~OwnershipTicket_t()
	{
		delete this->licenses;
	}
	*/

	//private:
	uint32 length;
	uint32 version;
	CSteamID steamid;
	AppId_t appid;
	uint32 externalip;
	uint32 internalip;
	uint32 ownershipflags;
	uint32 generation;
	uint32 expiration;
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

	/*
	~OwnershipSection_t()
	{
		delete ticket;
	}
	*/

	//private:
	uint32 length;
	OwnershipTicket_t *ticket;
	unsigned char signature[128];
};

#define AUTHBLOB_READ(type, value) \
	if (!authBlob.Read<type>(&value)) \
	{ \
		if (bError) \
			*bError = true; \
		return; \
	}

class AuthBlob_t 
{
public:
	AuthBlob_t(const void *pvAuthBlob, size_t cubAuthBlob, bool *bError = NULL)
	{
		CBlob authBlob(pvAuthBlob, cubAuthBlob);

		uint32 sectionlength;
		AUTHBLOB_READ(uint32, sectionlength);

		if (sectionlength != 20)
		{
			if ((authBlob.GetPosition() + sectionlength) > cubAuthBlob)
			{
				if (bError)
					*bError = true;
				return;
			}
			authBlob.AdvancePosition(sectionlength);
			section = NULL;
		} else {
			unsigned char unknown[8];
			if (!authBlob.Read(unknown, 8))
			{
				if (bError)
					*bError = true;
				return;
			}
			uint64 steamid;
			AUTHBLOB_READ(uint64, steamid);
			uint32 generation;
			AUTHBLOB_READ(uint32, generation);

			section = new Section_t(
				sectionlength,
				unknown,
				steamid,
				generation
				);
		}

		uint32 section2length;
		AUTHBLOB_READ(uint32, section2length);

		if (section2length == 0)
		{
			/*
			if ((authBlob.GetPosition() + section2length) > cubAuthBlob)
			{
				if (bError)
					*bError = true;
				return;
			}
			authBlob.AdvancePosition(section2length);
			*/
			ownership = NULL;
		} else {
			uint32 length;
			AUTHBLOB_READ(uint32, length);
			uint32 version;
			AUTHBLOB_READ(uint32, version);
			uint64 steamid;
			AUTHBLOB_READ(uint64, steamid);
			AppId_t appid;
			AUTHBLOB_READ(AppId_t, appid);
			uint32 externalip;
			AUTHBLOB_READ(uint32, externalip);
			uint32 internalip;
			AUTHBLOB_READ(uint32, internalip);
			uint32 ownershipflags;
			AUTHBLOB_READ(uint32, ownershipflags);
			uint32 generation;
			AUTHBLOB_READ(uint32, generation);
			uint32 expiration;
			AUTHBLOB_READ(uint32, expiration);
			uint16 numlicenses;
			AUTHBLOB_READ(uint16, numlicenses);

			uint32 *licenses = new uint32[numlicenses];
			for (int i = 0; i < numlicenses; i++)
			{
				AUTHBLOB_READ(uint32, licenses[i]);
			}

			uint32 filler;
			AUTHBLOB_READ(uint32, filler);

			unsigned char signature[128];
			if (!authBlob.Read(signature, 128))
			{
				if (bError)
					*bError = true;
				return;
			}

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

		this->length = authBlob.GetPosition();
	}

	/*
	~AuthBlob_t()
	{
		delete section;
		delete ownership;
	}
	*/

	//private:
	uint32 length;
	Section_t *section;
	OwnershipSection_t *ownership;
};

#endif // tickets_h__

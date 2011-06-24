#ifndef tickets_h__
#define tickets_h__

#define NO_CSTEAMID_STL
#include "SteamTypes.h"

#include "blob.h"

class GCTokenSection_t
{
public:
	GCTokenSection_t(
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

class SessionSection_t
{
public:
	SessionSection_t(
		uint32 length,
		uint32 unk1,
		uint32 unk2,
		uint32 externalip,
		uint32 filler,
		uint32 sometoken,
		uint32 unk4)
	{
		this->length = length;
		this->unk1 = unk1;
		this->unk2 = unk2;
		this->externalip = externalip;
		this->filler = filler;
		this->sometoken = sometoken;
		this->unk4 = unk4;

	}

	//private:
	uint32 length;
	uint32 unk1;
	uint32 unk2;
	uint32 externalip;
	uint32 filler;
	uint32 sometoken;
	uint32 unk4;
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
		uint16 numdlcs, 
		uint32 dlcs[], 
		uint16 numsubs, 
		uint32 subs[], 
		uint16 filler)
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
		this->numdlcs = numdlcs;
		this->dlcs = dlcs;
		this->numsubs = numsubs;
		this->subs = subs;
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
	uint16 numdlcs;
	uint32 *dlcs;
	uint16 numsubs;
	uint32 *subs;
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
			gcsection = NULL;
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

			gcsection = new GCTokenSection_t(
				sectionlength,
				unknown,
				steamid,
				generation
				);
		}

		uint32 sesionsectionlength;
		AUTHBLOB_READ(uint32, sesionsectionlength);

		if (sesionsectionlength != 24)
		{
			if ((authBlob.GetPosition() + sesionsectionlength) > cubAuthBlob)
			{
				if (bError)
					*bError = true;
				return;
			}
			authBlob.AdvancePosition(sesionsectionlength);
			session = NULL;
		} else {
			uint32 unk1;
			AUTHBLOB_READ(uint32, unk1);
			uint32 unk2;
			AUTHBLOB_READ(uint32, unk2);
			uint32 externalip;
			AUTHBLOB_READ(uint32, externalip);
			uint32 filler;
			AUTHBLOB_READ(uint32, filler);
			uint32 sometoken;
			AUTHBLOB_READ(uint32, sometoken);
			uint32 unk4;
			AUTHBLOB_READ(uint32, unk4);

			session = new SessionSection_t(
				sesionsectionlength,
				unk1,
				unk2,
				externalip,
				filler,
				sometoken,
				unk4
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

			uint16 numdlcs;
			AUTHBLOB_READ(uint16, numdlcs);
			uint32 *dlcs = new uint32[numdlcs];
			for (int i = 0; i < numdlcs; i++)
			{
				AUTHBLOB_READ(uint32, dlcs[i]);
			}

			uint16 numsubs;
			AUTHBLOB_READ(uint16, numsubs);
			uint32 *subs = new uint32[numsubs];
			for (int i = 0; i < numsubs; i++)
			{
				AUTHBLOB_READ(uint32, subs[i]);
			}

			uint16 filler;
			AUTHBLOB_READ(uint16, filler);

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
				numdlcs,
				dlcs,
				numsubs,
				subs,
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
	GCTokenSection_t *gcsection;
	SessionSection_t *session;
	OwnershipSection_t *ownership;
};

#endif // tickets_h__

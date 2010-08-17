/**
 * =============================================================================
 * SteamTools - Exposes some SteamClient functions to SourceMod plugins.
 * Copyright (C) 2010 Asher Baker (asherkin).  All rights reserved.
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, AlliedModders LLC gives you permission to link the
 * code of this program (as well as its derivative works) to "Half-Life 2," the
 * "Source Engine," the "SourcePawn JIT," and any Game MODs that run on software
 * by the Valve Corporation.  You must obey the GNU General Public License in
 * all respects for all other code used.  Additionally, AlliedModders LLC grants
 * this exception to all derivative works.  AlliedModders LLC defines further
 * exceptions, found in LICENSE.txt (as of this writing, version JULY-31-2007),
 * or <http://www.sourcemod.net/license.php>.
 * =============================================================================
 */

#ifndef _INCLUDE_SOURCEMOD_EXTENSION_PROPER_H_
#define _INCLUDE_SOURCEMOD_EXTENSION_PROPER_H_

/**
 * @file extension.h
 * @brief SteamTools extension code header.
 */

#include "smsdk_ext.h"

#define NO_CSTEAMID_STL
#define INTERFACEOSW_H
#include <Steamworks.h>

class CSteamClient
{
public:
	CSteamClient(int PlayerIndex, CSteamID SteamID)
	{
		this->PlayerIndex = PlayerIndex;
		this->SteamID = SteamID;
	}

	bool operator==(const CSteamClient &other) const {
		return this->SteamID == other.SteamID;
	}

	bool operator!=(const CSteamClient &other) const {
		return !(*this == other);
	}

	bool operator==(const int &other) const {
		return this->PlayerIndex == other;
	}

	bool operator!=(const int &other) const {
		return !(*this == other);
	}

	bool operator==(const CSteamID &other) const {
		return this->SteamID == other;
	}

	bool operator!=(const CSteamID &other) const {
		return !(*this == other);
	}

public:
	int GetIndex() const { return PlayerIndex; }
	CSteamID GetSteamID() const { return SteamID; }

protected:
	int PlayerIndex;
	CSteamID SteamID;
};

/**
 * @brief Sample implementation of the SDK Extension.
 * Note: Uncomment one of the pre-defined virtual functions in order to use it.
 */
class SteamTools : public SDKExtension, public IConCommandBaseAccessor, public IClientListener
{
public:
	/**
	 * @brief This is called after the initial loading sequence has been processed.
	 *
	 * @param error		Error message buffer.
	 * @param maxlength	Size of error message buffer.
	 * @param late		Whether or not the module was loaded after map load.
	 * @return			True to succeed loading, false to fail.
	 */
	virtual bool SDK_OnLoad(char *error, size_t maxlen, bool late);
	
	/**
	 * @brief This is called right before the extension is unloaded.
	 */
	virtual void SDK_OnUnload();

	/**
	 * @brief Called when the pause state is changed.
	 */
	//virtual void SDK_OnPauseChange(bool paused);

	/**
	 * @brief this is called when Core wants to know if your extension is working.
	 *
	 * @param error		Error message buffer.
	 * @param maxlength	Size of error message buffer.
	 * @return			True if working, false otherwise.
	 */
	virtual bool QueryRunning(char *error, size_t maxlen);
public:
#if defined SMEXT_CONF_METAMOD
	/**
	 * @brief Called when Metamod is attached, before the extension version is called.
	 *
	 * @param error			Error buffer.
	 * @param maxlength		Maximum size of error buffer.
	 * @param late			Whether or not Metamod considers this a late load.
	 * @return				True to succeed, false to fail.
	 */
	virtual bool SDK_OnMetamodLoad(ISmmAPI *ismm, char *error, size_t maxlen, bool late);

	/**
	 * @brief Called when Metamod is detaching, after the extension version is called.
	 * NOTE: By default this is blocked unless sent from SourceMod.
	 *
	 * @param error			Error buffer.
	 * @param maxlength		Maximum size of error buffer.
	 * @return				True to succeed, false to fail.
	 */
	//virtual bool SDK_OnMetamodUnload(char *error, size_t maxlen);

	/**
	 * @brief Called when Metamod's pause state is changing.
	 * NOTE: By default this is blocked unless sent from SourceMod.
	 *
	 * @param paused		Pause state being set.
	 * @param error			Error buffer.
	 * @param maxlength		Maximum size of error buffer.
	 * @return				True to succeed, false to fail.
	 */
	//virtual bool SDK_OnMetamodPauseChange(bool paused, char *error, size_t maxlen);
#endif

public: //IConCommandBaseAccessor
	bool RegisterConCommandBase(ConCommandBase *pCommand);

public: //IClientListener
	void OnClientAuthorized(int client, const char *authstring);
	void OnClientDisconnecting(int client);
};

void Hook_GameFrame(bool simulating);
bool Hook_WasRestartRequested();

static cell_t RequestGroupStatus(IPluginContext *pContext, const cell_t *params);
static cell_t RequestGameplayStats(IPluginContext *pContext, const cell_t *params);
static cell_t RequestServerReputation(IPluginContext *pContext, const cell_t *params);
static cell_t ForceHeartbeat(IPluginContext *pContext, const cell_t *params);
static cell_t IsVACEnabled(IPluginContext *pContext, const cell_t *params);
static cell_t IsConnected(IPluginContext *pContext, const cell_t *params);
static cell_t GetPublicIP(IPluginContext *pContext, const cell_t *params);

static cell_t SetKeyValue(IPluginContext *pContext, const cell_t *params);
static cell_t ClearAllKeyValues(IPluginContext *pContext, const cell_t *params);

static cell_t AddMasterServer(IPluginContext *pContext, const cell_t *params);
static cell_t RemoveMasterServer(IPluginContext *pContext, const cell_t *params);
static cell_t GetNumMasterServers(IPluginContext *pContext, const cell_t *params);
static cell_t GetMasterServerAddress(IPluginContext *pContext, const cell_t *params);

static cell_t RequestStats(IPluginContext *pContext, const cell_t *params);
static cell_t GetStatInt(IPluginContext *pContext, const cell_t *params);
static cell_t GetStatFloat(IPluginContext *pContext, const cell_t *params);
static cell_t IsAchieved(IPluginContext *pContext, const cell_t *params);

CSteamID SteamIDToCSteamID(const char *steamID);
bool CheckInterfaces();
bool LoadSteamclient(ISteamClient008 **pSteamClient, int method = 0);
#endif // _INCLUDE_SOURCEMOD_EXTENSION_PROPER_H_

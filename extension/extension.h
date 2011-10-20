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

class ISteamClient: public ISteamClient012 {};
#define STEAMCLIENT_INTERFACE_VERSION STEAMCLIENT_INTERFACE_VERSION_012

class ISteamGameServer: public ISteamGameServer011 {};
#define STEAMGAMESERVER_INTERFACE_VERSION STEAMGAMESERVER_INTERFACE_VERSION_011

class ISteamUtils: public ISteamUtils005 {};
#define STEAMUTILS_INTERFACE_VERSION STEAMUTILS_INTERFACE_VERSION_005

class ISteamGameServerStats: public ISteamGameServerStats001 {};
#define STEAMGAMESERVERSTATS_INTERFACE_VERSION STEAMGAMESERVERSTATS_INTERFACE_VERSION_001

class ISteamHTTP: public ISteamHTTP001 {};
#define STEAMHTTP_INTERFACE_VERSION STEAMHTTP_INTERFACE_VERSION_001

/**
 * @brief Sample implementation of the SDK Extension.
 * Note: Uncomment one of the pre-defined virtual functions in order to use it.
 */
class SteamTools: 
	public SDKExtension, 
	public IConCommandBaseAccessor, 
	public IPluginsListener
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

public: //IPluginsListener
	void OnPluginLoaded(IPlugin *plugin);
};

void Hook_Think(bool finalTick);
void Hook_GameServerSteamAPIActivated(void);

bool Hook_WasRestartRequested();

EBeginAuthSessionResult Hook_BeginAuthSession(const void *pAuthTicket, int cbAuthTicket, CSteamID steamID);
void Hook_EndAuthSession(CSteamID steamID);

bool CheckInterfaces();
bool LoadSteamclient(ISteamClient **pSteamClient, int method = 0);

CSteamID atocsteamid(const char *pRenderedID);

extern sp_nativeinfo_t g_ExtensionNatives[];

#endif // _INCLUDE_SOURCEMOD_EXTENSION_PROPER_H_

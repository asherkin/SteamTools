/*
 * =============================================================================
 * SteamTools - Exposes some SteamClient functions to SourceMod plugins.
 * Copyright (C) 2010 Asher Baker (Asherkin).  All rights reserved.
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
 */

#include "extension.h"

/**
 * @file extension.cpp
 * @brief SteamTools extension code.
 */

SteamTools g_SteamTools;
SMEXT_LINK(&g_SteamTools);

SH_DECL_HOOK1_void(IServerGameDLL, GameFrame, SH_NOATTRIB, 0, bool);
SH_DECL_HOOK0(ISteamMasterServerUpdater001, WasRestartRequested, SH_NOATTRIB, 0, bool);
SH_DECL_HOOK6_void(ISteamGameServer008, UpdateServerStatus,	SH_NOATTRIB, 0, int, int, int, const char *, const char *, const char *);

ConVar GroupSteamID("steamtools_version", SMEXT_CONF_VERSION, FCVAR_NOTIFY|FCVAR_REPLICATED, SMEXT_CONF_DESCRIPTION);

IServerGameDLL *g_pServerGameDLL = NULL;
ICvar *g_pLocalCVar = NULL;

ISteamGameServer008 *g_pSteamGameServer = NULL;
ISteamMasterServerUpdater001 *g_pSteamMasterServerUpdater = NULL;

typedef HSteamPipe (*GetPipeFn)();
typedef HSteamUser (*GetUserFn)();

GetPipeFn g_GameServerSteamPipe;
GetUserFn g_GameServerSteamUser;

int g_GameFrameHookID = 0;
int g_WasRestartRequestedHookID = 0;
int g_UpdateServerStatusHookID = 0;

IForward * g_pForwardGroupStatusResult = NULL;
IForward * g_pForwardRestartRequested = NULL;

bool g_ReportBotPlayers = true;

sp_nativeinfo_t g_ExtensionNatives[] =
{
	{ "Steam_RequestGroupStatus",	RequestGroupStatus },
	{ "Steam_ForceHeartbeat",		ForceHeartbeat },
	{ "Steam_IsVACEnabled",			IsVACEnabled },
	{ "Steam_ReportBotPlayers",		ReportBotPlayers },
	{ NULL,							NULL }
};

void Hook_GameFrame(bool simulating)
{
	if(g_pSteamGameServer)
	{
		CallbackMsg_t callbackMsg;
		HSteamCall steamCall;
		if (Steam_BGetCallback(g_GameServerSteamPipe(), &callbackMsg, &steamCall))
		{
			if (callbackMsg.m_iCallback == GSClientGroupStatus_t::k_iCallback)
			{
					GSClientGroupStatus_t *GroupStatus = (GSClientGroupStatus_t *)callbackMsg.m_pubParam;

					cell_t cellResults = 0;
					g_pForwardGroupStatusResult->PushString(GroupStatus->m_SteamIDUser.Render());
					g_pForwardGroupStatusResult->PushCell(GroupStatus->m_bMember);
					g_pForwardGroupStatusResult->Execute(&cellResults);

					Steam_FreeLastCallback(g_GameServerSteamPipe());
			}
		}
	} else {

#if defined _WIN32
		CreateInterfaceFn steamclient = (CreateInterfaceFn)GetProcAddress(GetModuleHandleA("steamclient.dll"), "CreateInterface");
		ISteamClient008 *client = (ISteamClient008 *)steamclient(STEAMCLIENT_INTERFACE_VERSION_008, NULL);

		HMODULE steamapi = GetModuleHandleA("steam_api.dll");

		g_GameServerSteamPipe = (GetPipeFn)GetProcAddress(steamapi, "SteamGameServer_GetHSteamPipe");
		g_GameServerSteamUser = (GetUserFn)GetProcAddress(steamapi, "SteamGameServer_GetHSteamUser");
#elif defined _LINUX
		void* steamclient_library = dlopen("steamclient_linux.so", RTLD_LAZY);
		if(steamclient_library == NULL)
		{
			// report error ...
			return;
		}

		CreateInterfaceFn steamclient = (CreateInterfaceFn)dlsym(steamclient_library, "CreateInterface");
		ISteamClient008 *client = (ISteamClient008 *)steamclient(STEAMCLIENT_INTERFACE_VERSION_008, NULL);

		dlclose(steamclient_library);

		void* steam_api_library = dlopen("libsteam_api_linux.so", RTLD_LAZY);
		if(steam_api_library == NULL)
		{
			// report error ...
			return;
		}

		g_GameServerSteamPipe = (GetPipeFn)dlsym(steam_api_library, "SteamGameServer_GetHSteamPipe");
		g_GameServerSteamUser = (GetUserFn)dlsym(steam_api_library, "SteamGameServer_GetHSteamUser");

		dlclose(steam_api_library);
#else
		// report error ...
		return;
#endif

		// let's not get impatient
		if(g_GameServerSteamPipe() == 0 || g_GameServerSteamUser() == 0)
			return;

		g_pSteamGameServer = (ISteamGameServer008 *)client->GetISteamGenericInterface(g_GameServerSteamUser(), g_GameServerSteamPipe(), STEAMGAMESERVER_INTERFACE_VERSION_008);
		g_pSteamMasterServerUpdater = (ISteamMasterServerUpdater001 *)client->GetISteamMasterServerUpdater(g_GameServerSteamUser(), g_GameServerSteamPipe(), STEAMMASTERSERVERUPDATER_INTERFACE_VERSION_001);
	}
}

bool SteamTools::SDK_OnLoad(char *error, size_t maxlen, bool late)
{
#if SOURCE_ENGINE != SE_ORANGEBOXVALVE
	g_pSM->Format(error, maxlen, "This extension is only supported on Team Fortress 2.");
	return false;
#endif

#if !defined _WIN32 && !defined _LINUX
	#error This extension is only supported on Windows and Linux.
	g_pSM->Format(error, maxlen, "This extension is only supported on Windows and Linux.");
	return false;
#endif

	if (strcmp(g_pSM->GetGameFolderName(), "tf") != 0)
	{
		g_pSM->Format(error, maxlen, "This extension is only supported on Team Fortress 2.");
		return false;
	}

	g_GameFrameHookID = SH_ADD_HOOK(IServerGameDLL, GameFrame, g_pServerGameDLL, SH_STATIC(Hook_GameFrame), true);
	g_WasRestartRequestedHookID = SH_ADD_HOOK(ISteamMasterServerUpdater001, WasRestartRequested, g_pSteamMasterServerUpdater, SH_STATIC(Hook_WasRestartRequested), true);
	g_UpdateServerStatusHookID = SH_ADD_HOOK(ISteamGameServer008, UpdateServerStatus, g_pSteamGameServer, SH_STATIC(Hook_UpdateServerStatus), false);

	g_pShareSys->AddNatives(myself, g_ExtensionNatives);
	g_pShareSys->RegisterLibrary(myself, "SteamTools");

	g_pForwardGroupStatusResult = g_pForwards->CreateForward("Steam_GroupStatusResult", ET_Ignore, 2, NULL, Param_String, Param_Cell);
	g_pForwardRestartRequested = g_pForwards->CreateForward("Steam_RestartRequested", ET_Ignore, 0, NULL);

	return true;
}

bool Hook_WasRestartRequested()
{
	bool WasRestartRequested;
	if (WasRestartRequested = g_pSteamMasterServerUpdater->WasRestartRequested())
	{
		cell_t cellResults = 0;
		g_pForwardRestartRequested->Execute(&cellResults);
	}
	RETURN_META_VALUE(MRES_SUPERCEDE, WasRestartRequested);
}

void Hook_UpdateServerStatus(int cPlayers, int cPlayersMax, int cBotPlayers, const char *pchServerName, const char *pSpectatorServerName, const char *pchMapName)
{
	if (g_ReportBotPlayers)
	{
		RETURN_META(MRES_IGNORED);
	} else {
		RETURN_META_NEWPARAMS(MRES_HANDLED, &ISteamGameServer008::UpdateServerStatus, (cPlayers, cPlayersMax, 0, pchServerName, pSpectatorServerName, pchMapName));
	}
}

bool SteamTools::SDK_OnMetamodLoad(ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	GET_V_IFACE_CURRENT(GetServerFactory, g_pServerGameDLL, IServerGameDLL, INTERFACEVERSION_SERVERGAMEDLL);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pLocalCVar, ICvar, CVAR_INTERFACE_VERSION);

	if (!g_pServerGameDLL)
	{
		snprintf(error, maxlen, "Could not find interface %s", INTERFACEVERSION_SERVERGAMEDLL);
		return false;
	}
	if (!g_pLocalCVar)
	{
		snprintf(error, maxlen, "Could not find interface %s", CVAR_INTERFACE_VERSION);
		return false;
	}

	g_pCVar = g_pLocalCVar;
	ConVar_Register(0, this);

	return true;
}

bool SteamTools::RegisterConCommandBase(ConCommandBase *pCommand)
{
		META_REGCVAR(pCommand);
		return true;
}

void SteamTools::SDK_OnUnload()
{
	if (g_GameFrameHookID != 0)
	{
		SH_REMOVE_HOOK_ID(g_GameFrameHookID);
		g_GameFrameHookID = 0;
	}
	if (g_WasRestartRequestedHookID != 0)
	{
		SH_REMOVE_HOOK_ID(g_WasRestartRequestedHookID);
		g_WasRestartRequestedHookID = 0;
	}
	if (g_UpdateServerStatusHookID != 0)
	{
		SH_REMOVE_HOOK_ID(g_UpdateServerStatusHookID);
		g_UpdateServerStatusHookID = 0;
	}

	g_pForwards->ReleaseForward(g_pForwardGroupStatusResult);
	g_pForwards->ReleaseForward(g_pForwardRestartRequested);
}

static cell_t RequestGroupStatus(IPluginContext *pContext, const cell_t *params)
{
	char * strUserID;
	pContext->LocalToString(params[1], &strUserID);

	g_pSteamGameServer->RequestUserGroupStatus(SteamIDToCSteamID(strUserID), CSteamID(params[2], k_EUniversePublic, k_EAccountTypeClan));

	return 0;
}

static cell_t ForceHeartbeat(IPluginContext *pContext, const cell_t *params)
{
	g_pSteamMasterServerUpdater->ForceHeartbeat();
	return 0;
}

static cell_t IsVACEnabled(IPluginContext *pContext, const cell_t *params)
{
	return g_pSteamGameServer->Secure();
}

static cell_t ReportBotPlayers(IPluginContext *pContext, const cell_t *params)
{
	g_ReportBotPlayers = params[1];
	return 0;
}

CSteamID SteamIDToCSteamID(const char *steamID)
{
	char *buf = new char[64];
	strcpy(buf, steamID);

	int iServer = 0;
	int iAuthID = 0;

	char *szTmp = strtok(buf, ":");
	while((szTmp = strtok(NULL, ":")))
	{
		char *szTmp2 = strtok(NULL, ":");
		if(szTmp2)
		{
			iServer = atoi(szTmp);
			iAuthID = atoi(szTmp2);
		}
	}

    uint64 i64friendID = (uint64)iAuthID * 2;
	uint64 constConvert = 76561197960265728ULL;
	i64friendID += constConvert + iServer;

	return CSteamID(i64friendID);
}

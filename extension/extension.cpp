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

/**
 * =============================================================================
 * Attributions & Thanks:
 * =============================================================================
 * AzuiSleet          - Wrote the original example code to acquire the SteamClient
 *                      factory.
 * VoiDeD & AzuiSleet - The OpenSteamworks project.
 * =============================================================================
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

IForward * g_pForwardGroupStatusResult = NULL;
IForward * g_pForwardRestartRequested = NULL;

sp_nativeinfo_t g_ExtensionNatives[] =
{
	{ "Steam_RequestGroupStatus",	RequestGroupStatus },
	{ "Steam_ForceHeartbeat",		ForceHeartbeat },
	{ "Steam_IsVACEnabled",			IsVACEnabled },
	{ "Steam_GetPublicIP",			GetPublicIP },
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

		CreateInterfaceFn steamclient = (CreateInterfaceFn)dlsym(steamclient_library, "CreateInterface");
		ISteamClient008 *client = (ISteamClient008 *)steamclient(STEAMCLIENT_INTERFACE_VERSION_008, NULL);

		dlclose(steamclient_library);

		void* steam_api_library = dlopen("libsteam_api_linux.so", RTLD_LAZY);

		g_GameServerSteamPipe = (GetPipeFn)dlsym(steam_api_library, "SteamGameServer_GetHSteamPipe");
		g_GameServerSteamUser = (GetUserFn)dlsym(steam_api_library, "SteamGameServer_GetHSteamUser");

		dlclose(steam_api_library);
#else
		// report error ...
		return;
#endif

		g_pSM->LogMessage(myself, "Steam library loading complete.");

		// let's not get impatient
		if(g_GameServerSteamPipe() == 0 || g_GameServerSteamUser() == 0)
			return;

		g_pSM->LogMessage(myself, "Acquiring interfaces and hooking functions...");

		g_pSteamGameServer = (ISteamGameServer008 *)client->GetISteamGenericInterface(g_GameServerSteamUser(), g_GameServerSteamPipe(), STEAMGAMESERVER_INTERFACE_VERSION_008);
		g_pSteamMasterServerUpdater = (ISteamMasterServerUpdater001 *)client->GetISteamMasterServerUpdater(g_GameServerSteamUser(), g_GameServerSteamPipe(), STEAMMASTERSERVERUPDATER_INTERFACE_VERSION_001);

		g_WasRestartRequestedHookID = SH_ADD_HOOK(ISteamMasterServerUpdater001, WasRestartRequested, g_pSteamMasterServerUpdater, SH_STATIC(Hook_WasRestartRequested), true);

		g_pSM->LogMessage(myself, "Loading complete.");

	}
}

bool SteamTools::SDK_OnLoad(char *error, size_t maxlen, bool late)
{

#if !defined _WIN32 && !defined _LINUX
#error This extension is only supported on Windows and Linux.
#endif

	if (strcmp(g_pSM->GetGameFolderName(), "tf") != 0)
	{
		g_pSM->LogMessage(myself, "Functionality is only guaranteed on Team Fortress 2, and may break features of other mods.");
	}

	g_GameFrameHookID = SH_ADD_HOOK(IServerGameDLL, GameFrame, g_pServerGameDLL, SH_STATIC(Hook_GameFrame), true);

	g_pShareSys->AddNatives(myself, g_ExtensionNatives);
	g_pShareSys->RegisterLibrary(myself, "SteamTools");

	g_pForwardGroupStatusResult = g_pForwards->CreateForward("Steam_GroupStatusResult", ET_Ignore, 2, NULL, Param_String, Param_Cell);
	g_pForwardRestartRequested = g_pForwards->CreateForward("Steam_RestartRequested", ET_Ignore, 0, NULL);

	g_pSM->LogMessage(myself, "Initial loading stage complete...");

	return true;
}

bool Hook_WasRestartRequested()
{
	bool bWasRestartRequested;
	if (bWasRestartRequested = SH_CALL(g_pSteamMasterServerUpdater, &ISteamMasterServerUpdater001::WasRestartRequested)())
	{
		cell_t cellResults = 0;
		g_pForwardRestartRequested->Execute(&cellResults);
	}
	RETURN_META_VALUE(MRES_SUPERCEDE, bWasRestartRequested);
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

	g_pForwards->ReleaseForward(g_pForwardGroupStatusResult);
	g_pForwards->ReleaseForward(g_pForwardRestartRequested);
}

static cell_t RequestGroupStatus(IPluginContext *pContext, const cell_t *params)
{
	char * strUserID;
	pContext->LocalToString(params[1], &strUserID);
	return g_pSteamGameServer->RequestUserGroupStatus(SteamIDToCSteamID(strUserID), CSteamID(params[2], k_EUniversePublic, k_EAccountTypeClan));;
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

static cell_t GetPublicIP(IPluginContext *pContext, const cell_t *params)
{
	uint32 ipAddress = g_pSteamGameServer->GetPublicIP();
	unsigned char octet[4]  = {0,0,0,0};

	for (int i=0; i<4; i++)
	{
		octet[i] = ( ipAddress >> (i*8) ) & 0xFF;
	}

	cell_t *addr;
	pContext->LocalToPhysAddr(params[1], &addr);

	addr[0] = octet[3];
	addr[1] = octet[2];
	addr[2] = octet[1];
	addr[3] = octet[0];

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
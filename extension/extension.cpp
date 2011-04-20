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
 *                      factory, information about GameServer auth tickets.
 * VoiDeD & AzuiSleet - The OpenSteamworks project.
 * Didrole            - Linux autoloading.
 * =============================================================================
 */

#if !defined _WIN32 && !defined _LINUX
#error This extension is only supported on Windows and Linux.
#endif

#if (SOURCE_ENGINE != SE_ORANGEBOXVALVE) && (SOURCE_ENGINE != SE_ALIENSWARM)
#error Unsupported SDK Version
#endif

#define NO_CSTEAMID_STL
#define INTERFACEOSW_H
#include <Steamworks.h>

class ISteamClient: public ISteamClient009 {};
#define STEAMCLIENT_INTERFACE_VERSION STEAMCLIENT_INTERFACE_VERSION_009

class ISteamMasterServerUpdater: public ISteamMasterServerUpdater001 {};
#define STEAMMASTERSERVERUPDATER_INTERFACE_VERSION STEAMMASTERSERVERUPDATER_INTERFACE_VERSION_001

class ISteamGameServer: public ISteamGameServer010 {};
#define STEAMGAMESERVER_INTERFACE_VERSION STEAMGAMESERVER_INTERFACE_VERSION_010

class ISteamUtils: public ISteamUtils005 {};
#define STEAMUTILS_INTERFACE_VERSION STEAMUTILS_INTERFACE_VERSION_005

class ISteamGameServerStats: public ISteamGameServerStats001 {};
#define STEAMGAMESERVERSTATS_INTERFACE_VERSION STEAMGAMESERVERSTATS_INTERFACE_VERSION_001

#include "extension.h"
#include "filesystem.h"
#include "tickets.h"
#include "utlmap.h"

/**
 * @file extension.cpp
 * @brief SteamTools extension code.
 */

SteamTools g_SteamTools;
SMEXT_LINK(&g_SteamTools);

SH_DECL_HOOK1_void(IServerGameDLL, Think, SH_NOATTRIB, 0, bool);
SH_DECL_HOOK0_void(IServerGameDLL, GameServerSteamAPIActivated, SH_NOATTRIB, 0);

SH_DECL_HOOK0(ISteamMasterServerUpdater, WasRestartRequested, SH_NOATTRIB, 0, bool);
SH_DECL_HOOK4(ISteamGameServer, SendUserConnectAndAuthenticate, SH_NOATTRIB, 0, bool, uint32, const void *, uint32, CSteamID *);
SH_DECL_HOOK1_void(ISteamGameServer, SendUserDisconnect, SH_NOATTRIB, 0, CSteamID);

ConVar SteamToolsVersion("steamtools_version", SMEXT_CONF_VERSION, FCVAR_NOTIFY|FCVAR_REPLICATED, SMEXT_CONF_DESCRIPTION);

IServerGameDLL *g_pServerGameDLL = NULL;
ICvar *g_pLocalCVar = NULL;
IFileSystem *g_pFileSystem = NULL;

ISteamGameServer *g_pSteamGameServer = NULL;
ISteamMasterServerUpdater *g_pSteamMasterServerUpdater = NULL;
ISteamUtils *g_pSteamUtils = NULL;
ISteamGameServerStats *g_pSteamGameServerStats = NULL;

CSteamID g_CustomSteamID = k_steamIDNil;
SteamAPICall_t g_SteamAPICall = k_uAPICallInvalid;
CUtlVector<SteamAPICall_t> g_RequestUserStatsSteamAPICalls;

typedef CUtlMap<uint32, CCopyableUtlVector<uint32> > SubIDMap;

bool SubIDLessFunc(const SubIDMap::KeyType_t &in1, const SubIDMap::KeyType_t &in2)
{
	return (in1 < in2);
};

SubIDMap g_subIDs(SubIDLessFunc);

typedef HSteamPipe (*GetPipeFn)();
typedef HSteamUser (*GetUserFn)();

typedef bool (*GetCallbackFn)(HSteamPipe hSteamPipe, CallbackMsg_t *pCallbackMsg);
typedef void (*FreeLastCallbackFn)(HSteamPipe hSteamPipe);

GetPipeFn g_GameServerSteamPipe;
GetUserFn g_GameServerSteamUser;

GetCallbackFn GetCallback;
FreeLastCallbackFn FreeLastCallback;

int g_ThinkHookID = 0;
int g_GameServerSteamAPIActivatedHookID = 0;

int g_WasRestartRequestedHookID = 0;
int g_SendUserConnectAndAuthenticateHookID = 0;
int g_SendUserDisconnectHookID = 0;

bool g_SteamServersConnected = false;
bool g_SteamLoadFailed = false;

IForward *g_pForwardGroupStatusResult = NULL;
IForward *g_pForwardGameplayStats = NULL;
IForward *g_pForwardReputation = NULL;
IForward *g_pForwardRestartRequested = NULL;

IForward *g_pForwardSteamServersConnected = NULL;
IForward *g_pForwardSteamServersDisconnected = NULL;

IForward *g_pForwardClientReceivedStats = NULL;
IForward *g_pForwardClientUnloadedStats = NULL;

IForward *g_pForwardLoaded = NULL;

int g_nPlayers = 0;

sp_nativeinfo_t g_ExtensionNatives[] =
{
	{ "Steam_RequestGroupStatus",			RequestGroupStatus },
	{ "Steam_RequestGameplayStats",			RequestGameplayStats },
	{ "Steam_RequestServerReputation",		RequestServerReputation },
	{ "Steam_ForceHeartbeat",				ForceHeartbeat },
	{ "Steam_IsVACEnabled",					IsVACEnabled },
	{ "Steam_IsConnected",					IsConnected },
	{ "Steam_GetPublicIP",					GetPublicIP },
	{ "Steam_SetRule",						SetKeyValue },
	{ "Steam_ClearRules",					ClearAllKeyValues },
	{ "Steam_AddMasterServer",				AddMasterServer },
	{ "Steam_RemoveMasterServer",			RemoveMasterServer },
	{ "Steam_GetNumMasterServers",			GetNumMasterServers },
	{ "Steam_GetMasterServerAddress",		GetMasterServerAddress },
	{ "Steam_RequestStats",					RequestStats },
	{ "Steam_GetStat",						GetStatInt },
	{ "Steam_GetStatFloat",					GetStatFloat },
	{ "Steam_IsAchieved",					IsAchieved },
	{ "Steam_GetNumClientSubscriptions",	GetNumClientSubscriptions },
	{ "Steam_GetClientSubscription",		GetClientSubscription },
	{ "Steam_GetCSteamIDForClient",			GetCSteamIDForClient },
	{ "Steam_GetCSteamIDFromRenderedID",	GetCSteamIDFromRenderedID },
	{ "Steam_SetCustomSteamID",				SetCustomSteamID },
	{ "Steam_GetCustomSteamID",				GetCustomSteamID },
	{ "Steam_GroupIDToCSteamID",			GroupIDToCSteamID },
	{ NULL,									NULL }
};

void Hook_GameServerSteamAPIActivated(void)
{
#if defined _WIN32	
	CSysModule *pModSteamApi = g_pFileSystem->LoadModule("../bin/steam_api.dll", "MOD", false);
#elif defined _LINUX
	CSysModule *pModSteamApi = g_pFileSystem->LoadModule("../bin/libsteam_api.so", "MOD", false);
#endif

	if ( !pModSteamApi )
	{
		g_pSM->LogError(myself, "Unable to get steam_api handle.");
		return;
	}

	HMODULE steam_api_library = reinterpret_cast<HMODULE>(pModSteamApi);

	g_GameServerSteamPipe = (GetPipeFn)GetProcAddress(steam_api_library, "SteamGameServer_GetHSteamPipe");
	g_GameServerSteamUser = (GetUserFn)GetProcAddress(steam_api_library, "SteamGameServer_GetHSteamUser");

	ISteamClient *client = NULL;

	if (!LoadSteamclient(&client))
		return;

	g_pSteamGameServer = (ISteamGameServer *)client->GetISteamGenericInterface(g_GameServerSteamUser(), g_GameServerSteamPipe(), STEAMGAMESERVER_INTERFACE_VERSION);
	g_pSteamMasterServerUpdater = (ISteamMasterServerUpdater *)client->GetISteamGenericInterface(g_GameServerSteamUser(), g_GameServerSteamPipe(), STEAMMASTERSERVERUPDATER_INTERFACE_VERSION);
	g_pSteamUtils = (ISteamUtils *)client->GetISteamGenericInterface(g_GameServerSteamUser(), g_GameServerSteamPipe(), STEAMUTILS_INTERFACE_VERSION);
	g_pSteamGameServerStats = (ISteamGameServerStats *)client->GetISteamGenericInterface(g_GameServerSteamUser(), g_GameServerSteamUser(), STEAMGAMESERVERSTATS_INTERFACE_VERSION);

	if (!CheckInterfaces())
		return;

	g_WasRestartRequestedHookID = SH_ADD_HOOK(ISteamMasterServerUpdater, WasRestartRequested, g_pSteamMasterServerUpdater, SH_STATIC(Hook_WasRestartRequested), false);
	g_SendUserConnectAndAuthenticateHookID = SH_ADD_HOOK(ISteamGameServer, SendUserConnectAndAuthenticate, g_pSteamGameServer, SH_STATIC(Hook_SendUserConnectAndAuthenticate), true);
	g_SendUserDisconnectHookID = SH_ADD_HOOK(ISteamGameServer, SendUserDisconnect, g_pSteamGameServer, SH_STATIC(Hook_SendUserDisconnect), true);

	g_SMAPI->ConPrintf("[STEAMTOOLS] Loading complete.\n");

	g_pForwardLoaded->Execute(NULL);

	g_SteamServersConnected = g_pSteamGameServer->LoggedOn();

	if (g_SteamServersConnected)
	{
		g_pForwardSteamServersConnected->Execute(NULL);
	} else {
		g_pForwardSteamServersDisconnected->Execute(NULL);
	}

	g_ThinkHookID = SH_ADD_HOOK(IServerGameDLL, Think, g_pServerGameDLL, SH_STATIC(Hook_Think), true);

	if (g_GameServerSteamAPIActivatedHookID != 0)
	{
		SH_REMOVE_HOOK_ID(g_GameServerSteamAPIActivatedHookID);
		g_GameServerSteamAPIActivatedHookID = 0;
	}
}

void Hook_Think(bool finalTick)
{
	CallbackMsg_t callbackMsg;
	if (GetCallback(g_GameServerSteamPipe(), &callbackMsg))
	{
		switch (callbackMsg.m_iCallback)
		{
		case GSClientGroupStatus_t::k_iCallback:
			{
				GSClientGroupStatus_t *GroupStatus = (GSClientGroupStatus_t *)callbackMsg.m_pubParam;

				int i;
				for (i = 1; i <= g_nPlayers; ++i)
				{
					IGamePlayer *player = playerhelpers->GetGamePlayer(i);
					if (!player)
						continue;

					edict_t *playerEdict = player->GetEdict();
					if (!playerEdict)
						continue;

					if (*engine->GetClientSteamID(playerEdict) == GroupStatus->m_SteamIDUser)
						break;
				}

				if (i > g_nPlayers)
				{
					i = -1;
					g_CustomSteamID = GroupStatus->m_SteamIDUser;
				}

				g_pForwardGroupStatusResult->PushCell(i);
				g_pForwardGroupStatusResult->PushCell(GroupStatus->m_SteamIDGroup.GetAccountID());
				g_pForwardGroupStatusResult->PushCell(GroupStatus->m_bMember);
				g_pForwardGroupStatusResult->PushCell(GroupStatus->m_bOfficer);
				g_pForwardGroupStatusResult->Execute(NULL);

				g_CustomSteamID = k_steamIDNil;

				FreeLastCallback(g_GameServerSteamPipe());
				break;
			}
		case GSGameplayStats_t::k_iCallback:
			{
				GSGameplayStats_t *GameplayStats = (GSGameplayStats_t *)callbackMsg.m_pubParam;

				if (GameplayStats->m_eResult == k_EResultOK)
				{
					g_pForwardGameplayStats->PushCell(GameplayStats->m_nRank);
					g_pForwardGameplayStats->PushCell(GameplayStats->m_unTotalConnects);
					g_pForwardGameplayStats->PushCell(GameplayStats->m_unTotalMinutesPlayed);
					g_pForwardGameplayStats->Execute(NULL);
				} else {
					g_pSM->LogError(myself, "Server Gameplay Stats received with an unexpected eResult. (eResult = %d)", GameplayStats->m_eResult);
				}
				FreeLastCallback(g_GameServerSteamPipe());
				break;
			}
		case SteamServersConnected_t::k_iCallback:
			{
				if (!g_SteamServersConnected)
				{
					g_pForwardSteamServersConnected->Execute(NULL);
					g_SteamServersConnected = true;
				}
				break;
			}
		case SteamServersDisconnected_t::k_iCallback:
			{
				if (g_SteamServersConnected)
				{
					g_pForwardSteamServersDisconnected->Execute(NULL);
					g_SteamServersConnected = false;
				}
				break;
			}
		case SteamAPICallCompleted_t::k_iCallback:
			{
				if (!g_pSteamUtils)
					return;

				SteamAPICallCompleted_t *APICallComplete = (SteamAPICallCompleted_t *)callbackMsg.m_pubParam;

				if (APICallComplete->m_hAsyncCall == g_SteamAPICall)
				{
					GSReputation_t Reputation;
					bool bFailed = false;
					g_pSteamUtils->GetAPICallResult(g_SteamAPICall, &Reputation, sizeof(Reputation), Reputation.k_iCallback, &bFailed);
					if (bFailed)
					{
						ESteamAPICallFailure failureReason = g_pSteamUtils->GetAPICallFailureReason(g_SteamAPICall);
						g_pSM->LogError(myself, "Server Reputation failed. (ESteamAPICallFailure = %d)", failureReason);
					} else {
						if (Reputation.m_eResult == k_EResultOK)
						{
							g_pForwardReputation->PushCell(Reputation.m_unReputationScore);
							g_pForwardReputation->PushCell(Reputation.m_bBanned);
							g_pForwardReputation->PushCell(Reputation.m_unBannedIP);
							g_pForwardReputation->PushCell(Reputation.m_usBannedPort);
							g_pForwardReputation->PushCell(Reputation.m_ulBannedGameID);
							g_pForwardReputation->PushCell(Reputation.m_unBanExpires);
							g_pForwardReputation->Execute(NULL);
						} else {
							g_pSM->LogError(myself, "Server Reputation received with an unexpected eResult. (eResult = %d)", Reputation.m_eResult);
						}
					}

					g_SteamAPICall = k_uAPICallInvalid;
					FreeLastCallback(g_GameServerSteamPipe());
				} else if (int elem = g_RequestUserStatsSteamAPICalls.Find(APICallComplete->m_hAsyncCall) != -1) {
					GSStatsReceived_t StatsReceived;
					bool bFailed = false;
					g_pSteamUtils->GetAPICallResult(APICallComplete->m_hAsyncCall, &StatsReceived, sizeof(StatsReceived), StatsReceived.k_iCallback, &bFailed);
					if (bFailed)
					{
						ESteamAPICallFailure failureReason = g_pSteamUtils->GetAPICallFailureReason(APICallComplete->m_hAsyncCall);
						g_pSM->LogError(myself, "Getting stats failed. (ESteamAPICallFailure = %d)", failureReason);
					} else {
						if (StatsReceived.m_eResult == k_EResultOK)
						{
							int i;
							for (i = 1; i <= g_nPlayers; ++i)
							{
								IGamePlayer *player = playerhelpers->GetGamePlayer(i);
								if (!player)
									continue;

								edict_t *playerEdict = player->GetEdict();
								if (!playerEdict)
									continue;

								if (*engine->GetClientSteamID(playerEdict) == StatsReceived.m_steamIDUser)
									break;
							}

							if (i > g_nPlayers)
							{
								i = -1;
								g_CustomSteamID = StatsReceived.m_steamIDUser;
							}

							g_pForwardClientReceivedStats->PushCell(i);
							g_pForwardClientReceivedStats->Execute(NULL);

							g_CustomSteamID = k_steamIDNil;

						} else if (StatsReceived.m_eResult == k_EResultFail) {
							g_pSM->LogError(myself, "Getting stats for user %s failed, backend reported that the user has no stats.", StatsReceived.m_steamIDUser.Render());
						} else {
							g_pSM->LogError(myself, "Stats for user %s received with an unexpected eResult. (eResult = %d)", StatsReceived.m_steamIDUser.Render(), StatsReceived.m_eResult);
						}
					}

					g_RequestUserStatsSteamAPICalls.Remove(elem);
					FreeLastCallback(g_GameServerSteamPipe());
				}
				break;
			}
		case GSStatsReceived_t::k_iCallback:
			{
				// The handler above dealt with this anyway, stop this getting to the engine.
				FreeLastCallback(g_GameServerSteamPipe());
				break;
			}
		case GSStatsUnloaded_t::k_iCallback:
			{
				GSStatsUnloaded_t *StatsUnloaded = (GSStatsUnloaded_t *)callbackMsg.m_pubParam;

				int i;
				for (i = 1; i <= g_nPlayers; ++i)
				{
					IGamePlayer *player = playerhelpers->GetGamePlayer(i);
					if (!player)
						continue;

					edict_t *playerEdict = player->GetEdict();
					if (!playerEdict)
						continue;

					if (*engine->GetClientSteamID(playerEdict) == StatsUnloaded->m_steamIDUser)
						break;
				}

				if (i > g_nPlayers)
				{
					i = -1;
					g_CustomSteamID = StatsUnloaded->m_steamIDUser;
				}

				g_pForwardClientUnloadedStats->PushCell(i);
				g_pForwardClientUnloadedStats->Execute(NULL);

				g_CustomSteamID = k_steamIDNil;

				FreeLastCallback(g_GameServerSteamPipe());
				break;
			}
		default:
			{
				//g_SMAPI->ConPrintf("Unhandled Callback: %d\n", callbackMsg.m_iCallback);
				break;
			}
		}
	}
}

bool CheckInterfaces() 
{
	g_SteamLoadFailed = false;
	
	if (!g_pSteamGameServer)
	{
		g_pSM->LogError(myself, "Could not find interface %s", STEAMGAMESERVER_INTERFACE_VERSION);
		g_SteamLoadFailed = true;
	}
	
	if (!g_pSteamMasterServerUpdater)
	{
		g_pSM->LogError(myself, "Could not find interface %s", STEAMMASTERSERVERUPDATER_INTERFACE_VERSION);
		g_SteamLoadFailed = true;
	}
	
	if (!g_pSteamUtils)
	{
		g_pSM->LogError(myself, "Could not find interface %s", STEAMUTILS_INTERFACE_VERSION);
		g_SteamLoadFailed = true;
	}
	
	if (!g_pSteamGameServerStats)
	{
		g_pSM->LogError(myself, "Could not find interface %s", STEAMGAMESERVERSTATS_INTERFACE_VERSION);
		g_SteamLoadFailed = true;
	}
		
	if (g_SteamLoadFailed)
	{
		if (g_ThinkHookID != 0)
		{
			SH_REMOVE_HOOK_ID(g_ThinkHookID);
			g_ThinkHookID = 0;
		}

		return false;
	} else {
		return true;
	}
}

bool LoadSteamclient(ISteamClient **pSteamClient, int method)
{
	if(!g_GameServerSteamPipe || !g_GameServerSteamUser || !g_GameServerSteamPipe() || !g_GameServerSteamUser())
		return false;

	HMODULE steamclient_library = NULL;
	ISteamClient *pLocalSteamClient = NULL;

	g_SMAPI->ConPrintf("[STEAMTOOLS] Trying method %d ...\n", (method + 1));

	switch(method)
	{
	case 0:
		{
#if defined _WIN32
			CSysModule *pModSteamClient = g_pFileSystem->LoadModule("../bin/steamclient.dll", "MOD", false);
#elif defined _LINUX
			CSysModule *pModSteamClient = g_pFileSystem->LoadModule("../bin/steamclient.so", "MOD", false);
#endif
			if (!pModSteamClient)
			{
				g_pSM->LogError(myself, "Unable to get steamclient handle.");
				break;
			}
			steamclient_library = reinterpret_cast<HMODULE>(pModSteamClient);
			break;
		}
#ifdef _WIN32
	case 1:
		{
			steamclient_library = GetModuleHandle("steamclient.dll");
			break;
		}
	case 2:
		{
			HKEY hRegKey;
			char pchSteamDir[MAX_PATH];
			if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\Valve\\Steam", 0, KEY_QUERY_VALUE, &hRegKey) != ERROR_SUCCESS)
			{
				g_pSM->LogError(myself, "Steam registry key not found.");
				break;
			}
			DWORD dwLength = sizeof(pchSteamDir);
			RegQueryValueExA(hRegKey, "InstallPath", NULL, NULL, (BYTE*)pchSteamDir, &dwLength);
			RegCloseKey(hRegKey);
			strcat(pchSteamDir, "/steamclient.dll");
			CSysModule *pModSteamClient = g_pFileSystem->LoadModule(pchSteamDir, "MOD", false);
			if (!pModSteamClient)
			{
				g_pSM->LogError(myself, "Unable to get steamclient handle.");
				break;
			}
			steamclient_library = reinterpret_cast<HMODULE>(pModSteamClient);
			break;
		}
#endif //_WIN32
	default:
		{
			g_pSM->LogError(myself, "Ran out of methods to acquire SteamWorks interfaces.");
			return false;
		}
	}

	if (!steamclient_library)
	{
		return LoadSteamclient(pSteamClient, (method + 1));
	}

	CreateInterfaceFn steamclient = (CreateInterfaceFn)GetProcAddress(steamclient_library, "CreateInterface");

	pLocalSteamClient = (ISteamClient *)steamclient(STEAMCLIENT_INTERFACE_VERSION, NULL);

	ISteamGameServer *gameserver = (ISteamGameServer *)pLocalSteamClient->GetISteamGenericInterface(g_GameServerSteamUser(), g_GameServerSteamPipe(), STEAMGAMESERVER_INTERFACE_VERSION);

	if (!gameserver)
	{
		return LoadSteamclient(pSteamClient, (method + 1));
	}

	g_SMAPI->ConPrintf("[STEAMTOOLS] Method %d worked!\n", (method + 1));

	*pSteamClient = pLocalSteamClient;

	GetCallback = (GetCallbackFn)GetProcAddress(steamclient_library, "Steam_BGetCallback");
	FreeLastCallback = (FreeLastCallbackFn)GetProcAddress(steamclient_library, "Steam_FreeLastCallback");

	return true;
}

bool SteamTools::SDK_OnLoad(char *error, size_t maxlen, bool late)
{
	g_GameServerSteamAPIActivatedHookID = SH_ADD_HOOK(IServerGameDLL, GameServerSteamAPIActivated, g_pServerGameDLL, SH_STATIC(Hook_GameServerSteamAPIActivated), true);

	g_pShareSys->AddNatives(myself, g_ExtensionNatives);
	g_pShareSys->RegisterLibrary(myself, "SteamTools");

	plsys->AddPluginsListener(this);

	g_pForwardGroupStatusResult = g_pForwards->CreateForward("Steam_GroupStatusResult", ET_Ignore, 4, NULL, Param_Cell, Param_Cell, Param_Cell, Param_Cell);
	g_pForwardGameplayStats = g_pForwards->CreateForward("Steam_GameplayStats", ET_Ignore, 3, NULL, Param_Cell, Param_Cell, Param_Cell);
	g_pForwardReputation = g_pForwards->CreateForward("Steam_Reputation", ET_Ignore, 6, NULL, Param_Cell, Param_Cell, Param_Cell, Param_Cell, Param_Cell, Param_Cell);
	g_pForwardRestartRequested = g_pForwards->CreateForward("Steam_RestartRequested", ET_Ignore, 0, NULL);

	g_pForwardSteamServersConnected = g_pForwards->CreateForward("Steam_SteamServersConnected", ET_Ignore, 0, NULL);
	g_pForwardSteamServersDisconnected = g_pForwards->CreateForward("Steam_SteamServersDisconnected", ET_Ignore, 0, NULL);

	g_pForwardClientReceivedStats = g_pForwards->CreateForward("Steam_StatsReceived", ET_Ignore, 1, NULL, Param_Cell);
	g_pForwardClientUnloadedStats = g_pForwards->CreateForward("Steam_StatsUnloaded", ET_Ignore, 1, NULL, Param_Cell);

	g_pForwardLoaded = g_pForwards->CreateForward("Steam_FullyLoaded", ET_Ignore, 0, NULL);

	g_SMAPI->ConPrintf("[STEAMTOOLS] Initial loading stage complete...\n");

	return true;
}

void Hook_SendUserDisconnect(CSteamID steamIDUser)
{
	g_subIDs.Remove(steamIDUser.GetAccountID());

	g_nPlayers--;

	RETURN_META(MRES_IGNORED);
}

void SteamTools::OnPluginLoaded(IPlugin *plugin)
{
	if (!g_pSteamGameServer)
		return;

	cell_t result;

	IPluginContext *pluginContext = plugin->GetRuntime()->GetDefaultContext();

	IPluginFunction *steamToolsLoadedCallback = pluginContext->GetFunctionByName("Steam_FullyLoaded");

	if (steamToolsLoadedCallback)
	{
		steamToolsLoadedCallback->CallFunction(NULL, 0, &result);
	} else {
		// This plugin doesn't use SteamTools
		return;
	}

	IPluginFunction *steamConnectionStateCallback = NULL;
	if (g_SteamServersConnected)
	{
		steamConnectionStateCallback = pluginContext->GetFunctionByName("Steam_SteamServersConnected");
	} else {
		steamConnectionStateCallback = pluginContext->GetFunctionByName("Steam_SteamServersDisconnected");
	}

	if (steamConnectionStateCallback)
	{
		steamConnectionStateCallback->CallFunction(NULL, 0, &result);
	}
}

bool Hook_WasRestartRequested()
{
	cell_t cellResults = 0;
	bool bWasRestartRequested = false;
	if ((bWasRestartRequested = SH_CALL(g_pSteamMasterServerUpdater, &ISteamMasterServerUpdater::WasRestartRequested)()))
	{
		g_pForwardRestartRequested->Execute(&cellResults);
	}
	RETURN_META_VALUE(MRES_SUPERCEDE, (cellResults < Pl_Handled)?bWasRestartRequested:false);
}

bool Hook_SendUserConnectAndAuthenticate(uint32 unIPClient, const void *pvAuthBlob, uint32 cubAuthBlobSize, CSteamID *pSteamIDUser)
{
	bool ret = META_RESULT_ORIG_RET(bool);

	bool error = false;
	AuthBlob_t authblob = AuthBlob_t(pvAuthBlob, cubAuthBlobSize, &error);

	if (error) // An error was encountered trying to parse the ticket.
	{
		CBlob authBlob(pvAuthBlob, cubAuthBlobSize);
		uint32 revVersion;
		if (authBlob.Read<uint32>(&revVersion) && revVersion == 83)
		{
			g_pSM->LogMessage(myself, "Client connecting from %u.%u.%u.%u sent a non-steam auth blob. (RevEmu ticket detected)", (unIPClient) & 0xFF, (unIPClient >> 8) & 0xFF, (unIPClient >> 16) & 0xFF, (unIPClient >> 24) & 0xFF);
		} else {
			g_pSM->LogMessage(myself, "Client connecting from %u.%u.%u.%u sent a non-steam auth blob.", (unIPClient) & 0xFF, (unIPClient >> 8) & 0xFF, (unIPClient >> 16) & 0xFF, (unIPClient >> 24) & 0xFF);
		}
		RETURN_META_VALUE(MRES_IGNORED, (bool)NULL);
	}

	if (!ret)
	{
		if (!authblob.ownership)
		{
			g_pSM->LogMessage(myself, "Client connecting from %u.%u.%u.%u (%s) isn't using Steam.", (unIPClient) & 0xFF, (unIPClient >> 8) & 0xFF, (unIPClient >> 16) & 0xFF, (unIPClient >> 24) & 0xFF, (authblob.section)?(authblob.section->steamid.Render()):("NO STEAMID"));
		} else if (!authblob.section) {
			g_pSM->LogMessage(myself, "Client connecting from %u.%u.%u.%u (%s) is in offline mode.", (unIPClient) & 0xFF, (unIPClient >> 8) & 0xFF, (unIPClient >> 16) & 0xFF, (unIPClient >> 24) & 0xFF, authblob.ownership->ticket->steamid.Render());
		} else {
			g_pSM->LogMessage(myself, "Client connecting from %u.%u.%u.%u (%s) was denied by Steam for an unknown reason. (Maybe an expired or stolen ticket?).", (unIPClient) & 0xFF, (unIPClient >> 8) & 0xFF, (unIPClient >> 16) & 0xFF, (unIPClient >> 24) & 0xFF, authblob.ownership->ticket->steamid.Render());
		}

		RETURN_META_VALUE(MRES_IGNORED, (bool)NULL);
	}

	if (!authblob.section && authblob.ownership)
	{
		g_pSM->LogMessage(myself, "Client connecting from %u.%u.%u.%u (%s) is in offline mode but their ticket hasn't expired yet.", (unIPClient) & 0xFF, (unIPClient >> 8) & 0xFF, (unIPClient >> 16) & 0xFF, (unIPClient >> 24) & 0xFF, authblob.ownership->ticket->steamid.Render());
		RETURN_META_VALUE(MRES_IGNORED, (bool)NULL);
	} else if (!authblob.section || !authblob.ownership) {
		g_pSM->LogError(myself, "SendUserConnectAndAuthenticate: Aborting due to missing sections in ticket. (authblob.length = %u)", authblob.length);
		RETURN_META_VALUE(MRES_IGNORED, (bool)NULL);
	}

	if (authblob.ownership->ticket->version != 4)
	{
		g_pSM->LogError(myself, "SendUserConnectAndAuthenticate: Aborting due to unexpected ticket version. (ticketVersion = %u)", authblob.ownership->ticket->version);
		RETURN_META_VALUE(MRES_IGNORED, (bool)NULL);
	}

	SubIDMap::IndexType_t index = g_subIDs.Insert(pSteamIDUser->GetAccountID());
	g_subIDs.Element(index).CopyArray(authblob.ownership->ticket->licenses, authblob.ownership->ticket->numlicenses);

	g_nPlayers++;

	RETURN_META_VALUE(MRES_IGNORED, (bool)NULL);
}

bool SteamTools::SDK_OnMetamodLoad(ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	GET_V_IFACE_CURRENT(GetServerFactory, g_pServerGameDLL, IServerGameDLL, INTERFACEVERSION_SERVERGAMEDLL);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pLocalCVar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetFileSystemFactory, g_pFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);

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
	if (!g_pFileSystem)
	{
		snprintf(error, maxlen, "Could not find interface %s", FILESYSTEM_INTERFACE_VERSION);
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
	if (g_ThinkHookID != 0)
	{
		SH_REMOVE_HOOK_ID(g_ThinkHookID);
		g_ThinkHookID = 0;
	}
	if (g_GameServerSteamAPIActivatedHookID != 0)
	{
		SH_REMOVE_HOOK_ID(g_GameServerSteamAPIActivatedHookID);
		g_GameServerSteamAPIActivatedHookID = 0;
	}
	if (g_WasRestartRequestedHookID != 0)
	{
		SH_REMOVE_HOOK_ID(g_WasRestartRequestedHookID);
		g_WasRestartRequestedHookID = 0;
	}
	if (g_SendUserConnectAndAuthenticateHookID != 0)
	{
		SH_REMOVE_HOOK_ID(g_SendUserConnectAndAuthenticateHookID);
		g_SendUserConnectAndAuthenticateHookID = 0;
	}
	if (g_SendUserDisconnectHookID != 0)
	{
		SH_REMOVE_HOOK_ID(g_SendUserDisconnectHookID);
		g_SendUserDisconnectHookID = 0;
	}

	g_pForwards->ReleaseForward(g_pForwardGroupStatusResult);
	g_pForwards->ReleaseForward(g_pForwardGameplayStats);
	g_pForwards->ReleaseForward(g_pForwardReputation);

	g_pForwards->ReleaseForward(g_pForwardRestartRequested);

	g_pForwards->ReleaseForward(g_pForwardSteamServersConnected);
	g_pForwards->ReleaseForward(g_pForwardSteamServersDisconnected);

	plsys->RemovePluginsListener(this);
}

bool SteamTools::QueryRunning( char *error, size_t maxlen )
{
	if (g_SteamLoadFailed)
	{
		snprintf(error, maxlen, "One or more SteamWorks interfaces failed to be acquired.");
		return false;
	}
	return true;
}

CON_COMMAND(steamid, "") {
	if (args.ArgC() != 2) {
		META_CONPRINTF("Usage: %s <steamid>\n", args.Arg(0));
		return;
	}

	CSteamID steamID = atocsteamid(args.Arg(1));

	if (steamID.IsValid())
		META_CONPRINTF("%s --> %llu (%lu)\n", args.Arg(1), steamID.ConvertToUint64(), steamID.GetAccountID());
	else
		META_CONPRINTF("%s is not a valid SteamID\n", args.Arg(1));
}

CSteamID atocsteamid(const char *pRenderedID) {
	char renderedID[32];
	V_strcpy(renderedID, pRenderedID);

	char *strPrefix = strtok(renderedID, ":");
	char *strParity = strtok(NULL, ":");
	char *strHalfAccount = strtok(NULL, ":");

	if (strPrefix == NULL || strParity == NULL || strHalfAccount == NULL)
		return k_steamIDNil;

	if (V_strlen(strPrefix) > 6 || V_strncmp(strPrefix, "STEAM_", 6) != 0)
		return k_steamIDNil;

	int parity = atoi(strParity);
	if (parity != 0 && parity != 1)
		return k_steamIDNil;

	long halfAccount = atol(strHalfAccount);
	if (halfAccount < 0 || halfAccount == LONG_MAX)
		return k_steamIDNil;

	uint32 account = ((halfAccount << 1) | parity);

	return CSteamID(account, k_EUniversePublic, k_EAccountTypeIndividual);
}

static cell_t RequestGroupStatus(IPluginContext *pContext, const cell_t *params)
{
	if (!g_pSteamGameServer)
		return 0;

	const CSteamID *pSteamID;
	if(params[1] > -1)
	{
		pSteamID = engine->GetClientSteamID(engine->PEntityOfEntIndex(params[1]));
	} else {
		if (g_CustomSteamID.IsValid())
			pSteamID = &g_CustomSteamID;
		else
			return pContext->ThrowNativeError("Custom SteamID not set.");
	}
	if (!pSteamID)
		return pContext->ThrowNativeError("No SteamID found for client %d", params[1]);

	return g_pSteamGameServer->RequestUserGroupStatus(*pSteamID, CSteamID(params[2], k_EUniversePublic, k_EAccountTypeClan));
}

static cell_t RequestGameplayStats(IPluginContext *pContext, const cell_t *params)
{
	if (!g_pSteamGameServer)
		return 0;

	g_pSteamGameServer->GetGameplayStats();
	return 0;
}

static cell_t RequestServerReputation(IPluginContext *pContext, const cell_t *params)
{
	if (!g_pSteamGameServer)
		return 0;

	if (g_SteamAPICall == k_uAPICallInvalid)
	{
		g_SteamAPICall = g_pSteamGameServer->GetServerReputation();
		return true;
	} else {
		return false;
	}
}

static cell_t ForceHeartbeat(IPluginContext *pContext, const cell_t *params)
{
	if (!g_pSteamMasterServerUpdater)
		return 0;

	g_pSteamMasterServerUpdater->ForceHeartbeat();
	return 0;
}

static cell_t IsVACEnabled(IPluginContext *pContext, const cell_t *params)
{
	if (!g_pSteamGameServer)
		return 0;

	return g_pSteamGameServer->Secure();
}

static cell_t IsConnected(IPluginContext *pContext, const cell_t *params)
{
	/*
	if (!g_pSteamGameServer)
		return 0;

	return g_pSteamGameServer->LoggedOn();
	*/

	return g_SteamServersConnected;
}

static cell_t GetPublicIP(IPluginContext *pContext, const cell_t *params)
{
	if (!g_pSteamGameServer)
		return 0;

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

static cell_t SetKeyValue(IPluginContext *pContext, const cell_t *params)
{
	if (!g_pSteamMasterServerUpdater)
		return 0;

	char *pKey;
	pContext->LocalToString(params[1], &pKey);
	char *pValue;
	pContext->LocalToString(params[2], &pValue);
	g_pSteamMasterServerUpdater->SetKeyValue(pKey, pValue);
	return 0;
}

static cell_t ClearAllKeyValues(IPluginContext *pContext, const cell_t *params)
{
	if (!g_pSteamMasterServerUpdater)
		return 0;

	g_pSteamMasterServerUpdater->ClearAllKeyValues();
	return 0;
}

static cell_t AddMasterServer(IPluginContext *pContext, const cell_t *params)
{
	if (!g_pSteamMasterServerUpdater)
		return 0;

	char *pServerAddress;
	pContext->LocalToString(params[1], &pServerAddress);
	return g_pSteamMasterServerUpdater->AddMasterServer(pServerAddress);
}

static cell_t RemoveMasterServer(IPluginContext *pContext, const cell_t *params)
{
	if (!g_pSteamMasterServerUpdater)
		return 0;

	char *pServerAddress;
	pContext->LocalToString(params[1], &pServerAddress);
	return g_pSteamMasterServerUpdater->RemoveMasterServer(pServerAddress);
}

static cell_t GetNumMasterServers(IPluginContext *pContext, const cell_t *params)
{
	if (!g_pSteamMasterServerUpdater)
		return 0;

	return g_pSteamMasterServerUpdater->GetNumMasterServers();
}

static cell_t GetMasterServerAddress(IPluginContext *pContext, const cell_t *params)
{
	if (!g_pSteamMasterServerUpdater)
		return 0;

	char *serverAddress = new char[params[3]];
	int numbytes = g_pSteamMasterServerUpdater->GetMasterServerAddress(params[1], serverAddress, params[3]);
	pContext->StringToLocal(params[2], numbytes, serverAddress);
	return numbytes;
}

static cell_t RequestStats(IPluginContext *pContext, const cell_t *params)
{
	if (!g_pSteamGameServerStats)
		return 0;

	const CSteamID *pSteamID;
	if(params[1] > -1)
	{
		pSteamID = engine->GetClientSteamID(engine->PEntityOfEntIndex(params[1]));
	} else {
		if (g_CustomSteamID.IsValid())
			pSteamID = &g_CustomSteamID;
		else
			return pContext->ThrowNativeError("Custom SteamID not set.");
	}
	if (!pSteamID)
		return pContext->ThrowNativeError("No SteamID found for client %d", params[1]);

	g_RequestUserStatsSteamAPICalls.AddToTail(g_pSteamGameServerStats->RequestUserStats(*pSteamID));
	return 0;
}

static cell_t GetStatInt(IPluginContext *pContext, const cell_t *params)
{
	if (!g_pSteamGameServerStats)
		return 0;

	const CSteamID *pSteamID;
	if(params[1] > -1)
	{
		pSteamID = engine->GetClientSteamID(engine->PEntityOfEntIndex(params[1]));
	} else {
		if (g_CustomSteamID.IsValid())
			pSteamID = &g_CustomSteamID;
		else
			return pContext->ThrowNativeError("Custom SteamID not set.");
	}
	if (!pSteamID)
		return pContext->ThrowNativeError("No SteamID found for client %d", params[1]);

	char *strStatName;
	pContext->LocalToString(params[2], &strStatName);

	int32 data;
	if (g_pSteamGameServerStats->GetUserStat(*pSteamID, strStatName, &data))
	{
		return data;
	} else {
		return pContext->ThrowNativeError("Failed to get stat %s for client %d", strStatName, params[1]);
	}
}

static cell_t GetStatFloat(IPluginContext *pContext, const cell_t *params)
{
	if (!g_pSteamGameServerStats)
		return 0;

	const CSteamID *pSteamID;
	if(params[1] > -1)
	{
		pSteamID = engine->GetClientSteamID(engine->PEntityOfEntIndex(params[1]));
	} else {
		if (g_CustomSteamID.IsValid())
			pSteamID = &g_CustomSteamID;
		else
			return pContext->ThrowNativeError("Custom SteamID not set.");
	}
	if (!pSteamID)
		return pContext->ThrowNativeError("No SteamID found for client %d", params[1]);

	char *strStatName;
	pContext->LocalToString(params[2], &strStatName);

	float data;
	if (g_pSteamGameServerStats->GetUserStat(*pSteamID, strStatName, &data))
	{
		return sp_ftoc(data);
	} else {
		return pContext->ThrowNativeError("Failed to get stat %s for client %d", strStatName, params[1]);
	}
}

static cell_t IsAchieved(IPluginContext *pContext, const cell_t *params)
{
	if (!g_pSteamGameServerStats)
		return 0;

	const CSteamID *pSteamID;
	if(params[1] > -1)
	{
		pSteamID = engine->GetClientSteamID(engine->PEntityOfEntIndex(params[1]));
	} else {
		if (g_CustomSteamID.IsValid())
			pSteamID = &g_CustomSteamID;
		else
			return pContext->ThrowNativeError("Custom SteamID not set.");
	}
	if (!pSteamID)
		return pContext->ThrowNativeError("No SteamID found for client %d", params[1]);

	char *strAchName;
	pContext->LocalToString(params[2], &strAchName);

	bool bAchieved;
	if (g_pSteamGameServerStats->GetUserAchievement(*pSteamID, strAchName, &bAchieved))
	{
		return bAchieved;
	} else {
		return pContext->ThrowNativeError("Failed to get achievement %s for client %d", strAchName, params[1]);
	}
}

static cell_t GetNumClientSubscriptions(IPluginContext *pContext, const cell_t *params)
{
	const CSteamID *pSteamID;
	if(params[1] > -1)
	{
		pSteamID = engine->GetClientSteamID(engine->PEntityOfEntIndex(params[1]));
	} else {
		return pContext->ThrowNativeError("Custom SteamID can not be used for this function", params[1]);
	}
	if (!pSteamID)
		return pContext->ThrowNativeError("No SteamID found for client %d", params[1]);

	SubIDMap::IndexType_t index = g_subIDs.Find(pSteamID->GetAccountID());
	if (!g_subIDs.IsValidIndex(index))
		return pContext->ThrowNativeError("No subscriptions were found for client %d", params[1]);

	return g_subIDs.Element(index).Count();
}

static cell_t GetClientSubscription(IPluginContext *pContext, const cell_t *params)
{
	const CSteamID *pSteamID;
	if(params[1] > -1)
	{
		pSteamID = engine->GetClientSteamID(engine->PEntityOfEntIndex(params[1]));
	} else {
		return pContext->ThrowNativeError("Custom SteamID can not be used for this function", params[1]);
	}
	if (!pSteamID)
		return pContext->ThrowNativeError("No SteamID found for client %d", params[1]);

	SubIDMap::IndexType_t index = g_subIDs.Find(pSteamID->GetAccountID());
	if (!g_subIDs.IsValidIndex(index))
		return pContext->ThrowNativeError("No subscriptions were found for client %d", params[1]);

	if(!g_subIDs.Element(index).IsValidIndex(params[2]))
		return pContext->ThrowNativeError("Subscription index %u is out of bounds for client %d", index, params[1]);

	return g_subIDs.Element(index).Element(params[2]);
}

static cell_t GetCSteamIDForClient(IPluginContext *pContext, const cell_t *params)
{
	const CSteamID *pSteamID;
	if(params[1] > -1)
	{
		pSteamID = engine->GetClientSteamID(engine->PEntityOfEntIndex(params[1]));
	} else {
		if (g_CustomSteamID.IsValid())
			pSteamID = &g_CustomSteamID;
		else
			return pContext->ThrowNativeError("Custom SteamID not set.");
	}
	if (!pSteamID)
		return pContext->ThrowNativeError("No SteamID found for client %d", params[1]);

	char *steamIDString = new char[params[3]];
	int numbytes = g_pSM->Format(steamIDString, params[3], "%llu", pSteamID->ConvertToUint64());

	pContext->StringToLocal(params[2], numbytes, steamIDString);
	return numbytes;
}

static cell_t GetCSteamIDFromRenderedID(IPluginContext *pContext, const cell_t *params)
{
	char *pRenderedSteamID;
	pContext->LocalToString(params[1], &pRenderedSteamID);

	CSteamID steamID = atocsteamid(pRenderedSteamID);

	if (steamID.IsValid())
	{
		char *steamIDString = new char[params[3]];
		int numbytes = g_pSM->Format(steamIDString, params[3], "%llu", steamID.ConvertToUint64());

		pContext->StringToLocal(params[2], numbytes, steamIDString);
		return numbytes;
	} else {
		return pContext->ThrowNativeError("%s is not a valid SteamID", pRenderedSteamID);
	}
}

static cell_t SetCustomSteamID(IPluginContext *pContext, const cell_t *params)
{
	char *pRenderedSteamID;
	pContext->LocalToString(params[1], &pRenderedSteamID);

	CSteamID steamID = atocsteamid(pRenderedSteamID);

	if (steamID.IsValid())
	{
		g_CustomSteamID = steamID;
		return true;
	} else {
		g_CustomSteamID = k_steamIDNil;
		return pContext->ThrowNativeError("%s is not a valid SteamID", pRenderedSteamID);
	}
}

static cell_t GetCustomSteamID(IPluginContext *pContext, const cell_t *params)
{
	if (!g_CustomSteamID.IsValid())
		return pContext->ThrowNativeError("Custom SteamID not set.");

	char *steamIDString = new char[params[2]];
	int numbytes = g_pSM->Format(steamIDString, params[2], "%s", g_CustomSteamID.Render());

	pContext->StringToLocal(params[1], numbytes, steamIDString);
	return numbytes;
}

static cell_t GroupIDToCSteamID(IPluginContext *pContext, const cell_t *params)
{
	char *steamIDString = new char[params[3]];
	int numbytes = g_pSM->Format(steamIDString, params[3], "%llu", CSteamID(params[1], k_EUniversePublic, k_EAccountTypeClan).ConvertToUint64());

	pContext->StringToLocal(params[2], numbytes, steamIDString);
	return numbytes;
}

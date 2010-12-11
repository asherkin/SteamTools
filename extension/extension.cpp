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

#include "extension.h"
#include "filesystem.h"
#include "tickets.h"

/**
 * @file extension.cpp
 * @brief SteamTools extension code.
 */

SteamTools g_SteamTools;
SMEXT_LINK(&g_SteamTools);

SH_DECL_HOOK1_void(IServerGameDLL, Think, SH_NOATTRIB, 0, bool);
SH_DECL_HOOK0(ISteamMasterServerUpdater001, WasRestartRequested, SH_NOATTRIB, 0, bool);
SH_DECL_HOOK4(ISteamGameServer010, SendUserConnectAndAuthenticate, SH_NOATTRIB, 0, bool, uint32, const void *, uint32, CSteamID *);

ConVar SteamToolsVersion("steamtools_version", SMEXT_CONF_VERSION, FCVAR_NOTIFY|FCVAR_REPLICATED, SMEXT_CONF_DESCRIPTION);

IServerGameDLL *g_pServerGameDLL = NULL;
ICvar *g_pLocalCVar = NULL;
IFileSystem *g_pFileSystem = NULL;

ISteamGameServer010 *g_pSteamGameServer = NULL;
ISteamMasterServerUpdater001 *g_pSteamMasterServerUpdater = NULL;
ISteamUtils005 *g_pSteamUtils = NULL;
ISteamGameServerStats001 *g_pSteamGameServerStats = NULL;

SteamAPICall_t g_SteamAPICall = k_uAPICallInvalid;
CUtlVector<SteamAPICall_t> g_RequestUserStatsSteamAPICalls;
CUtlVector<CSteamClient> g_SteamClients;

typedef HSteamPipe (*GetPipeFn)();
typedef HSteamUser (*GetUserFn)();

typedef bool (*GetCallbackFn)(HSteamPipe hSteamPipe, CallbackMsg_t *pCallbackMsg);
typedef void (*FreeLastCallbackFn)(HSteamPipe hSteamPipe);

GetPipeFn g_GameServerSteamPipe;
GetUserFn g_GameServerSteamUser;

GetCallbackFn GetCallback;
FreeLastCallbackFn FreeLastCallback;

int g_ThinkHookID = 0;
int g_WasRestartRequestedHookID = 0;
int g_SendUserConnectAndAuthenticateHookID = 0;

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
	{ NULL,									NULL }
};

void Hook_Think(bool finalTick)
{
	if(g_pSteamGameServer)
	{
		CallbackMsg_t callbackMsg;
		if (GetCallback(g_GameServerSteamPipe(), &callbackMsg))
		{
			switch (callbackMsg.m_iCallback)
			{
			case GSClientGroupStatus_t::k_iCallback:
				{
					GSClientGroupStatus_t *GroupStatus = (GSClientGroupStatus_t *)callbackMsg.m_pubParam;

					for ( int i = 0; i < g_SteamClients.Count(); ++i )
					{
						if (g_SteamClients.Element(i) == GroupStatus->m_SteamIDUser)
						{
							cell_t cellResults = 0;
							g_pForwardGroupStatusResult->PushCell(g_SteamClients.Element(i).GetIndex());
							g_pForwardGroupStatusResult->PushCell(GroupStatus->m_SteamIDGroup.GetAccountID());
							g_pForwardGroupStatusResult->PushCell(GroupStatus->m_bMember);
							g_pForwardGroupStatusResult->PushCell(GroupStatus->m_bOfficer);
							g_pForwardGroupStatusResult->Execute(&cellResults);
							break;
						}
					}

					FreeLastCallback(g_GameServerSteamPipe());
					break;
				}
			case GSGameplayStats_t::k_iCallback:
				{
					GSGameplayStats_t *GameplayStats = (GSGameplayStats_t *)callbackMsg.m_pubParam;

					if (GameplayStats->m_eResult == k_EResultOK)
					{
						cell_t cellResults = 0;
						g_pForwardGameplayStats->PushCell(GameplayStats->m_nRank);
						g_pForwardGameplayStats->PushCell(GameplayStats->m_unTotalConnects);
						g_pForwardGameplayStats->PushCell(GameplayStats->m_unTotalMinutesPlayed);
						g_pForwardGameplayStats->Execute(&cellResults);
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
						cell_t cellResults = 0;
						g_pForwardSteamServersConnected->Execute(&cellResults);
						g_SteamServersConnected = true;
					}
					break;
				}
			case SteamServersDisconnected_t::k_iCallback:
				{
					if (g_SteamServersConnected)
					{
						cell_t cellResults = 0;
						g_pForwardSteamServersDisconnected->Execute(&cellResults);
						g_SteamServersConnected = false;
					}
					break;
				}
			case SteamAPICallCompleted_t::k_iCallback:
				{
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
								cell_t cellResults = 0;

								g_pForwardReputation->PushCell(Reputation.m_unReputationScore);
								g_pForwardReputation->PushCell(Reputation.m_bBanned);
								g_pForwardReputation->PushCell(Reputation.m_unBannedIP);
								g_pForwardReputation->PushCell(Reputation.m_usBannedPort);
								g_pForwardReputation->PushCell(Reputation.m_ulBannedGameID);
								g_pForwardReputation->PushCell(Reputation.m_unBanExpires);

								g_pForwardReputation->Execute(&cellResults);
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
								for ( int i = 0; i < g_SteamClients.Count(); ++i )
								{
									if (g_SteamClients.Element(i) == StatsReceived.m_steamIDUser)
									{
										cell_t cellResults = 0;
										g_pForwardClientReceivedStats->PushCell(g_SteamClients.Element(i).GetIndex());
										g_pForwardClientReceivedStats->Execute(&cellResults);
										break;
									}
								}
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

					for ( int i = 0; i < g_SteamClients.Count(); ++i )
					{
						if (g_SteamClients.Element(i) == StatsUnloaded->m_steamIDUser)
						{
							cell_t cellResults = 0;
							g_pForwardClientUnloadedStats->PushCell(g_SteamClients.Element(i).GetIndex());
							g_pForwardClientUnloadedStats->Execute(&cellResults);
							break;
						}
					}

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
	} else {
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

		ISteamClient009 *client = NULL;

		if (!LoadSteamclient(&client))
			return;

		g_pSteamGameServer = (ISteamGameServer010 *)client->GetISteamGenericInterface(g_GameServerSteamUser(), g_GameServerSteamPipe(), STEAMGAMESERVER_INTERFACE_VERSION_010);
		g_pSteamMasterServerUpdater = (ISteamMasterServerUpdater001 *)client->GetISteamGenericInterface(g_GameServerSteamUser(), g_GameServerSteamPipe(), STEAMMASTERSERVERUPDATER_INTERFACE_VERSION_001);
		g_pSteamUtils = (ISteamUtils005 *)client->GetISteamGenericInterface(g_GameServerSteamUser(), g_GameServerSteamPipe(), STEAMUTILS_INTERFACE_VERSION_005);
		g_pSteamGameServerStats = (ISteamGameServerStats001 *)client->GetISteamGenericInterface(g_GameServerSteamUser(), g_GameServerSteamUser(), STEAMGAMESERVERSTATS_INTERFACE_VERSION_001);

		if (!CheckInterfaces())
			return;

		g_WasRestartRequestedHookID = SH_ADD_HOOK(ISteamMasterServerUpdater001, WasRestartRequested, g_pSteamMasterServerUpdater, SH_STATIC(Hook_WasRestartRequested), false);
		g_SendUserConnectAndAuthenticateHookID = SH_ADD_HOOK(ISteamGameServer010, SendUserConnectAndAuthenticate, g_pSteamGameServer, SH_STATIC(Hook_SendUserConnectAndAuthenticate), true);
		
		g_SMAPI->ConPrintf("[STEAMTOOLS] Loading complete.\n");

		cell_t dummy;
		g_pForwardLoaded->Execute(&dummy);

		g_SteamServersConnected = g_pSteamGameServer->LoggedOn();
	}
}

bool CheckInterfaces() 
{
	g_SteamLoadFailed = false;
	
	if (!g_pSteamGameServer)
	{
		g_pSM->LogError(myself, "Could not find interface %s", STEAMGAMESERVER_INTERFACE_VERSION_010);
		g_SteamLoadFailed = true;
	}
	
	if (!g_pSteamMasterServerUpdater)
	{
		g_pSM->LogError(myself, "Could not find interface %s", STEAMMASTERSERVERUPDATER_INTERFACE_VERSION_001);
		g_SteamLoadFailed = true;
	}
	
	if (!g_pSteamUtils)
	{
		g_pSM->LogError(myself, "Could not find interface %s", STEAMUTILS_INTERFACE_VERSION_005);
		g_SteamLoadFailed = true;
	}
	
	if (!g_pSteamGameServerStats)
	{
		g_pSM->LogError(myself, "Could not find interface %s", STEAMGAMESERVERSTATS_INTERFACE_VERSION_001);
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

bool LoadSteamclient(ISteamClient009 **pSteamClient, int method)
{
	if(!g_GameServerSteamPipe || !g_GameServerSteamUser || !g_GameServerSteamPipe() || !g_GameServerSteamUser())
		return false;

	HMODULE steamclient_library = NULL;
	ISteamClient009 *pLocalSteamClient = NULL;

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

	pLocalSteamClient = (ISteamClient009 *)steamclient(STEAMCLIENT_INTERFACE_VERSION_009, NULL);

	ISteamGameServer010 *gameserver = (ISteamGameServer010 *)pLocalSteamClient->GetISteamGenericInterface(g_GameServerSteamUser(), g_GameServerSteamPipe(), STEAMGAMESERVER_INTERFACE_VERSION_010);

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

#if !defined _WIN32 && !defined _LINUX
#error This extension is only supported on Windows and Linux.
#endif

	if (strcmp(g_pSM->GetGameFolderName(), "tf") != 0)
	{
		g_pSM->LogMessage(myself, "Functionality is only guaranteed on Team Fortress 2, and may break features of other mods.");
	}

	g_ThinkHookID = SH_ADD_HOOK(IServerGameDLL, Think, g_pServerGameDLL, SH_STATIC(Hook_Think), true);

	g_pShareSys->AddNatives(myself, g_ExtensionNatives);
	g_pShareSys->RegisterLibrary(myself, "SteamTools");
	playerhelpers->AddClientListener(this);
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

	if (late)
	{
		int iMaxClients = playerhelpers->GetMaxClients();
		for (int iClient = 1; iClient <= iMaxClients; iClient++) {
			IGamePlayer *pPlayer = playerhelpers->GetGamePlayer(iClient);
			if (pPlayer == NULL) continue;
			//if (pPlayer->IsConnected() == false) continue;
			if (pPlayer->IsAuthorized() == false) continue;

			// Add client
			CSteamID steamID = SteamIDToCSteamID(pPlayer->GetAuthString());
			g_SteamClients.AddToTail(CSteamClient(iClient, steamID));
		}

		if (g_pSteamGameServer)
		{
			cell_t dummy;

			g_pForwardLoaded->Execute(&dummy);

			if (g_SteamServersConnected)
			{
				g_pForwardSteamServersConnected->Execute(&dummy);
			} else {
				g_pForwardSteamServersDisconnected->Execute(&dummy);
			}
		}
	}

	g_SMAPI->ConPrintf("[STEAMTOOLS] Initial loading stage complete...\n");

	return true;
}

void SteamTools::OnClientAuthorized(int client, const char *authstring)
{
	CSteamID steamID = SteamIDToCSteamID(authstring);
	for ( int i = 0; i < g_SteamClients.Count(); ++i )
	{
		if (g_SteamClients.Element(i) == steamID)
		{
			g_SteamClients.Element(i).SetIndex(client);
			break;
		}
	}
}

void SteamTools::OnClientDisconnecting(int client)
{
	for ( int i = 0; i < g_SteamClients.Count(); ++i )
	{
		if (g_SteamClients.Element(i) == client)
		{
			g_SteamClients.Remove(i);
			break;
		}
	}
}

void SteamTools::OnPluginLoaded(IPlugin *plugin)
{
	if (g_pSteamGameServer)
	{
		cell_t result;

		IPluginContext *pluginContext = plugin->GetRuntime()->GetDefaultContext();

		IPluginFunction *steamToolsLoadedCallback = pluginContext->GetFunctionByName("Steam_FullyLoaded");

		if (steamToolsLoadedCallback)
		{
			int funcError = steamToolsLoadedCallback->CallFunction(NULL, 0, &result);
			if (funcError != SP_ERROR_NONE)
			{
				pluginContext->ThrowNativeErrorEx(SP_ERROR_NOT_RUNNABLE, "Failed to fire Steam_FullyLoaded callback: %d", funcError);
			}
		} else {
			// This plugin doesn't use SteamTools
			return;
		}

		IPluginFunction *steamConnectionStateCallback = NULL;
		if (g_SteamServersConnected)
		{
			pluginContext->GetFunctionByName("Steam_SteamServersConnected");
		} else {
			pluginContext->GetFunctionByName("Steam_SteamServersDisconnected");
		}

		if (steamConnectionStateCallback)
		{
			int funcError = steamConnectionStateCallback->CallFunction(NULL, 0, &result);
			if (funcError != SP_ERROR_NONE)
			{
				pluginContext->ThrowNativeErrorEx(SP_ERROR_NOT_RUNNABLE, "Failed to fire Steam_SteamServers[Connected|Disconnected] callback: %d", funcError);
			}
		}
	}
}

bool Hook_WasRestartRequested()
{
	cell_t cellResults = 0;
	bool bWasRestartRequested = false;
	if ((bWasRestartRequested = SH_CALL(g_pSteamMasterServerUpdater, &ISteamMasterServerUpdater001::WasRestartRequested)()))
	{
		g_pForwardRestartRequested->Execute(&cellResults);
	}
	RETURN_META_VALUE(MRES_SUPERCEDE, (cellResults < Pl_Handled)?bWasRestartRequested:false);
}

bool Hook_SendUserConnectAndAuthenticate(uint32 unIPClient, const void *pvAuthBlob, uint32 cubAuthBlobSize, CSteamID *pSteamIDUser)
{
	bool ret = META_RESULT_ORIG_RET(bool);
	AuthBlob_t authblob = AuthBlob_t(pvAuthBlob);

	if (!ret)
	{
		if (!authblob.ownership) {
			g_pSM->LogMessage(myself, "Client connecting from %u.%u.%u.%u (%s) isn't using Steam.", (unIPClient) & 0xFF, (unIPClient >> 8) & 0xFF, (unIPClient >> 16) & 0xFF, (unIPClient >> 24) & 0xFF, authblob.section->steamid.Render());
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
		g_SteamClients.AddToTail(CSteamClient(*pSteamIDUser, NULL));
		RETURN_META_VALUE(MRES_IGNORED, (bool)NULL);
	} else if (!authblob.section || !authblob.ownership) {
		g_pSM->LogError(myself, "SendUserConnectAndAuthenticate: Aborting due to missing sections in ticket. (authblob.length = %u)", authblob.length);
		g_SteamClients.AddToTail(CSteamClient(*pSteamIDUser, NULL));
		RETURN_META_VALUE(MRES_IGNORED, (bool)NULL);
	}

	if (authblob.ownership->ticket->version != 4)
	{
		g_pSM->LogError(myself, "SendUserConnectAndAuthenticate: Aborting due to unexpected ticket version. (ticketVersion = %u)", authblob.ownership->ticket->version);
		g_SteamClients.AddToTail(CSteamClient(*pSteamIDUser, NULL));
		RETURN_META_VALUE(MRES_IGNORED, (bool)NULL);
	}

	g_SteamClients.AddToTail(CSteamClient(*pSteamIDUser, authblob.ownership->ticket->licenses));

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
	for ( int i = 0; i < g_SteamClients.Count(); ++i )
	{
		g_SteamClients.Remove(i);
	}

	if (g_ThinkHookID != 0)
	{
		SH_REMOVE_HOOK_ID(g_ThinkHookID);
		g_ThinkHookID = 0;
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

	g_pForwards->ReleaseForward(g_pForwardGroupStatusResult);
	g_pForwards->ReleaseForward(g_pForwardGameplayStats);
	g_pForwards->ReleaseForward(g_pForwardReputation);

	g_pForwards->ReleaseForward(g_pForwardRestartRequested);

	g_pForwards->ReleaseForward(g_pForwardSteamServersConnected);
	g_pForwards->ReleaseForward(g_pForwardSteamServersDisconnected);

	playerhelpers->RemoveClientListener(this);
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

static cell_t RequestGroupStatus(IPluginContext *pContext, const cell_t *params)
{
	if (!g_pSteamGameServer)
		return 0;

	for ( int i = 0; i < g_SteamClients.Count(); ++i )
	{
		if (g_SteamClients.Element(i) == params[1])
		{
			return g_pSteamGameServer->RequestUserGroupStatus(g_SteamClients.Element(i).GetSteamID(), CSteamID(params[2], k_EUniversePublic, k_EAccountTypeClan));
		}
	}
	return pContext->ThrowNativeError("No g_SteamClients entry found for client %d", params[1]);
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

	for ( int i = 0; i < g_SteamClients.Count(); ++i )
	{
		if (g_SteamClients.Element(i) == params[1])
		{
			g_RequestUserStatsSteamAPICalls.AddToTail(g_pSteamGameServerStats->RequestUserStats(g_SteamClients.Element(i).GetSteamID()));
			return 0;
		}
	}
	return pContext->ThrowNativeError("No g_SteamClients entry found for client %d", params[1]);
}

static cell_t GetStatInt(IPluginContext *pContext, const cell_t *params)
{
	if (!g_pSteamGameServerStats)
		return 0;

	for ( int i = 0; i < g_SteamClients.Count(); ++i )
	{
		if (g_SteamClients.Element(i) == params[1])
		{
			int32 data;
			char *strStatName;
			pContext->LocalToString(params[2], &strStatName);
			if (g_pSteamGameServerStats->GetUserStat(g_SteamClients.Element(i).GetSteamID(), strStatName, &data))
			{
				return data;
			} else {
				return pContext->ThrowNativeError("Failed to get stat %s for client %d", strStatName, params[1]);
			}
			break;
		}
	}
	return pContext->ThrowNativeError("No g_SteamClients entry found for client %d", params[1]);
}

static cell_t GetStatFloat(IPluginContext *pContext, const cell_t *params)
{
	if (!g_pSteamGameServerStats)
		return 0;

	for ( int i = 0; i < g_SteamClients.Count(); ++i )
	{
		if (g_SteamClients.Element(i) == params[1])
		{
			float data;
			char *strStatName;
			pContext->LocalToString(params[2], &strStatName);
			if (g_pSteamGameServerStats->GetUserStat(g_SteamClients.Element(i).GetSteamID(), strStatName, &data))
			{
				return sp_ftoc(data);
			} else {
				return pContext->ThrowNativeError("Failed to get stat %s for client %d", strStatName, params[1]);
			}
			break;
		}
	}
	return pContext->ThrowNativeError("No g_SteamClients entry found for client %d", params[1]);
}

static cell_t IsAchieved(IPluginContext *pContext, const cell_t *params)
{
	if (!g_pSteamGameServerStats)
		return 0;

	for ( int i = 0; i < g_SteamClients.Count(); ++i )
	{
		if (g_SteamClients.Element(i) == params[1])
		{
			bool bAchieved;
			char *strAchName;
			pContext->LocalToString(params[2], &strAchName);
			if (g_pSteamGameServerStats->GetUserAchievement(g_SteamClients.Element(i).GetSteamID(), strAchName, &bAchieved))
			{
				return bAchieved;
			} else {
				return pContext->ThrowNativeError("Failed to get achievement %s for client %d", strAchName, params[1]);
			}
			break;
		}
	}
	return pContext->ThrowNativeError("No g_SteamClients entry found for client %d", params[1]);
}

static cell_t GetNumClientSubscriptions(IPluginContext *pContext, const cell_t *params)
{
	for ( int i = 0; i < g_SteamClients.Count(); ++i )
	{
		if (g_SteamClients.Element(i) == params[1])
		{
			if (g_SteamClients.Element(i).GetSubIDs() != NULL)
			{
				uint32 *subIDs = g_SteamClients.Element(i).GetSubIDs();
				return sizeof(*subIDs) / sizeof(uint32);
			} else {
				return pContext->ThrowNativeError("No subscriptions were found for client %d", params[1]);
			}
			break;
		}
	}
	return pContext->ThrowNativeError("No g_SteamClients entry found for client %d", params[1]);
}

static cell_t GetClientSubscription(IPluginContext *pContext, const cell_t *params)
{
	for ( int i = 0; i < g_SteamClients.Count(); ++i )
	{
		if (g_SteamClients.Element(i) == params[1])
		{
			if (g_SteamClients.Element(i).GetSubIDs() != NULL)
			{
				int index = params[2];
				if (index >= sizeof(*g_SteamClients.Element(i).GetSubIDs()) / sizeof(uint32))
				{
					return pContext->ThrowNativeError("Subscription index %d is out of bounds for client %d", index, params[1]);
				} else {
					uint32 *subIDs = g_SteamClients.Element(i).GetSubIDs();
					return subIDs[index];
				}
			} else {
				return pContext->ThrowNativeError("No subscriptions were found for client %d", params[1]);
			}
			break;
		}
	}
	return pContext->ThrowNativeError("No g_SteamClients entry found for client %d", params[1]);
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

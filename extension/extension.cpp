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
ISteamUtils005 *g_pSteamUtils = NULL;
ISteamGameServerStats001 *g_pSteamGameServerStats = NULL;

ISteamGameServer010 *g_pSteamGameServer010 = NULL;

SteamAPICall_t g_SteamAPICall = k_uAPICallInvalid;
CUtlVector<SteamAPICall_t> g_RequestUserStatsSteamAPICalls;
CUtlVector<CStatsClient> g_StatsClients;

typedef HSteamPipe (*GetPipeFn)();
typedef HSteamUser (*GetUserFn)();

typedef bool (*GetCallbackFn)(HSteamPipe hSteamPipe, CallbackMsg_t *pCallbackMsg);
typedef void (*FreeLastCallbackFn)(HSteamPipe hSteamPipe);

GetPipeFn g_GameServerSteamPipe;
GetUserFn g_GameServerSteamUser;

GetCallbackFn GetCallback;
FreeLastCallbackFn FreeLastCallback;

int g_GameFrameHookID = 0;
int g_WasRestartRequestedHookID = 0;

bool g_SteamServersConnected = false;

IForward * g_pForwardGroupStatusResult = NULL;
IForward * g_pForwardGameplayStats = NULL;
IForward * g_pForwardReputation = NULL;
IForward * g_pForwardRestartRequested = NULL;

IForward * g_pForwardSteamServersConnected = NULL;
IForward * g_pForwardSteamServersDisconnected = NULL;

sp_nativeinfo_t g_ExtensionNatives[] =
{
	{ "Steam_RequestGroupStatus",		RequestGroupStatus },
	{ "Steam_RequestGameplayStats",		RequestGameplayStats },
	{ "Steam_RequestServerReputation",	RequestServerReputation },
	{ "Steam_ForceHeartbeat",			ForceHeartbeat },
	{ "Steam_IsVACEnabled",				IsVACEnabled },
	{ "Steam_IsConnected",				IsConnected },
	{ "Steam_GetPublicIP",				GetPublicIP },
	{ NULL,								NULL }
};

/**
 * =============================================================================
 * Testing Area:
 * =============================================================================
 */



/**
 * =============================================================================
 */

void Hook_GameFrame(bool simulating)
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

						cell_t cellResults = 0;
						g_pForwardGroupStatusResult->PushString(GroupStatus->m_SteamIDUser.Render());
						g_pForwardGroupStatusResult->PushCell(GroupStatus->m_SteamIDGroup.GetAccountID());
						g_pForwardGroupStatusResult->PushCell(GroupStatus->m_bMember);
						g_pForwardGroupStatusResult->PushCell(GroupStatus->m_bOfficer);
						g_pForwardGroupStatusResult->Execute(&cellResults);

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
								for ( int i = 0; i < g_StatsClients.Count(); ++i )
								{
									if (g_StatsClients.Element(i) == StatsReceived.m_steamIDUser)
									{
										g_StatsClients.Element(i).HasStats(true);
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
			case GSStatsUnloaded_t::k_iCallback:
				{
					GSStatsUnloaded_t *StatsUnloaded = (GSStatsUnloaded_t *)callbackMsg.m_pubParam;

					for ( int i = 0; i < g_StatsClients.Count(); ++i )
					{
						if (g_StatsClients.Element(i) == StatsUnloaded->m_steamIDUser)
						{
							g_StatsClients.Element(i).HasStats(false);
							break;
						}
					}

					FreeLastCallback(g_GameServerSteamPipe());
					break;
				}
			default:
				{
					//g_SMAPI->ConPrintf("Unhandled Callback: %d", callbackMsg.m_iCallback);
					break;
				}
			}
		}
	} else {

#if defined _WIN32
		HMODULE steamclient_library = GetModuleHandle("steamclient.dll");

		CreateInterfaceFn steamclient = (CreateInterfaceFn)GetProcAddress(steamclient_library, "CreateInterface");

		GetCallback = (GetCallbackFn)GetProcAddress(steamclient_library, "Steam_BGetCallback");
		FreeLastCallback = (FreeLastCallbackFn)GetProcAddress(steamclient_library, "Steam_FreeLastCallback");

		HMODULE steam_api_library = GetModuleHandle("steam_api.dll");

		g_GameServerSteamPipe = (GetPipeFn)GetProcAddress(steam_api_library, "SteamGameServer_GetHSteamPipe");
		g_GameServerSteamUser = (GetUserFn)GetProcAddress(steam_api_library, "SteamGameServer_GetHSteamUser");
#elif defined _LINUX
		void* steamclient_library = dlopen("steamclient.so", RTLD_NOW);
		
		CreateInterfaceFn steamclient = dlsym(steamclient_library, "CreateInterface");

		GetCallback = (GetCallbackFn)dlsym(steamclient_library, "Steam_BGetCallback");
		FreeLastCallback = (FreeLastCallbackFn)dlsym(steamclient_library, "Steam_FreeLastCallback");

		void* steam_api_library = dlopen("libsteam_api.so", RTLD_NOW);

		g_GameServerSteamPipe = (GetPipeFn)dlsym(steam_api_library, "SteamGameServer_GetHSteamPipe");
		g_GameServerSteamUser = (GetUserFn)dlsym(steam_api_library, "SteamGameServer_GetHSteamUser");
#else
		// report error ...
		return;
#endif
		ISteamClient008 *client = (ISteamClient008 *)steamclient(STEAMCLIENT_INTERFACE_VERSION_008, NULL);

		g_pSM->LogMessage(myself, "Steam library loading complete.");

		// let's not get impatient
		if(g_GameServerSteamPipe() == 0 || g_GameServerSteamUser() == 0)
			return;

		g_pSM->LogMessage(myself, "Acquiring interfaces and hooking functions...");

		g_pSteamGameServer = (ISteamGameServer008 *)client->GetISteamGameServer(g_GameServerSteamUser(), g_GameServerSteamPipe(), STEAMGAMESERVER_INTERFACE_VERSION_008);
		g_pSteamMasterServerUpdater = (ISteamMasterServerUpdater001 *)client->GetISteamMasterServerUpdater(g_GameServerSteamUser(), g_GameServerSteamPipe(), STEAMMASTERSERVERUPDATER_INTERFACE_VERSION_001);
		g_pSteamUtils = (ISteamUtils005 *)client->GetISteamUtils(g_GameServerSteamPipe(), STEAMUTILS_INTERFACE_VERSION_005);
		g_pSteamGameServerStats = (ISteamGameServerStats001 *)client->GetISteamGenericInterface(g_GameServerSteamUser(), g_GameServerSteamUser(), STEAMGAMESERVERSTATS_INTERFACE_VERSION_001);

		g_pSteamGameServer010 = (ISteamGameServer010 *)client->GetISteamGameServer(g_GameServerSteamUser(), g_GameServerSteamPipe(), STEAMGAMESERVER_INTERFACE_VERSION_010);

		g_WasRestartRequestedHookID = SH_ADD_HOOK(ISteamMasterServerUpdater001, WasRestartRequested, g_pSteamMasterServerUpdater, SH_STATIC(Hook_WasRestartRequested), false);

		g_pSM->LogMessage(myself, "Loading complete.");

		g_SteamServersConnected = g_pSteamGameServer->LoggedOn();
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

	g_pForwardGroupStatusResult = g_pForwards->CreateForward("Steam_GroupStatusResult", ET_Ignore, 4, NULL, Param_String, Param_Cell, Param_Cell, Param_Cell);
	g_pForwardGameplayStats = g_pForwards->CreateForward("Steam_GameplayStats", ET_Ignore, 3, NULL, Param_Cell, Param_Cell, Param_Cell);
	g_pForwardReputation = g_pForwards->CreateForward("Steam_Reputation", ET_Ignore, 6, NULL, Param_Cell, Param_Cell, Param_Cell, Param_Cell, Param_Cell, Param_Cell);
	g_pForwardRestartRequested = g_pForwards->CreateForward("Steam_RestartRequested", ET_Ignore, 0, NULL);

	g_pForwardSteamServersConnected = g_pForwards->CreateForward("Steam_SteamServersConnected", ET_Ignore, 0, NULL);
	g_pForwardSteamServersDisconnected = g_pForwards->CreateForward("Steam_SteamServersDisconnected", ET_Ignore, 0, NULL);

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
	g_pForwards->ReleaseForward(g_pForwardGameplayStats);
	g_pForwards->ReleaseForward(g_pForwardReputation);
	g_pForwards->ReleaseForward(g_pForwardRestartRequested);

	g_pForwards->ReleaseForward(g_pForwardSteamServersConnected);
	g_pForwards->ReleaseForward(g_pForwardSteamServersDisconnected);
}

static cell_t RequestGroupStatus(IPluginContext *pContext, const cell_t *params)
{
	char * strUserID;
	pContext->LocalToString(params[1], &strUserID);
	return g_pSteamGameServer->RequestUserGroupStatus(SteamIDToCSteamID(strUserID), CSteamID(params[2], k_EUniversePublic, k_EAccountTypeClan));
}

static cell_t RequestGameplayStats(IPluginContext *pContext, const cell_t *params)
{
	g_pSteamGameServer->GetGameplayStats();
	return 0;
}

static cell_t RequestServerReputation(IPluginContext *pContext, const cell_t *params)
{
	if (g_SteamAPICall == k_uAPICallInvalid)
	{
		g_SteamAPICall = g_pSteamGameServer010->GetServerReputation();
		return true;
	} else {
		return false;
	}
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

static cell_t IsConnected(IPluginContext *pContext, const cell_t *params)
{
	return g_pSteamGameServer->LoggedOn();
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

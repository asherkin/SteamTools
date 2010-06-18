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

#pragma semicolon 1

#include <sourcemod>

#define AUTOLOAD_EXTENSIONS
#define REQUIRE_EXTENSIONS
#include <steamtools>

#define PLUGIN_VERSION "0.2.1"

public Plugin:myinfo = {
	name        = "SteamTools Tester",
	author      = "Asher Baker (asherkin)",
	description = "Plugin for testing the SteamTools extension.",
	version     = PLUGIN_VERSION,
	url         = "http://limetech.org/"
};

new ReplySource:Async_GroupStatus_Reply;
new ReplySource:Async_GameplayStats_Reply;
new ReplySource:Async_ServerReputation_Reply;

new Async_GroupStatus_Client;
new Async_GameplayStats_Client;
new Async_ServerReputation_Client;

public OnPluginStart()
{
	LoadTranslations("common.phrases");

	RegAdminCmd("sm_groupstatus", Command_GroupStatus, ADMFLAG_ROOT, "Requests a client's membership status in a Steam Community Group.");
	RegAdminCmd("sm_printgameplaystats", Command_GameplayStats, ADMFLAG_ROOT, "Requests a server's gameplay stats from SteamWorks.");
	RegAdminCmd("sm_printserverreputation", Command_ServerReputation, ADMFLAG_ROOT, "Requests a server's reputation from the Steam Master Servers.");
	RegAdminCmd("sm_forceheartbeat", Command_Heartbeat, ADMFLAG_ROOT, "Sends a heartbeat to the Steam Master Servers.");
	RegAdminCmd("sm_printvacstatus", Command_VACStatus, ADMFLAG_ROOT, "Shows the current VAC status.");
	RegAdminCmd("sm_printconnectionstatus", Command_ConnectionStatus, ADMFLAG_ROOT, "Shows the current Steam connection status.");
	RegAdminCmd("sm_printip", Command_PrintIP, ADMFLAG_ROOT, "Shows the server's current external IP address.");
}

public Action:Command_GroupStatus(client, args)
{
	if (args != 2)
	{
		ReplyToCommand(client, "[SM] Usage: sm_groupstatus <client> <group>");
		return Plugin_Handled;
	}

	new String:arg1[32];
	new String:arg2[32];

	GetCmdArg(1, arg1, sizeof(arg1));
	GetCmdArg(2, arg2, sizeof(arg2));

	new String:clientAuthString[64];
 
	new String:target_name[MAX_TARGET_LENGTH];
	new target_list[MAXPLAYERS], target_count;
	new bool:tn_is_ml;
 
	if ((target_count = ProcessTargetString(
			arg1,
			client,
			target_list,
			MAXPLAYERS,
			COMMAND_FILTER_NO_IMMUNITY,
			target_name,
			sizeof(target_name),
			tn_is_ml)) <= 0)
	{
		ReplyToTargetError(client, target_count);
		return Plugin_Handled;
	}

	new bool:didLastRequestWork = false;
 
	for (new i = 0; i < target_count; i++)
	{
		GetClientAuthString(target_list[i], clientAuthString, 64);
		didLastRequestWork = Steam_RequestGroupStatus(clientAuthString, StringToInt(arg2));
	}

	ReplyToCommand(client, "[SM] %s.", didLastRequestWork?"Group status requested":"Error in requesting group status, not connected to Steam");

	Async_GroupStatus_Client = client;
	Async_GroupStatus_Reply = GetCmdReplySource();

	return Plugin_Handled;
}

public Action:Command_GameplayStats(client, args)
{
	Steam_RequestGameplayStats();
	ReplyToCommand(client, "[SM] Gameplay Stats Requested.");

	Async_GameplayStats_Client = client;
	Async_GameplayStats_Reply = GetCmdReplySource();

	return Plugin_Handled;
}

public Action:Command_ServerReputation(client, args)
{
	Steam_RequestServerReputation();
	ReplyToCommand(client, "[SM] Server Reputation Requested.");

	Async_ServerReputation_Client = client;
	Async_ServerReputation_Reply = GetCmdReplySource();

	return Plugin_Handled;
}

public Action:Command_Heartbeat(client, args)
{
	Steam_ForceHeartbeat();
	ReplyToCommand(client, "[SM] Heartbeat Sent.");
	return Plugin_Handled;
}

public Action:Command_VACStatus(client, args)
{
	ReplyToCommand(client, "[SM] VAC is %s.", Steam_IsVACEnabled()?"active":"not active");
	return Plugin_Handled;
}

public Action:Command_ConnectionStatus(client, args)
{
	ReplyToCommand(client, "[SM] %s to Steam servers.", Steam_IsConnected()?"Connected":"Not connected");
	return Plugin_Handled;
}

public Action:Command_PrintIP(client, args)
{
	new octets[4];
	Steam_GetPublicIP(octets);
	ReplyToCommand(client, "[SM] Server IP Address: %d.%d.%d.%d", octets[0], octets[1], octets[2], octets[3]);
	return Plugin_Handled;
}

public Action:Steam_GroupStatusResult(String:clientAuthString[64], groupAccountID, bool:groupMember, bool:groupOfficer)
{
	new String:authBuffer[64];
	
	for (new i = 1; i < MaxClients; i++)
	{
		GetClientAuthString(i, authBuffer, 64);
		if (StrEqual(clientAuthString, authBuffer))
		{
			SetCmdReplySource(Async_GroupStatus_Reply);
			ReplyToCommand(Async_GroupStatus_Client, "[SM] %N is %s in group %d.", i, groupMember?(groupOfficer?"an officer":"a member"):"not a member", groupAccountID);
			Async_GroupStatus_Reply = SM_REPLY_TO_CONSOLE;
			Async_GroupStatus_Client = 0;
			break;
		}
	}
	return Plugin_Continue;
}

public Action:Steam_GameplayStats(rank, totalConnects, totalMinutesPlayed)
{
	SetCmdReplySource(Async_GameplayStats_Reply);
	ReplyToCommand(Async_GameplayStats_Client, "[SM] Rank: %d. Total Connects: %d. Total Minutes Played: %d.", rank, totalConnects, totalMinutesPlayed);
	Async_GameplayStats_Reply = SM_REPLY_TO_CONSOLE;
	Async_GameplayStats_Client = 0;
	return Plugin_Continue;
}

public Action:Steam_Reputation(reputationScore, bool:banned, bannedIP, bannedPort, bannedGameID, banExpires)
{
	SetCmdReplySource(Async_ServerReputation_Reply);
	ReplyToCommand(Async_ServerReputation_Client, "[SM] Reputation Score: %d. Banned: %s.", reputationScore, banned?"true":"false");
	Async_ServerReputation_Reply = SM_REPLY_TO_CONSOLE;
	Async_ServerReputation_Client = 0;
	return Plugin_Continue;
}

public Action:Steam_RestartRequested()
{
	PrintToServer("[SM] Server needs to be restarted due to an update.");
	return Plugin_Continue;
}

public Action:Steam_SteamServersConnected()
{
	PrintToChatAll("[SM] Lost connection to Steam servers.");
	return Plugin_Continue;
}

public Action:Steam_SteamServersDisconnected()
{
	PrintToChatAll("[SM] Connection to Steam servers reestablished.");
	return Plugin_Continue;
}
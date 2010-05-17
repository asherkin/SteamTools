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

#include <steamtools>

#define PLUGIN_VERSION "0.1.0"

public Plugin:myinfo = {
	name        = "SteamTools Tester",
	author      = "Asher Baker (asherkin)",
	description = "Plugin for testing the SteamTools extension.",
	version     = PLUGIN_VERSION,
	url         = "http://limetech.org/"
};

// START Loading Code

new bool:g_SteamToolsAvailable = false;

public OnPluginStart()
{
	LoadTranslations("common.phrases");

	RegAdminCmd("sm_printvacstatus", Command_VACStatus, ADMFLAG_ROOT, "Shows the current VAC status.");
	RegAdminCmd("sm_forceheartbeat", Command_Heartbeat, ADMFLAG_ROOT, "Sends a heartbeat to the Steam Master Servers.");
	RegAdminCmd("sm_printip", Command_PrintIP, ADMFLAG_ROOT, "Shows the server's current external IP address.");
	RegAdminCmd("sm_groupstatus", Command_GroupStatus, ADMFLAG_ROOT, "Requests a clients membership status in a Steam Community Group.");

	g_SteamToolsAvailable = LibraryExists("SteamTools");
}
 
public OnLibraryRemoved(const String:name[])
{
	if (StrEqual(name, "SteamTools"))
	{
		g_SteamToolsAvailable = false;
	}
}
 
public OnLibraryAdded(const String:name[])
{
	if (StrEqual(name, "SteamTools"))
	{
		new String:myFilename[64];
		new Handle:myself = GetMyHandle();
		GetPluginFilename(myself, myFilename, 64);
		ServerCommand("sm plugins reload %s", myFilename);
	}
}

public OnClientAuthorized(client, const String:auth[])
{
	if (!g_SteamToolsAvailable)
	{
		ServerCommand("sm exts load %s", "steamtools");
	}
}

// END Loading Code

public Action:Steam_RestartRequested()
{
	PrintToChatAll("[SM] Server needs to be restarted due to an update.");
	return Plugin_Continue;
}

public Action:Steam_GroupStatusResult(String:clientAuthString[64], bool:groupMember)
{
	new String:authBuffer[64];
	
	for (new i = 1; i < MaxClients; i++)
	{
		GetClientAuthString(i, authBuffer, 64);
		if (StrEqual(clientAuthString, authBuffer))
		{
			PrintToChatAll("[SM] %N is %s member in group.", i, groupMember?"a":"not a");
			break;
		}
	}
	return Plugin_Continue;
}

public Action:Command_VACStatus(client, args)
{
	if (!g_SteamToolsAvailable) return Plugin_Continue;
	
	ReplyToCommand(client, "[SM] VAC is %s.", Steam_IsVACEnabled()?"active":"not active");
	return Plugin_Handled;
}

public Action:Command_Heartbeat(client, args)
{
	if (!g_SteamToolsAvailable) return Plugin_Continue;
	
	Steam_ForceHeartbeat();
	ReplyToCommand(client, "[SM] Heartbeat Sent.");
	return Plugin_Handled;
}

public Action:Command_PrintIP(client, args)
{
	if (!g_SteamToolsAvailable) return Plugin_Continue;
	
	new octets[4];
	Steam_GetPublicIP(octets);
	ReplyToCommand(client, "[SM] Server IP Address: %d.%d.%d.%d", octets[0], octets[1], octets[2], octets[3]);
	return Plugin_Handled;
}

public Action:Command_GroupStatus(client, args)
{
	if (!g_SteamToolsAvailable) return Plugin_Continue;
	
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
	 
	return Plugin_Handled;
}
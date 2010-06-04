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

#if defined _steamtools_included
#endinput
#endif
#define _steamtools_included

native bool:Steam_RequestGroupStatus(String:clientAuthString[], groupAccountID);
native Steam_ForceHeartbeat();
native bool:Steam_IsVACEnabled();
native bool:Steam_IsConnected();
native Steam_GetPublicIP(octets[4]);
forward Action:Steam_GroupStatusResult(String:clientAuthString[64], groupAccountID, bool:groupMember, bool:groupOfficer);
forward Action:Steam_GameplayStats(rank, totalConnects, totalMinutesPlayed);
forward Action:Steam_RestartRequested();
forward Action:Steam_SteamServersConnected();
forward Action:Steam_SteamServersDisconnected();

public Extension:__ext_SteamTools = 
{
	name = "SteamTools",
	file = "steamtools.ext.2.ep2v",
	autoload = 0,
	required = 0,
}

public __ext_SteamTools_SetNTVOptional()
{
	MarkNativeAsOptional("Steam_RequestGroupStatus");
	MarkNativeAsOptional("Steam_RequestGameplayStats");
	MarkNativeAsOptional("Steam_ForceHeartbeat");
	MarkNativeAsOptional("Steam_IsVACEnabled");
	MarkNativeAsOptional("Steam_IsConnected");
	MarkNativeAsOptional("Steam_GetPublicIP");
}

#if defined _steamtools_included
#endinput
#endif
#define _steamtools_included

native bool:Steam_RequestGroupStatus(client, groupAccountID);
native Steam_RequestGameplayStats();
native Steam_RequestServerReputation();
native Steam_ForceHeartbeat();
native bool:Steam_IsVACEnabled();
native bool:Steam_IsConnected();
native Steam_GetPublicIP(octets[4]);
forward Action:Steam_GroupStatusResult(client, groupAccountID, bool:groupMember, bool:groupOfficer);
forward Action:Steam_GameplayStats(rank, totalConnects, totalMinutesPlayed);
forward Action:Steam_Reputation(reputationScore, bool:banned, bannedIP, bannedPort, bannedGameID, banExpires);
forward Action:Steam_RestartRequested();
forward Action:Steam_SteamServersConnected();
forward Action:Steam_SteamServersDisconnected();

native Steam_RequestStats(client);
native Steam_GetStat(client, String:statName[]);
native Float:Steam_GetStatFloat(client, String:statName[]);
native bool:Steam_IsAchieved(client, String:achievementName[]);
forward Action:Steam_StatsReceived(client);
forward Action:Steam_StatsUnloaded(client);

public Extension:__ext_SteamTools = 
{
	name = "SteamTools",
	file = "steamtools.ext",
#if defined AUTOLOAD_EXTENSIONS
	autoload = 1,
#else
	autoload = 0,
#endif
#if defined REQUIRE_EXTENSIONS
	required = 1,
#else
	required = 0,
#endif
}

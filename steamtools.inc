#if defined _steamtools_included
#endinput
#endif
#define _steamtools_included

native bool:Steam_RequestGroupStatus(String:clientAuthString, groupAccountID);
native Steam_ForceHeartbeat();
native bool:Steam_IsVACEnabled();
native Steam_ReportBotPlayers(bool:reportBotsToMaster);
forward Steam_GroupStatusResult(String:clientAuthString, bool:groupMember);
forward Steam_RestartRequested();

public Extension:__ext_steamtools = 
{
	name = "SteamTools",
	file = "steamtools.ext.2.ep2v",
	autoload = 0,
	#if defined REQUIRE_EXTENSIONS
		required = 1,
	#else
		required = 0,
	#endif
}
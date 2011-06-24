#if defined _steamtools_included
#endinput
#endif
#define _steamtools_included

/**
 * Called after SteamTools has completely finished loading. 
 * No features are available before this point.
 *
 * @noreturn
 */
forward Steam_FullyLoaded();


/**
 * Gets the current status of VAC on the server.
 *
 * @return A bool representing the current VAC status.
 */
native bool:Steam_IsVACEnabled();

/**
 * Gets the server's external IP address, as reported by Steam.
 *
 * @param octets Reference to an array to be filled with the octets of 
 *               the IP address.
 * 
 * @noreturn
 */
native Steam_GetPublicIP(octets[4]);


/**
 * Is fired when the Steam master servers report that your server is 
 * outdated
 *
 * @return Plugin_Continue to continue normal operation or Plugin_Handled 
 *         to block the regular console message.
 */
forward Action:Steam_RestartRequested();


/**
 * Requests a client's status in a Steam group.
 * Response is returned in Steam_GroupStatusResult forward.
 *
 * @param client         Client index.
 * @param groupAccountID 32-bit account ID of group.
 *
 * @return A bool representing whether or not the request was sent to 
 *         Steam.
 */
native bool:Steam_RequestGroupStatus(client, groupAccountID);

/**
 * Called when a response to a group status request is recieved.
 * This is called for all responses recieved, not just ones requested by 
 * your plugin.
 *
 * @param client         Client index.
 * @param groupAccountID 32-bit account ID of group. Make sure to check 
 *                       this agaist the ID you are expecting.
 * @param groupMember    Whether or not the client is a member in the 
 *                       specified group.
 * @param groupMember    Whether or not the client is an officer in the 
 *                       specified group.
 *
 * @noreturn
 */
forward Steam_GroupStatusResult(client, groupAccountID, bool:groupMember, bool:groupOfficer);


native Steam_RequestGameplayStats();
forward Steam_GameplayStats(rank, totalConnects, totalMinutesPlayed);


native Steam_RequestServerReputation();
forward Steam_Reputation(reputationScore, bool:banned, bannedIP, bannedPort, bannedGameID, banExpires);


/**
 * Gets the current Steam connection state, the forwards below fire 
 * whenever this changes.
 *
 * @return Steam connection state.
 */
native bool:Steam_IsConnected();

/**
 * Fired upon a successfull connection to Steam.
 * Is also fired for late-loaded plugins.
 *
 * @noreturn
 */
forward Steam_SteamServersConnected();

/**
 * Fired upon disconnection from Steam.
 * Is also fired for late-loaded plugins.
 *
 * For plugins loaded with the server, this will normally be fired right
 * after Steam_FullyLoaded, closly followed by Steam_SteamServersConnected 
 * if a successfull connection is established.
 *
 * @noreturn
 */
forward Steam_SteamServersDisconnected();

/**
 * Sets an entry in the server's list of rules. This list is used to 
 * build the response to the A2S_RULES query and is generally known as 
 * the list of public convars.
 *
 * @param key   Name of the key to set, is created if it does not already
 *              exist.
 * @param value Value of the key to set, the named key is removed if this 
 *              is blank.
 *
 * @noreturn
 */
native Steam_SetRule(String:key[], String:value[]);

/**
 * Clears the server's list of rules. This list is used to build the 
 * response to the A2S_RULES query and is generally known as the list of 
 * public convars.
 *
 * @noreturn
 */
native Steam_ClearRules();


native Steam_ForceHeartbeat();
native bool:Steam_AddMasterServer(String:serverAddress[]);
native bool:Steam_RemoveMasterServer(String:serverAddress[]);
native Steam_GetNumMasterServers();
native Steam_GetMasterServerAddress(server, String:serverAddress[], maxlength);


native Steam_RequestStats(client);
forward Steam_StatsReceived(client);
forward Steam_StatsUnloaded(client);
native Steam_GetStat(client, String:statName[]);
native Float:Steam_GetStatFloat(client, String:statName[]);
native bool:Steam_IsAchieved(client, String:achievementName[]);


native Steam_GetNumClientSubscriptions(client);
native Steam_GetClientSubscription(client, index);

native Steam_GetNumClientDLCs(client);
native Steam_GetClientDLC(client, index);

enum DLCLicenseStatus
{
	HasLicense = 0,				// User has a license for specified app
	DoesNotHaveLicense = 1,		// User does not have a license for the specified app
	NoAuth = 2,					// User has not been authenticated
}

native DLCLicenseStatus:Steam_CheckUserHasDLC(client, appid);

native Steam_GetCSteamIDForClient(client, String:steamID[], maxlength);

native bool:Steam_SetCustomSteamID(String:renderedID[]);
native bool:Steam_GetCustomSteamID(String:renderedID[], maxlength);

native Steam_RenderedIDToCSteamID(String:renderedID[], String:steamID[], maxlength);
native Steam_CSteamIDToRenderedID(String:steamID[], String:renderedID[], maxlength);

native Steam_GroupIDToCSteamID(groupID, String:steamID[], maxlength);
native Steam_CSteamIDToGroupID(String:steamID[]);

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

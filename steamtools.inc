native bool:Steam_RequestGroupStatus(String:clientAuthString, groupAccountID);
native Steam_ForceHeartbeat();
native bool:Steam_IsVACEnabled();
native Steam_ReportBotPlayers(bool:reportBotsToMaster);
forward Steam_GroupStatusResult(String:clientAuthString, bool:groupMember);
forward Steam_RestartRequested();
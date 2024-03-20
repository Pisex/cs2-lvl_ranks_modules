#include <stdio.h>
#include "lr_maps.h"
#include <sstream>
#include <stdio.h>
#include <string.h>
#include <cstring>
#include <memory>

lr_maps g_lr_maps;

ILRApi* g_pLRCore;

IMySQLConnection* g_pConnection;

IUtilsApi* g_pUtils;

IVEngineServer2* engine = nullptr;
CSchemaSystem* g_pCSchemaSystem = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;

int		g_iMapCount_Play[64],
		g_iMapCount_Kills[64],
		g_iMapCount_Deaths[64],
		g_iMapCount_RoundsOverall[64],
		g_iMapCount_Round[64][2],
		g_iMapCount_Time[64],
		g_iMapCount_BPlanted[64],
		g_iMapCount_BDefused[64],
		g_iMapCount_HRescued[64],
		g_iMapCount_HKilled[64];
bool	g_bPlayerActive[64];
char	g_sTableName[96],
		g_sCurrentNameMap[128];

int g_iLastTime;

PLUGIN_EXPOSE(lr_maps, g_lr_maps);
SH_DECL_HOOK3_void(IServerGameDLL, GameFrame, SH_NOATTRIB, 0, bool, bool, bool);
SH_DECL_HOOK5_void(IServerGameClients, ClientDisconnect, SH_NOATTRIB, 0, CPlayerSlot, ENetworkDisconnectionReason, const char *, uint64, const char *);

CGameEntitySystem* GameEntitySystem()
{
	return g_pUtils->GetCGameEntitySystem();
};

void StartupServer()
{
	g_pGameEntitySystem = GameEntitySystem();
	g_pEntitySystem = g_pUtils->GetCEntitySystem();
	g_SMAPI->Format(g_sCurrentNameMap, sizeof(g_sCurrentNameMap), "%s", g_pUtils->GetCGlobalVars()->mapname);
}

std::string ConvertSteamID(const char* usteamid) {
    std::string steamid(usteamid);

    steamid.erase(std::remove(steamid.begin(), steamid.end(), '['), steamid.end());
    steamid.erase(std::remove(steamid.begin(), steamid.end(), ']'), steamid.end());

    std::stringstream ss(steamid);
    std::string token;
    std::vector<std::string> usteamid_split;

    while (std::getline(ss, token, ':')) {
        usteamid_split.push_back(token);
    }

    std::string steamid_parts[3] = { "STEAM_1:", "", "" };

    int z = std::stoi(usteamid_split[2]);

    if (z % 2 == 0) {
        steamid_parts[1] = "0:";
    } else {
        steamid_parts[1] = "1:";
    }

    int steamacct = z / 2;
    steamid_parts[2] = std::to_string(steamacct);

    std::string result = steamid_parts[0] + steamid_parts[1] + steamid_parts[2];

    return result;
}

bool lr_maps::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();
	GET_V_IFACE_ANY(GetEngineFactory, g_pCSchemaSystem, CSchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetServerFactory, g_pSource2GameClients, IServerGameClients, SOURCE2GAMECLIENTS_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetServerFactory, g_pSource2Server, ISource2Server, SOURCE2SERVER_INTERFACE_VERSION);
	
	SH_ADD_HOOK_MEMFUNC(IServerGameClients, ClientDisconnect, g_pSource2GameClients, this, &lr_maps::OnClientDisconnect, true);
	SH_ADD_HOOK(IServerGameDLL, GameFrame, g_pSource2Server, SH_MEMBER(this, &lr_maps::GameFrame), true);
	g_SMAPI->AddListener( this, this );
	return true;
}

void lr_maps::GameFrame(bool simulating, bool bFirstTick, bool bLastTick)
{
	if(g_iLastTime == 0) g_iLastTime = std::time(0);
	else if(std::time(0) - g_iLastTime >= 1)
	{
		g_iLastTime = std::time(0);
		if(g_pLRCore->CheckCountPlayers())
		{
			for (int i = 0; i < 64; i++)
			{
				if(g_bPlayerActive[i])
				{
					g_iMapCount_Time[i]++;
				}
			}
		}
	}
}

bool lr_maps::Unload(char *error, size_t maxlen)
{
	SH_REMOVE_HOOK(IServerGameDLL, GameFrame, g_pSource2Server, SH_MEMBER(this, &lr_maps::GameFrame), true);
	SH_REMOVE_HOOK_MEMFUNC(IServerGameClients, ClientDisconnect, g_pSource2GameClients, this, &lr_maps::OnClientDisconnect, true);
	delete g_pLRCore;
	return true;
}

void OnResetLoadedHook(int iSlot, const char* SteamID)
{
	char sQuery[1024];
	g_SMAPI->Format(sQuery, sizeof(sQuery), "UPDATE `%s_maps` SET `countplays` = 0, `kills` = 0, `deaths` = 0, `rounds_overall` = 0, `rounds_ct` = 0, `rounds_t` = 0, `bomb_planted` = 0, `bomb_defused` = 0, `hostage_rescued` = 0, `hostage_killed` = 0, `playtime` = 0 WHERE `steam` = '%s';", g_sTableName, SteamID);
	g_pConnection->Query(sQuery, [](IMySQLQuery* test){});
	
	g_iMapCount_Play[iSlot] = 1;
	g_iMapCount_Kills[iSlot] = 0;
	g_iMapCount_Deaths[iSlot] = 0;
	g_iMapCount_RoundsOverall[iSlot] = 0;
	g_iMapCount_Round[iSlot][0] = 0;
	g_iMapCount_Round[iSlot][1] = 0;
	g_iMapCount_Time[iSlot] = 0;
	g_iMapCount_BPlanted[iSlot] = 0;
	g_iMapCount_BDefused[iSlot] = 0;
	g_iMapCount_HRescued[iSlot] = 0;
	g_iMapCount_HKilled[iSlot] = 0;
	g_bPlayerActive[iSlot] = true;
}

void SaveDataPlayer(int iClient)
{
	char sQuery[1024];
	g_SMAPI->Format(sQuery, sizeof(sQuery), "UPDATE `%s_maps` SET `countplays` = %d, `kills` = %d, `deaths` = %d, `rounds_overall` = %d, `rounds_ct` = %d, `rounds_t` = %d, `bomb_planted` = %d, `bomb_defused` = %d, `hostage_rescued` = %d, `hostage_killed` = %d, `playtime` = %d WHERE `steam` = '%s' AND `name_map` = '%s';", g_sTableName, g_iMapCount_Play[iClient], g_iMapCount_Kills[iClient], g_iMapCount_Deaths[iClient], g_iMapCount_RoundsOverall[iClient], g_iMapCount_Round[iClient][0], g_iMapCount_Round[iClient][1], g_iMapCount_BPlanted[iClient], g_iMapCount_BDefused[iClient], g_iMapCount_HRescued[iClient], g_iMapCount_HKilled[iClient], g_iMapCount_Time[iClient], ConvertSteamID(engine->GetPlayerNetworkIDString(iClient)).c_str(), g_sCurrentNameMap);
	g_pConnection->Query(sQuery, [](IMySQLQuery* test){});
}

void OnPlayerLoadedHook(int iSlot, const char* SteamID)
{
	char sQuery[512];
	g_bPlayerActive[iSlot] = false;

	g_SMAPI->Format(sQuery, sizeof(sQuery), "SELECT `countplays`, `kills`, `deaths`, `rounds_overall`, `rounds_ct`, `rounds_t`, `bomb_planted`, `bomb_defused`, `hostage_rescued`, `hostage_killed`, `playtime` FROM `%s_maps` WHERE `steam` = '%s' AND `name_map` = '%s';", g_sTableName, SteamID, g_sCurrentNameMap);
	g_pConnection->Query(sQuery, [iSlot, SteamID](IMySQLQuery* test){
		IMySQLResult* result = test->GetResultSet();
		if(!result->FetchRow())
		{
			char sQuery[512];
			g_iMapCount_Play[iSlot] = 1;
			g_iMapCount_Kills[iSlot] = 0;
			g_iMapCount_Deaths[iSlot] = 0;
			g_iMapCount_RoundsOverall[iSlot] = 0;
			g_iMapCount_Round[iSlot][0] = 0;
			g_iMapCount_Round[iSlot][1] = 0;
			g_iMapCount_Time[iSlot] = 0;
			g_iMapCount_BPlanted[iSlot] = 0;
			g_iMapCount_BDefused[iSlot] = 0;
			g_iMapCount_HRescued[iSlot] = 0;
			g_iMapCount_HKilled[iSlot] = 0;
			g_bPlayerActive[iSlot] = true;
			g_SMAPI->Format(sQuery, sizeof(sQuery), "INSERT INTO `%s_maps` (`steam`, `name_map`, `countplays`, `kills`, `deaths`, `rounds_overall`, `rounds_ct`, `rounds_t`, `bomb_planted`, `bomb_defused`, `hostage_rescued`, `hostage_killed`, `playtime`) VALUES ('%s', '%s', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d');", g_sTableName, SteamID, g_sCurrentNameMap, g_iMapCount_Play[iSlot], g_iMapCount_Kills[iSlot], g_iMapCount_Deaths[iSlot], g_iMapCount_RoundsOverall[iSlot], g_iMapCount_Round[iSlot][0], g_iMapCount_Round[iSlot][1], g_iMapCount_BPlanted[iSlot], g_iMapCount_BDefused[iSlot], g_iMapCount_HRescued[iSlot], g_iMapCount_HKilled[iSlot], g_iMapCount_Time[iSlot]);
			g_pConnection->Query(sQuery, [](IMySQLQuery* test){});
		}
		else
		{
			g_iMapCount_Play[iSlot] = result->GetInt(0) + 1;
			g_iMapCount_Kills[iSlot] = result->GetInt(1);
			g_iMapCount_Deaths[iSlot] = result->GetInt(2);
			g_iMapCount_RoundsOverall[iSlot] = result->GetInt(3);
			g_iMapCount_Round[iSlot][0] = result->GetInt(4);
			g_iMapCount_Round[iSlot][1] = result->GetInt(5);
			g_iMapCount_BPlanted[iSlot] = result->GetInt(6);
			g_iMapCount_BDefused[iSlot] = result->GetInt(7);
			g_iMapCount_HRescued[iSlot] = result->GetInt(8);
			g_iMapCount_HKilled[iSlot] = result->GetInt(9);
			g_iMapCount_Time[iSlot] = result->GetInt(10);
			g_bPlayerActive[iSlot] = true;
		}
	});
}

void lr_maps::OnClientDisconnect( CPlayerSlot slot, ENetworkDisconnectionReason reason, const char *pszName, uint64 xuid, const char *pszNetworkID )
{
	g_bPlayerActive[slot.Get()] = false;
	CCSPlayerController* pPlayerController = static_cast<CCSPlayerController*>(g_pEntitySystem->GetBaseEntity(static_cast<CEntityIndex>(slot.Get() + 1)));
	if (!pPlayerController || !pPlayerController->m_hPawn() || pPlayerController->m_steamID() <= 0)
		return;
	SaveDataPlayer(slot.Get());
}

void Hooks(const char* szName, IGameEvent* pEvent, bool bDontBroadcast)
{
	if(g_pLRCore->CheckCountPlayers())
	{
		switch(szName[0])
		{
			case 'p':
			{
				int iAttacker = pEvent->GetInt("attacker");
				int iClient = pEvent->GetInt("userid");
				if(iAttacker >= 0 && iAttacker <= 64 && iClient <= 64 && iClient >= 0 && iAttacker != iClient)
				{
					g_iMapCount_Kills[iAttacker]++;
					g_iMapCount_Deaths[iClient]++;
				}
				break;
			}

			case 'r':
			{
				int iWinnerTeam = pEvent->GetInt("winner");
				for (int i = 0; i < 64; i++)
				{
					if(g_bPlayerActive[i] && engine->IsClientFullyAuthenticated(i))
					{
						CCSPlayerController* pPlayerController = static_cast<CCSPlayerController*>(g_pEntitySystem->GetBaseEntity(static_cast<CEntityIndex>(i + 1)));
						if (!pPlayerController || !pPlayerController->m_hPawn() || pPlayerController->m_steamID() <= 0)
							continue;
						g_iMapCount_RoundsOverall[i]++;
						if(pPlayerController->m_iTeamNum() == iWinnerTeam)
						{
							switch(iWinnerTeam)
							{
								case 3: g_iMapCount_Round[i][0]++;
								case 2: g_iMapCount_Round[i][1]++;
							}
						}
						if(g_pLRCore->GetSettingsValue(LR_DB_SaveDataPlayer_Mode))
							SaveDataPlayer(i);
					}
				}
				break;
			}

			case 'b':
			{
				int iClient = pEvent->GetInt("userid");
				if(iClient >= 0 && iClient <= 64)
				{
					switch(szName[6])
					{
						case 'l': g_iMapCount_BPlanted[iClient]++;
						case 'e': g_iMapCount_BDefused[iClient]++;
					}
				}
				break;
			}

			case 'h':
			{
				int iClient = pEvent->GetInt("userid");
				if(iClient >= 0 && iClient <= 64)
				{
					switch(szName[8])
					{
						case 'k': g_iMapCount_HKilled[iClient]++;
						case 'r': g_iMapCount_HRescued[iClient]++;
					}
				}
				break;
			}
		}
	}
}

void OnCoreIsReadyHook()
{
	g_pConnection = g_pLRCore->GetDatabases();
	char szQuery[1024];
	g_SMAPI->Format(g_sTableName, sizeof(g_sTableName), g_pLRCore->GetTableName());
	g_SMAPI->Format(szQuery, sizeof(szQuery), "CREATE TABLE IF NOT EXISTS `%s_maps` (`steam` varchar(32) NOT NULL default '', `name_map` varchar(128) NOT NULL default '', `countplays` int NOT NULL DEFAULT 0, `kills` int NOT NULL DEFAULT 0, `deaths` int NOT NULL DEFAULT 0, `rounds_overall` int NOT NULL DEFAULT 0, `rounds_ct` int NOT NULL DEFAULT 0, `rounds_t` int NOT NULL DEFAULT 0, `bomb_planted` int NOT NULL DEFAULT 0, `bomb_defused` int NOT NULL DEFAULT 0, `hostage_rescued` int NOT NULL DEFAULT 0, `hostage_killed` int NOT NULL DEFAULT 0, `playtime` int NOT NULL DEFAULT 0, PRIMARY KEY (`steam`, `name_map`)) CHARSET = utf8 COLLATE utf8_general_ci;", g_sTableName);
	g_pConnection->Query(szQuery, [](IMySQLQuery* test){});
	g_pLRCore->HookOnPlayerLoaded(g_PLID, OnPlayerLoadedHook);
	g_pLRCore->HookOnResetPlayerStats(g_PLID, OnResetLoadedHook);
}

void lr_maps::AllPluginsLoaded()
{
	int ret;
	g_pLRCore = (ILRApi*)g_SMAPI->MetaFactory(LR_INTERFACE, &ret, NULL);
	if (ret == META_IFACE_FAILED)
	{
		char error[64];
		V_strncpy(error, "Failed to lookup lr core. Aborting", 64);
		ConColorMsg(Color(255, 0, 0, 255), "[%s] %s\n", GetLogTag(), error);
		std::string sBuffer = "meta unload "+std::to_string(g_PLID);
		engine->ServerCommand(sBuffer.c_str());
		return;
	}

	g_pUtils = (IUtilsApi*)g_SMAPI->MetaFactory(Utils_INTERFACE, &ret, NULL);
	if (ret == META_IFACE_FAILED)
	{
		char error[64];
		V_strncpy(error, "Failed to lookup menus core. Aborting", 64);
		ConColorMsg(Color(255, 0, 0, 255), "[%s] %s\n", GetLogTag(), error);
		std::string sBuffer = "meta unload "+std::to_string(g_PLID);
		engine->ServerCommand(sBuffer.c_str());
		return;
	}
	if(g_pLRCore->CoreIsLoaded()) OnCoreIsReadyHook();
	else g_pLRCore->HookOnCoreIsReady(g_PLID, OnCoreIsReadyHook);
	g_pUtils->StartupServer(g_PLID, StartupServer);
	g_pUtils->HookEvent(g_PLID, "player_death", Hooks);
	g_pUtils->HookEvent(g_PLID, "round_end", Hooks);
	g_pUtils->HookEvent(g_PLID, "bomb_planted", Hooks);
	g_pUtils->HookEvent(g_PLID, "bomb_defused", Hooks);
	g_pUtils->HookEvent(g_PLID, "hostage_killed", Hooks);
	g_pUtils->HookEvent(g_PLID, "hostage_rescued", Hooks);
}

const char *lr_maps::GetLicense()
{
	return "Public";
}

const char *lr_maps::GetVersion()
{
	return "1.0";
}

const char *lr_maps::GetDate()
{
	return __DATE__;
}

const char *lr_maps::GetLogTag()
{
	return "[LR-Maps]";
}

const char *lr_maps::GetAuthor()
{
	return "Pisex";
}

const char *lr_maps::GetDescription()
{
	return "";
}

const char *lr_maps::GetName()
{
	return "[LR] ExStats Maps";
}

const char *lr_maps::GetURL()
{
	return "https://discord.com/invite/g798xERK5Y";
}

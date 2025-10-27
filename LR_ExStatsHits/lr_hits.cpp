#include <stdio.h>
#include "lr_hits.h"
#include <sstream>

lr_hits g_lr_hits;

ILRApi* g_pLRCore;

IMySQLConnection* g_pConnection;

IUtilsApi* g_pUtils;

IVEngineServer2* engine = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;

#define HitData 11

#define HD_None -1
#define HD_DmgHealth 0
#define HD_DmgArmor 1
#define HD_HitHead 2
#define HD_HitChest 3
#define HD_HitBelly 4
#define HD_HitLeftArm 5
#define HD_HitRightArm 6
#define HD_HitLeftLeg 7
#define HD_HitRightLeg 8
#define HD_HitNeak 9
#define HD_HitAll 10

int g_iHits[64][HitData],
	g_iHitFlags[64];

char g_sTableName[32];

PLUGIN_EXPOSE(lr_hits, g_lr_hits);
SH_DECL_HOOK5_void(IServerGameClients, ClientDisconnect, SH_NOATTRIB, 0, CPlayerSlot, ENetworkDisconnectionReason, const char *, uint64, const char *);

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

bool lr_hits::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();
	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetServerFactory, g_pSource2GameClients, IServerGameClients, SOURCE2GAMECLIENTS_INTERFACE_VERSION);
	SH_ADD_HOOK_MEMFUNC(IServerGameClients, ClientDisconnect, g_pSource2GameClients, this, &lr_hits::OnClientDisconnect, true);
	g_SMAPI->AddListener( this, this );
	return true;
}

bool lr_hits::Unload(char *error, size_t maxlen)
{
	SH_REMOVE_HOOK_MEMFUNC(IServerGameClients, ClientDisconnect, g_pSource2GameClients, this, &lr_hits::OnClientDisconnect, true);
	delete g_pLRCore;
	return true;
}

void OnResetLoadedHook(int iSlot, const char* SteamID)
{
	char sQuery[512];
	g_SMAPI->Format(sQuery, sizeof(sQuery), "UPDATE `%s_hits` SET \
	`DmgHealth` = 0, \
	`DmgArmor` = 0, \
	`Head` = 0, \
	`Chest` = 0, \
	`Belly` = 0, \
	`LeftArm` = 0, \
	`RightArm` = 0, \
	`LeftLeg` = 0, \
	`RightLeg` = 0, \
	`Neak` = 0 \
WHERE \
	`SteamID` = '%s'", g_sTableName, SteamID);
	g_pConnection->Query(sQuery, [](ISQLQuery* test){});
}

void OnPlayerLoadedHook(int iSlot, const char* SteamID)
{
	if(iSlot)
	{
		for(int i = 0; i != HitData;)
		{
			g_iHits[iSlot][i++] = 0;
		}
	}
	char sQuery[1024];

	g_SMAPI->Format(sQuery, sizeof(sQuery), "SELECT \
	`DmgHealth`, \
	`DmgArmor`, \
	`Head`, \
	`Chest`, \
	`Belly`, \
	`LeftArm`, \
	`RightArm`, \
	`LeftLeg`, \
	`RightLeg`, \
	`Neak` \
FROM \
	`%s_hits` \
WHERE \
	`SteamID` = '%s';", g_sTableName, SteamID);
	g_pConnection->Query(sQuery, [iSlot, SteamID](ISQLQuery* test){
		ISQLResult* result = test->GetResultSet();
		bool bLoadData = true;
		if(!result->FetchRow())
		{
			char sQuery[128];
			g_SMAPI->Format(sQuery, sizeof(sQuery), "INSERT INTO `%s_hits` (`SteamID`) VALUES ('%s');", g_sTableName, SteamID);
			g_pConnection->Query(sQuery, [](ISQLQuery* test){});
			bLoadData = false;
		}

		for(int i = g_iHits[iSlot][HD_HitAll] = 0; i != HD_HitAll; i++)
		{
			g_iHits[iSlot][i] = bLoadData ? result->GetInt(i) : 0;

			if(i > 1)
			{
				g_iHits[iSlot][HD_HitAll] += g_iHits[iSlot][i];
			}
		}
	});
}

void SaveData(int iClient)
{
	int iFlags = g_iHitFlags[iClient];

	if(iFlags)
	{
		static const char* sHitColumnName[] = {"Head", "Chest", "Belly", "LeftArm", "RightArm", "LeftLeg", "RightLeg", "Neak"};

		char sQuery[256],
			 sColumns[128];

		for(int Type = HD_HitHead; Type != HitData; Type++)
		{
			if(iFlags & (1 << Type))
			{
				g_SMAPI->Format(sColumns, sizeof(sColumns), "%s`%s` = %d, ", sColumns, sHitColumnName[Type - 2], g_iHits[iClient][Type]);
			}
		}

		sColumns[strlen(sColumns) - 2] = '\0';
		g_SMAPI->Format(sQuery, sizeof(sQuery), "UPDATE `%s_hits` SET \
		`DmgHealth` = %d, \
		`DmgArmor` = %d, \
		%s \
	WHERE \
		`SteamID` = '%s'", g_sTableName, g_iHits[iClient][HD_DmgHealth], g_iHits[iClient][HD_DmgArmor], sColumns, ConvertSteamID(engine->GetPlayerNetworkIDString(iClient)).c_str());
		g_pConnection->Query(sQuery, [](ISQLQuery* test){});

		g_iHitFlags[iClient] = 0;
	}
}

void lr_hits::OnClientDisconnect( CPlayerSlot slot, ENetworkDisconnectionReason reason, const char *pszName, uint64 xuid, const char *pszNetworkID )
{
	if(!g_pLRCore->GetSettingsValue(LR_DB_SaveDataPlayer_Mode))
		SaveData(slot.Get());
}

void OnPlayerHurt(const char* szName, IGameEvent* pEvent, bool bDontBroadcast)
{
	int iAttacker = pEvent->GetInt("attacker");
	if(engine->IsClientFullyAuthenticated(iAttacker) && g_pLRCore->CheckCountPlayers())
	{
		g_iHits[iAttacker][HD_DmgHealth] += pEvent->GetInt("dmg_health");
		g_iHits[iAttacker][HD_DmgArmor] += pEvent->GetInt("dmg_armor");

		int iHB = pEvent->GetInt("hitgroup") + 1;

		if(1 < iHB && iHB < 11)
		{
			g_iHits[iAttacker][iHB]++;
			g_iHits[iAttacker][HD_HitAll]++;
			g_iHitFlags[iAttacker] |= (1 << iHB);
		}
		if(g_pLRCore->GetSettingsValue(LR_DB_SaveDataPlayer_Mode))
			SaveData(iAttacker);
	}
}

void OnCoreIsReadyHook()
{
	g_pConnection = g_pLRCore->GetDatabases();
	char szQuery[512];
	g_SMAPI->Format(g_sTableName, sizeof(g_sTableName), g_pLRCore->GetTableName());
	g_SMAPI->Format(szQuery, sizeof(szQuery), "CREATE TABLE IF NOT EXISTS `%s_hits` \
(\
	`SteamID` varchar(32) NOT NULL PRIMARY KEY DEFAULT '', \
	`DmgHealth` int NOT NULL DEFAULT 0, \
	`DmgArmor` int NOT NULL DEFAULT 0, \
	`Head` int NOT NULL DEFAULT 0, \
	`Chest` int NOT NULL DEFAULT 0, \
	`Belly` int NOT NULL DEFAULT 0, \
	`LeftArm` int NOT NULL DEFAULT 0, \
	`RightArm` int NOT NULL DEFAULT 0, \
	`LeftLeg` int NOT NULL DEFAULT 0, \
	`RightLeg` int NOT NULL DEFAULT 0, \
	`Neak` int NOT NULL DEFAULT 0\
) CHARSET = utf8 COLLATE utf8_general_ci;", g_sTableName);
	g_pConnection->Query(szQuery, [](ISQLQuery* test){});
	g_pLRCore->HookOnPlayerLoaded(g_PLID, OnPlayerLoadedHook);
	g_pLRCore->HookOnResetPlayerStats(g_PLID, OnResetLoadedHook);
}

void lr_hits::AllPluginsLoaded()
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
	g_pUtils->HookEvent(g_PLID, "player_hurt", OnPlayerHurt);
}

const char *lr_hits::GetLicense()
{
	return "Public";
}

const char *lr_hits::GetVersion()
{
	return "1.0";
}

const char *lr_hits::GetDate()
{
	return __DATE__;
}

const char *lr_hits::GetLogTag()
{
	return "[LR-Hits]";
}

const char *lr_hits::GetAuthor()
{
	return "Pisex";
}

const char *lr_hits::GetDescription()
{
	return "";
}

const char *lr_hits::GetName()
{
	return "[LR] ExStats Hits";
}

const char *lr_hits::GetURL()
{
	return "https://discord.com/invite/g798xERK5Y";
}

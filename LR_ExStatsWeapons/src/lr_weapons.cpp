#include <stdio.h>
#include "lr_weapons.h"
#include <sstream>

lr_weapons g_lr_weapons;

ILRApi* g_pLRCore;

IMySQLConnection* g_pConnection;

IUtilsApi* g_pUtils;

IVEngineServer2* engine = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;

struct WeaponsData
{
	float 		fCoefficient;
	const char* sName;
};

std::map<std::string, WeaponsData> g_hWeapons;
char    g_sTableName[32];
int		g_iCountWeapons,
		g_iWeaponsStats[64][96];
bool	g_bWeaponsCoeffActive,
		g_bWeaponsNew[64][96];
char	g_sWeaponsClassName[96][64];

PLUGIN_EXPOSE(lr_weapons, g_lr_weapons);
SH_DECL_HOOK5_void(IServerGameClients, ClientDisconnect, SH_NOATTRIB, 0, CPlayerSlot, ENetworkDisconnectionReason, const char *, uint64, const char *);

CGameEntitySystem* GameEntitySystem()
{
	return g_pUtils->GetCGameEntitySystem();
};

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

void LoadSettings()
{
	const char *pszPath = "addons/configs/levels_ranks/exstats_weapons.ini";
	char sBuffer[512];

	KeyValues* hKv = new KeyValues("LR_ExStatsWeapons");

	if (!hKv->LoadFromFile(g_pFullFileSystem, pszPath))
	{
		Warning("Failed to load %s\n", pszPath);
		return;
	}

	g_bWeaponsCoeffActive = hKv->GetInt("weapon_coefficient", 1);
	g_iCountWeapons = 0;

	FOR_EACH_SUBKEY(hKv, pKey)
	{
		g_SMAPI->Format(g_sWeaponsClassName[g_iCountWeapons], sizeof(g_sWeaponsClassName[g_iCountWeapons]), pKey->GetName());
		WeaponsData hData;
		hData.sName = pKey->GetString("name");
		hData.fCoefficient = pKey->GetFloat("coefficient", 1.0);
		g_hWeapons[g_sWeaponsClassName[g_iCountWeapons++]] = hData;
	}
}

void SaveDataPlayer(int iClient, int weapon = -1)
{
	char sQuery[512];
	if(weapon != -1)
	{
		if(g_bWeaponsNew[iClient][weapon])
		{
			g_SMAPI->Format(sQuery, sizeof(sQuery), "INSERT INTO `%s_weapons` (`steam`, `classname`, `kills`) VALUES ('%s', '%s', '%d');", g_sTableName, ConvertSteamID(engine->GetPlayerNetworkIDString(iClient)).c_str(), g_sWeaponsClassName[weapon], g_iWeaponsStats[iClient][weapon]);
			g_bWeaponsNew[iClient][weapon] = false;
		}
		else
		{
			g_SMAPI->Format(sQuery, sizeof(sQuery), "UPDATE `%s_weapons` SET `kills` = %d WHERE `steam` = '%s' AND `classname` = '%s';", g_sTableName, g_iWeaponsStats[iClient][weapon], ConvertSteamID(engine->GetPlayerNetworkIDString(iClient)).c_str(), g_sWeaponsClassName[weapon]);
		}

		g_pConnection->Query(sQuery, [](ISQLQuery* test){});
	}
	else
	{
		for(int i; i != g_iCountWeapons; i++)
		{
			if(g_iWeaponsStats[iClient][i])
			{
				if(g_bWeaponsNew[iClient][i])
				{
					g_SMAPI->Format(sQuery, sizeof(sQuery), "INSERT INTO `%s_weapons` (`steam`, `classname`, `kills`) VALUES ('%s', '%s', '%d');", g_sTableName, ConvertSteamID(engine->GetPlayerNetworkIDString(iClient)).c_str(), g_sWeaponsClassName[i], g_iWeaponsStats[iClient][i]);
					g_bWeaponsNew[iClient][i] = false;
				}
				else
				{
					g_SMAPI->Format(sQuery, sizeof(sQuery), "UPDATE `%s_weapons` SET `kills` = %d WHERE `steam` = '%s' AND `classname` = '%s';", g_sTableName, g_iWeaponsStats[iClient][i], ConvertSteamID(engine->GetPlayerNetworkIDString(iClient)).c_str(), g_sWeaponsClassName[i]);
				}

				g_pConnection->Query(sQuery, [](ISQLQuery* test){});
			}
		}
	}
}

bool lr_weapons::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetFileSystemFactory, g_pFullFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetServerFactory, g_pSource2GameClients, IServerGameClients, SOURCE2GAMECLIENTS_INTERFACE_VERSION);
	SH_ADD_HOOK_MEMFUNC(IServerGameClients, ClientDisconnect, g_pSource2GameClients, this, &lr_weapons::OnClientDisconnect, true);
	g_SMAPI->AddListener( this, this );
	
	return true;
}

void lr_weapons::OnClientDisconnect( CPlayerSlot slot, ENetworkDisconnectionReason reason, const char *pszName, uint64 xuid, const char *pszNetworkID )
{
	CCSPlayerController* pPlayerController = CCSPlayerController::FromSlot(slot.Get());
	if (!pPlayerController || !pPlayerController->m_hPawn() || pPlayerController->m_steamID() <= 0)
		return;
	if(!g_pLRCore->GetSettingsValue(LR_DB_SaveDataPlayer_Mode))
		SaveDataPlayer(slot.Get());
}

bool lr_weapons::Unload(char *error, size_t maxlen)
{
	SH_REMOVE_HOOK_MEMFUNC(IServerGameClients, ClientDisconnect, g_pSource2GameClients, this, &lr_weapons::OnClientDisconnect, true);
	delete g_pLRCore;
	return true;
}

void OnResetLoadedHook(int iSlot, const char* SteamID)
{
	char sQuery[256];
	g_SMAPI->Format(sQuery, sizeof(sQuery), "UPDATE `%s_weapons` SET `kills` = 0 WHERE `steam` = '%s';", g_sTableName, SteamID);
	g_pConnection->Query(sQuery, [iSlot](ISQLQuery* test){
		for(int i = 0; i != g_iCountWeapons; i++)
		{
			g_iWeaponsStats[iSlot][i] = 0;
		}
	});
}

void OnPlayerLoadedHook(int iSlot, const char* SteamID)
{
	char sQuery[1024];
	for(int i; i != g_iCountWeapons; i++)
	{
		g_SMAPI->Format(sQuery, sizeof(sQuery), "SELECT `kills` FROM `%s_weapons` WHERE `classname` = '%s' AND `steam` = '%s'", g_sTableName, g_sWeaponsClassName[i], SteamID);
		g_pConnection->Query(sQuery, [iSlot, i](ISQLQuery* test){
			ISQLResult* result = test->GetResultSet();
			if(result->FetchRow())
			{
				g_bWeaponsNew[iSlot][i] = false;
				g_iWeaponsStats[iSlot][i] = result->GetInt(0);
			}
			else
			{
				g_bWeaponsNew[iSlot][i] = true;
				g_iWeaponsStats[iSlot][i] = 0;
			}
		});
	}
}

int RoundToNearest(float fValue) {
    return static_cast<int>(round(fValue));
}

void OnPlayerKilledPreHook(IGameEvent* pEvent, int &iExpCaused, int iExpVictim, int iExpAttacker)
{
	if(g_pLRCore->CheckCountPlayers())
	{
		int iAttacker = pEvent->GetInt("attacker");
		CCSPlayerController* pAttacker = (CCSPlayerController*)pEvent->GetPlayerController("attacker");
		if(pAttacker && pAttacker->GetPlayerPawn())
		{
			const char* sBuffer;
			char sClassname[64];
			sBuffer = pEvent->GetString("weapon");
			g_SMAPI->Format(sClassname, sizeof(sClassname), "weapon_%s", sBuffer);
			
			if(sBuffer[0] == 'k' || !strcmp(sBuffer, "bayonet"))
			{
				g_SMAPI->Format(sClassname, sizeof(sClassname), "weapon_knife");
			}

			for(int i; i != g_iCountWeapons; i++)
			{
				if(!strcmp(sClassname, g_sWeaponsClassName[i]))
				{
					if(g_bWeaponsCoeffActive)
					{
						WeaponsData hData = g_hWeapons[sClassname];
						iExpCaused = RoundToNearest(iExpCaused * hData.fCoefficient);
					}
					g_iWeaponsStats[iAttacker][i]++;
					if(g_pLRCore->GetSettingsValue(LR_DB_SaveDataPlayer_Mode))
						SaveDataPlayer(iAttacker, i);
					break;
				}
			}
		}
	}
}

void OnDatabaseCleanupHook(int iType)
{
	if(iType == 0 || iType == 2)
	{
		char sQuery[512];
		g_SMAPI->Format(sQuery, sizeof(sQuery), "TRUNCATE TABLE `%s_weapons`;", g_sTableName);
		g_pConnection->Query(sQuery, [](ISQLQuery* test){});
		for(int i = 0; i < 64; i++)
		{
			CCSPlayerController* pPlayer = CCSPlayerController::FromSlot(i);
			if(pPlayer && pPlayer->GetPlayerPawn() && pPlayer->m_steamID() > 0)
			{
				OnResetLoadedHook(i, ConvertSteamID(engine->GetPlayerNetworkIDString(i)).c_str());
			}
		}
	}
}

void OnCoreIsReadyHook()
{
	g_pConnection = g_pLRCore->GetDatabases();
	char szQuery[512];
	g_SMAPI->Format(g_sTableName, sizeof(g_sTableName), g_pLRCore->GetTableName());
	g_SMAPI->Format(szQuery, sizeof(szQuery), "CREATE TABLE IF NOT EXISTS `%s_weapons` \
(\
	`steam` varchar(32) NOT NULL default '', \
	`classname` varchar(64) NOT NULL default '', \
	`kills` int NOT NULL default 0, \
	PRIMARY KEY (`steam`, `classname`)\
) CHARSET = utf8 COLLATE utf8_general_ci;", g_sTableName);
	g_pConnection->Query(szQuery, [](ISQLQuery* test){});
	
	g_pLRCore->HookOnPlayerLoaded(g_PLID, OnPlayerLoadedHook);
	g_pLRCore->HookOnResetPlayerStats(g_PLID, OnResetLoadedHook);
	g_pLRCore->HookOnPlayerKilledPre(g_PLID, OnPlayerKilledPreHook);
	g_pLRCore->HookOnDatabaseCleanup(g_PLID, OnDatabaseCleanupHook);
	LoadSettings();
}

void lr_weapons::AllPluginsLoaded()
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
	g_pUtils->StartupServer(g_PLID, [](){
		g_pGameEntitySystem = GameEntitySystem();
		g_pEntitySystem = g_pUtils->GetCEntitySystem();
	});
}

const char *lr_weapons::GetLicense()
{
	return "Public";
}

const char *lr_weapons::GetVersion()
{
	return "1.0";
}

const char *lr_weapons::GetDate()
{
	return __DATE__;
}

const char *lr_weapons::GetLogTag()
{
	return "[LR-Weapons]";
}

const char *lr_weapons::GetAuthor()
{
	return "Pisex";
}

const char *lr_weapons::GetDescription()
{
	return "";
}

const char *lr_weapons::GetName()
{
	return "[LR] ExStats Weapons";
}

const char *lr_weapons::GetURL()
{
	return "https://discord.com/invite/g798xERK5Y";
}

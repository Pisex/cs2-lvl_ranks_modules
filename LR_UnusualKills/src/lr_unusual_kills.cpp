#include <stdio.h>
#include "lr_unusual_kills.h"
#include <sstream>

lr_unusual_kills g_lr_unusual_kills;

ILRApi* g_pLRCore;

IMySQLConnection* g_pConnection;

IUtilsApi* g_pUtils;

IVEngineServer2* engine = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;

std::map<std::string, std::string> g_vecPhrases;

#define MAX_UKTYPES 9
#define UnusualKill_None 0
#define UnusualKill_OpenFrag (1 << 0)
#define UnusualKill_Penetrated (1 << 1)
#define UnusualKill_NoScope (1 << 2)
#define UnusualKill_Run (1 << 3)
#define UnusualKill_Jump (1 << 4)
#define UnusualKill_Flash (1 << 5)
#define UnusualKill_Smoke (1 << 6)
#define UnusualKill_LastClip (1 << 7)
#define UnusualKill_Distance (1 << 8)

#define SQL_CreateData "INSERT INTO `%s_unusualkills` (`SteamID`) VALUES ('%s');"

#define SQL_LoadData \
"SELECT \
	`OP`, \
	`Penetrated`, \
	`NoScope`, \
	`Run`, \
	`Jump`, \
	`Flash`, \
	`Smoke`, \
	`LastClip`, \
	`Distance` \
FROM `%s_unusualkills` WHERE `SteamID` = '%s';"

#define SQL_UpdateResetData \
"UPDATE `%s_unusualkills` SET \
	`OP` = 0, \
	`Penetrated` = 0, \
	`NoScope` = 0, \
	`Run` = 0, \
	`Jump` = 0, \
	`Flash` = 0, \
	`Smoke` = 0, \
	`LastClip` = 0, \
	`Distance` = 0 \
WHERE \
	`SteamID` = '%s';"

#define SQL_PrintTop \
"SELECT \
	`name`, \
	`%s` \
FROM \
	`%s`, \
	`%s_unusualkills` \
WHERE \
	`steam` = `SteamID` AND \
	`lastconnect` \
ORDER BY \
	`%s` \
DESC LIMIT 10;"

bool    g_bMessages,
        g_bOPKill;

int     g_iAccountID[64],
        g_iExp[MAX_UKTYPES],
        g_iExpMode,
        g_iWhirlInterval = 2,
        g_iUK[64][MAX_UKTYPES],
        m_iClip1,
        m_hActiveWeapon,
        m_vecVelocity;

float   g_flDistance;
float 	g_flSpeed;

char    g_sTableName[32];

std::vector<std::string> g_hProhibitedWeapons;

static const char* g_sNameUK[] = {"OP", "Penetrated", "NoScope", "Run", "Jump", "Flash", "Smoke", "LastClip", "Distance"};

PLUGIN_EXPOSE(lr_unusual_kills, g_lr_unusual_kills);

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

void SplitString(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while (getline(ss, item, delim)) {
        elems.push_back(item);
    }
}

void LoadSettings()
{
	static int  iUKSymbolTypes[] = {127, 127, 127, 8, 127, 5, 127, 127, 127, 4, 127, 7, 127, 2, 0, 1, 127, 3, 6, 127, 127, 127};

	const char *pszPath = "addons/configs/levels_ranks/unusual_kills.ini";
	char sBuffer[512];

	KeyValues* hKv = new KeyValues("LR_UnusualKills");

	if (!hKv->LoadFromFile(g_pFullFileSystem, pszPath))
	{
		Warning("Failed to load %s\n", pszPath);
		return;
	}

	g_bMessages = g_pLRCore->GetSettingsValue(LR_ShowUsualMessage) == 1;

	g_SMAPI->Format(sBuffer, sizeof(sBuffer), hKv->GetString("ProhibitedWeapons"));
	SplitString(sBuffer, ',', g_hProhibitedWeapons);
	g_iExpMode = hKv->GetInt("Exp_Mode", 1);

	KeyValues* pKVTypeKills = hKv->FindKey("TypeKills", false);
	FOR_EACH_SUBKEY(pKVTypeKills, pKey)
	{
		const char *pszName = pKey->GetName();
		int iUKType = iUKSymbolTypes[(pszName[0] | 32) - 97];
		switch(iUKType)
		{
			case 127:
			{
				Msg("%s: \"LR_UnusualKills\" -> \"Settings\" -> \"TypeKills\" -> \"%s\" - invalid selection\n", pszPath, sBuffer);
				break;
			}

			case 8:
			{
				g_flDistance = pKey->GetFloat("min_distance", 0.0);
				break;
			}

			case 3:
			{
				g_flSpeed = pKey->GetFloat("min_speed", 100.0);
				break;
			}
		}
		g_iExp[iUKType] = g_iExpMode > 0 ? pKey->GetInt("exp") : 0;
	}
}

bool lr_unusual_kills::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetFileSystemFactory, g_pFullFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetServerFactory, g_pSource2GameClients, IServerGameClients, SOURCE2GAMECLIENTS_INTERFACE_VERSION);
	g_SMAPI->AddListener( this, this );
	
	return true;
}

bool lr_unusual_kills::Unload(char *error, size_t maxlen)
{
	delete g_pLRCore;
	return true;
}

void OnResetLoadedHook(int iSlot, const char* SteamID)
{
	char sQuery[512];
	for(int i = 0; i != MAX_UKTYPES;)
	{
		g_iUK[iSlot][i++] = 0;
	}

	g_SMAPI->Format(sQuery, sizeof(sQuery), SQL_UpdateResetData, g_sTableName, SteamID);
	g_pConnection->Query(sQuery, [](ISQLQuery* test){});
}

void OnPlayerLoadedHook(int iSlot, const char* SteamID)
{
	char sQuery[1024];
	g_SMAPI->Format(sQuery, sizeof(sQuery), SQL_LoadData, g_sTableName, SteamID);
	g_pConnection->Query(sQuery, [iSlot, SteamID](ISQLQuery* test){
		ISQLResult* result = test->GetResultSet();
		if(result->FetchRow())
		{
			for(int i = 0; i != MAX_UKTYPES;)
			{
				g_iUK[iSlot][i++] = result->GetInt(i);
			}
		}
		else 
		{
			char sQuery[512];
			g_SMAPI->Format(sQuery, sizeof(sQuery), SQL_CreateData, g_sTableName, SteamID);
			g_pConnection->Query(sQuery, [](ISQLQuery* test){});

			for(int i = 0; i != MAX_UKTYPES;)
			{
				g_iUK[iSlot][i++] = 0;
			}
		}
	});
}

void OnRoundStart(const char* szName, IGameEvent* pEvent, bool bDontBroadcast)
{
	g_bOPKill = false;
}

void OnPlayerKilledPreHook(IGameEvent* pEvent, int &iExpCaused, int iExpVictim, int iExpAttacker)
{
	if(g_pLRCore->CheckCountPlayers())
	{
		const char* szWeapon = pEvent->GetString("weapon");
		bool isWeaponProhibited = std::find(g_hProhibitedWeapons.begin(), g_hProhibitedWeapons.end(), std::string(szWeapon)) != g_hProhibitedWeapons.end();
		if(!isWeaponProhibited && szWeapon[0] != 'k' && szWeapon[2] != 'y')
		{
			int iAttacker = pEvent->GetInt("attacker");
			int iUKFlags = UnusualKill_None;
			CCSPlayerController* pAttacker = (CCSPlayerController*)pEvent->GetPlayerController("attacker");
			if(pAttacker && pAttacker->GetPlayerPawn() && pAttacker->m_steamID() > 0)
			{
				CCSPlayer_WeaponServices* pWeaponServices = pAttacker->GetPlayerPawn()->m_pWeaponServices();
				if(pWeaponServices)
				{
					CBasePlayerWeapon* m_hActiveWeapon = pWeaponServices->m_hActiveWeapon();
					Vector vecVelocity = pAttacker->GetPlayerPawn()->m_vecAbsVelocity();
					if(!g_bOPKill)
					{
						iUKFlags |= UnusualKill_OpenFrag;
						g_bOPKill = true;
					}

					if(pEvent->GetInt("penetrated"))
					{
						iUKFlags |= UnusualKill_Penetrated;
					}

					if(pEvent->GetInt("noscope"))
					{
						iUKFlags |= UnusualKill_NoScope;
					}
					
					if(pEvent->GetInt("attackerinair"))
					{
						iUKFlags |= UnusualKill_Jump;
					}

					if(vecVelocity.Length() >= g_flSpeed)
					{
						iUKFlags |= UnusualKill_Run;
					}
					
					if(pEvent->GetFloat("distance") > g_flDistance)
					{
						iUKFlags |= UnusualKill_Distance;
					}

					if(pEvent->GetInt("attackerblind"))
					{
						iUKFlags |= UnusualKill_Flash;
					}

					if(pEvent->GetInt("thrusmoke"))
					{
						iUKFlags |= UnusualKill_Smoke;
					}

					if(m_hActiveWeapon && m_hActiveWeapon->m_iClip1() == 1)
					{
						iUKFlags |= UnusualKill_LastClip;
					}
					if(iUKFlags)
					{
						char sBuffer[64],
							sColumns[MAX_UKTYPES * 16] = "",
							sQuery[512];

						for(int iType = 0; iType != MAX_UKTYPES; iType++)
						{
							if(iUKFlags & (1 << iType))
							{
								g_SMAPI->Format(sBuffer, sizeof(sBuffer), "`%s` = %d", g_sNameUK[iType], ++g_iUK[iAttacker][iType]);
								strncat(sColumns, sBuffer, sizeof(sColumns) - strlen(sColumns) - 1);
								strncat(sColumns, ", ", sizeof(sColumns) - strlen(sColumns) - 1);
								if(g_iExpMode == 1)
								{
									if(g_bMessages && g_iExp[iType] && g_pLRCore->ChangeClientValue(iAttacker, g_iExp[iType]))
									{
										g_SMAPI->Format(sBuffer, sizeof(sBuffer), g_iExp[iType] > 0 ? "+%d" : "%d", g_iExp[iType]);
										g_pLRCore->PrintToChat(iAttacker, g_vecPhrases[g_sNameUK[iType]].c_str(), g_pLRCore->GetClientInfo(iAttacker, ST_EXP), sBuffer);
									}
								}
								else
								{
									iExpCaused = iExpCaused+g_iExp[iType];
								}
							}
						}

						if(sColumns[0])
						{
							sColumns[strlen(sColumns) - 2] = '\0';
    						g_SMAPI->Format(sQuery, sizeof(sQuery), "UPDATE `%s_unusualkills` SET %s WHERE `SteamID` = '%s';", g_sTableName, sColumns, ConvertSteamID(engine->GetPlayerNetworkIDString(iAttacker)).c_str());
							g_pConnection->Query(sQuery, [](ISQLQuery* test){});
						}
					}
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
		g_SMAPI->Format(sQuery, sizeof(sQuery), "TRUNCATE TABLE `%s_unusualkills`;", g_sTableName);
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
	g_SMAPI->Format(szQuery, sizeof(szQuery), "CREATE TABLE IF NOT EXISTS `%s_unusualkills` \
(\
	`SteamID` varchar(22) PRIMARY KEY, \
	`OP` int NOT NULL DEFAULT 0, \
	`Penetrated` int NOT NULL DEFAULT 0, \
	`NoScope` int NOT NULL DEFAULT 0, \
	`Run` int NOT NULL DEFAULT 0, \
	`Jump` int NOT NULL DEFAULT 0, \
	`Flash` int NOT NULL DEFAULT 0, \
	`Smoke` int NOT NULL DEFAULT 0, \
	`LastClip` int NOT NULL DEFAULT 0, \
	`Distance` int NOT NULL DEFAULT 0 \
) CHARSET = utf8 COLLATE utf8_general_ci;", g_sTableName);
	g_pConnection->Query(szQuery, [](ISQLQuery* test){});
	g_pLRCore->HookOnPlayerLoaded(g_PLID, OnPlayerLoadedHook);
	g_pLRCore->HookOnResetPlayerStats(g_PLID, OnResetLoadedHook);
	g_pLRCore->HookOnPlayerKilledPre(g_PLID, OnPlayerKilledPreHook);
	g_pLRCore->HookOnDatabaseCleanup(g_PLID, OnDatabaseCleanupHook);
	LoadSettings();

}

void lr_unusual_kills::AllPluginsLoaded()
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
	g_pUtils->HookEvent(g_PLID, "round_start", OnRoundStart);
	g_pUtils->StartupServer(g_PLID, [](){
		g_pGameEntitySystem = GameEntitySystem();
		g_pEntitySystem = g_pUtils->GetCEntitySystem();
	});

	{
		KeyValues::AutoDelete g_kvPhrases("Phrases");
		const char *pszPath = "addons/translations/lr_unusualkills.phrases.txt";

		if (!g_kvPhrases->LoadFromFile(g_pFullFileSystem, pszPath))
		{
			Warning("Failed to load %s\n", pszPath);
			return;
		}

		const char* g_pszLanguage = g_pUtils->GetLanguage();
		for (KeyValues *pKey = g_kvPhrases->GetFirstTrueSubKey(); pKey; pKey = pKey->GetNextTrueSubKey())
			g_vecPhrases[std::string(pKey->GetName())] = std::string(pKey->GetString(g_pszLanguage));
	}
}

const char *lr_unusual_kills::GetLicense()
{
	return "Public";
}

const char *lr_unusual_kills::GetVersion()
{
	return "1.0";
}

const char *lr_unusual_kills::GetDate()
{
	return __DATE__;
}

const char *lr_unusual_kills::GetLogTag()
{
	return "[LR-UK]";
}

const char *lr_unusual_kills::GetAuthor()
{
	return "Pisex";
}

const char *lr_unusual_kills::GetDescription()
{
	return "";
}

const char *lr_unusual_kills::GetName()
{
	return "[LR] Unusual Kills";
}

const char *lr_unusual_kills::GetURL()
{
	return "https://discord.com/invite/g798xERK5Y";
}
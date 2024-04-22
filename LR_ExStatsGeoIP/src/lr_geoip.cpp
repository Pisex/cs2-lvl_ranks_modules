#include <stdio.h>
#include "lr_geoip.h"
#include <maxminddb.h>

lr_geoip g_lr_geoip;

ILRApi* g_pLRCore;

IMySQLConnection* g_pConnection;

IUtilsApi* g_pUtils;

IVEngineServer2* engine = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;

char g_sTableName[32];

PLUGIN_EXPOSE(lr_geoip, g_lr_geoip);
bool lr_geoip::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();
	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
	g_SMAPI->AddListener( this, this );
	return true;
}

bool lr_geoip::Unload(char *error, size_t maxlen)
{
	delete g_pLRCore;
	return true;
}

void OnPlayerLoadedHook(int iSlot, const char* SteamID)
{
	char sQuery[1024], sCity[45], sRegion[45], sCountry[45], sCountryCode[3];
	auto playerNetInfo = engine->GetPlayerNetInfo(iSlot);
	if(playerNetInfo == nullptr) {
		return;
	}
	auto sIp2 = std::string(playerNetInfo->GetAddress());
	auto sIp = sIp2.substr(0, sIp2.find(":"));

	MMDB_s mmdb;
	char szPath[256];
	int iSize = g_SMAPI->PathFormat(szPath, sizeof(szPath), "%s/addons/configs/geoip/GeoLite2-City.mmdb", g_SMAPI->GetBaseDir());
    int status = MMDB_open(szPath, MMDB_MODE_MMAP, &mmdb);

    if (status != MMDB_SUCCESS) {
        Msg("Error opening the database.\n");
		return;
    }

    int gai_error, mmdb_error;
    MMDB_lookup_result_s result = MMDB_lookup_string(&mmdb, sIp.c_str(), &gai_error, &mmdb_error);

    if (gai_error || mmdb_error || !result.found_entry) {
        Msg("Error while looking up IP address.\n");
        MMDB_close(&mmdb);
        return;
    }

    MMDB_entry_data_s entry_data;
    status = MMDB_get_value(&result.entry, &entry_data, "country", "names", g_pUtils->GetLanguage(), NULL);
    if(status == MMDB_SUCCESS && entry_data.type == MMDB_DATA_TYPE_UTF8_STRING)
		g_SMAPI->Format(sCountry, entry_data.data_size+1, entry_data.utf8_string);

    status = MMDB_get_value(&result.entry, &entry_data, "country", "iso_code", NULL);
    if(status == MMDB_SUCCESS && entry_data.type == MMDB_DATA_TYPE_UTF8_STRING)
		g_SMAPI->Format(sCountryCode, entry_data.data_size+1, entry_data.utf8_string);

    status = MMDB_get_value(&result.entry, &entry_data, "city", "names", g_pUtils->GetLanguage(), NULL);
    if(status == MMDB_SUCCESS && entry_data.type == MMDB_DATA_TYPE_UTF8_STRING)
		g_SMAPI->Format(sCity, entry_data.data_size+1, entry_data.utf8_string);

    status = MMDB_get_value(&result.entry, &entry_data, "subdivisions", "0", "names", g_pUtils->GetLanguage(), NULL);
    if(status == MMDB_SUCCESS && entry_data.type == MMDB_DATA_TYPE_UTF8_STRING)
		g_SMAPI->Format(sRegion, entry_data.data_size+1, entry_data.utf8_string);

    MMDB_close(&mmdb);

	g_SMAPI->Format(sQuery, sizeof(sQuery), "INSERT IGNORE INTO `%s_geoip` SET `steam` = '%s', `clientip` = '%s', `country` = '%s', `region` = '%s', `city` = '%s', `country_code` = '%s' ON DUPLICATE KEY UPDATE `clientip` = '%s', `country` = '%s', `region` = '%s', `city` = '%s', `country_code` = '%s';", g_sTableName, SteamID, sIp.c_str(), sCountry, sRegion, sCity, sCountry, sIp.c_str(), sCountry, sRegion, sCity, sCountryCode);
	g_pConnection->Query(sQuery, [](ISQLQuery* test){});
}

void OnCoreIsReadyHook()
{
	g_pConnection = g_pLRCore->GetDatabases();
	char szQuery[512];
	g_SMAPI->Format(g_sTableName, sizeof(g_sTableName), g_pLRCore->GetTableName());
	g_SMAPI->Format(szQuery, sizeof(szQuery), "CREATE TABLE IF NOT EXISTS `%s_geoip` (`steam` varchar(32) NOT NULL default '' PRIMARY KEY, `clientip` varchar(16) NOT NULL default '', `country` varchar(48) NOT NULL default '', `region` varchar(48) NOT NULL default '', `city` varchar(48) NOT NULL default '', `country_code` varchar(4) NOT NULL default '') CHARSET=utf8 COLLATE utf8_general_ci", g_sTableName);
	g_pConnection->Query(szQuery, [](ISQLQuery* test){});
	g_pLRCore->HookOnPlayerLoaded(g_PLID, OnPlayerLoadedHook);
}

void lr_geoip::AllPluginsLoaded()
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
}

const char *lr_geoip::GetLicense()
{
	return "Public";
}

const char *lr_geoip::GetVersion()
{
	return "1.0";
}

const char *lr_geoip::GetDate()
{
	return __DATE__;
}

const char *lr_geoip::GetLogTag()
{
	return "[LR-GEOIP]";
}

const char *lr_geoip::GetAuthor()
{
	return "Pisex";
}

const char *lr_geoip::GetDescription()
{
	return "";
}

const char *lr_geoip::GetName()
{
	return "[LR] GEOIP";
}

const char *lr_geoip::GetURL()
{
	return "https://discord.com/invite/g798xERK5Y";
}

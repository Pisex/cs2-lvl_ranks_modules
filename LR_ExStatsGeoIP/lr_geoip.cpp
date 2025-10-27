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

#define SAFE_MMDB_STR(result, entry_data, buffer, ...)                                                  \
    if (MMDB_get_value(&(result).entry, &(entry_data), __VA_ARGS__, NULL) == MMDB_SUCCESS &&            \
        (entry_data).has_data && (entry_data).type == MMDB_DATA_TYPE_UTF8_STRING) {                     \
        g_SMAPI->Format(buffer, (entry_data).data_size + 1, "%.*s",                                     \
                        (entry_data).data_size, (entry_data).utf8_string);                              \
    } else {                                                                                            \
        buffer[0] = '\0'; /* очищаем строку, если нет данных */                                         \
    }

void OnPlayerLoadedHook(int iSlot, const char* SteamID)
{
    char sQuery[1024], sCity[45] = "", sRegion[45] = "", sCountry[45] = "", sCountryCode[3] = "";
    auto playerNetInfo = engine->GetPlayerNetInfo(iSlot);
    if (playerNetInfo == nullptr) {
        Msg("PlayerNetInfo is null for slot %d.\n", iSlot);
        return;
    }

    auto sIp2 = std::string(playerNetInfo->GetAddress());
    if (sIp2.empty()) {
        Msg("Failed to retrieve IP address for slot %d.\n", iSlot);
        return;
    }

    auto sIp = sIp2.substr(0, sIp2.find(":"));
    if (sIp.empty()) {
        Msg("Extracted IP address is empty for slot %d.\n", iSlot);
        return;
    }

    MMDB_s mmdb;
    char szPath[256];

    int iSize = g_SMAPI->PathFormat(szPath, sizeof(szPath), "%s/addons/configs/geoip/GeoLite2-City.mmdb", g_SMAPI->GetBaseDir());
    if (iSize <= 0) {
        Msg("Failed to format GeoIP database path.\n");
        return;
    }

    int status = MMDB_open(szPath, MMDB_MODE_MMAP, &mmdb);
    if (status != MMDB_SUCCESS) {
        Msg("Error opening the GeoIP database: %s\n", MMDB_strerror(status));
        return;
    }

    int gai_error = 0, mmdb_error = 0;
    MMDB_lookup_result_s result = MMDB_lookup_string(&mmdb, sIp.c_str(), &gai_error, &mmdb_error);

    if (gai_error != 0) {
        Msg("GeoIP lookup failed with GAI error: %s\n", gai_strerror(gai_error));
        MMDB_close(&mmdb);
        return;
    }

    if (mmdb_error != MMDB_SUCCESS) {
        Msg("GeoIP lookup failed with MMDB error: %s\n", MMDB_strerror(mmdb_error));
        MMDB_close(&mmdb);
        return;
    }

    if (!result.found_entry) {
        Msg("No GeoIP entry found for IP: %s\n", sIp.c_str());
        MMDB_close(&mmdb);
        return;
    }

    MMDB_entry_data_s entry_data;
    
    SAFE_MMDB_STR(result, entry_data, sCountry,     "country", "names", "en")
    SAFE_MMDB_STR(result, entry_data, sCountryCode, "country", "iso_code")
    SAFE_MMDB_STR(result, entry_data, sCity,        "city", "names", "en")
    SAFE_MMDB_STR(result, entry_data, sRegion,      "subdivisions", "0", "names", "en")

    MMDB_close(&mmdb);

    g_SMAPI->Format(sQuery, sizeof(sQuery),
        "INSERT IGNORE INTO `%s_geoip` SET `steam` = '%s', `clientip` = '%s', `country` = '%s', `region` = '%s', `city` = '%s', `country_code` = '%s' "
        "ON DUPLICATE KEY UPDATE `clientip` = '%s', `country` = '%s', `region` = '%s', `city` = '%s', `country_code` = '%s';",
        g_sTableName, SteamID, sIp.c_str(), sCountry, sRegion, sCity, sCountryCode,
        sIp.c_str(), sCountry, sRegion, sCity, sCountryCode);

    if (!g_pConnection) {
        Msg("Database connection is null. Query not executed.\n");
        return;
    }

    g_pConnection->Query(sQuery, [](ISQLQuery* test) {
        if (!test) {
            Msg("Failed to execute GeoIP query.\n");
        }
    });
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
	return "1.0.1";
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

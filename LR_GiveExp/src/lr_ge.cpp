#include <stdio.h>
#include "lr_ge.h"
#include <sstream>

lr_ge g_lr_ge;

ILRApi* g_pLRCore;

IMySQLConnection* g_pConnection;

IVEngineServer2* engine = nullptr;
CSchemaSystem* g_pCSchemaSystem = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;

char g_sTableName[96];
PLUGIN_EXPOSE(lr_ge, g_lr_ge);

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

bool containsOnlyDigits(const std::string& str) {
	return str.find_first_not_of("0123456789") == std::string::npos;
}

bool lr_ge::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();
	GET_V_IFACE_ANY(GetEngineFactory, g_pCSchemaSystem, CSchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetServerFactory, g_pSource2GameClients, IServerGameClients, SOURCE2GAMECLIENTS_INTERFACE_VERSION);
	ConVar_Register(FCVAR_RELEASE | FCVAR_SERVER_CAN_EXECUTE | FCVAR_GAMEDLL);
	g_SMAPI->AddListener( this, this );
	return true;
}

bool lr_ge::Unload(char *error, size_t maxlen)
{
	delete g_pLRCore;
	return true;
}

void OnCoreIsReadyHook()
{
	g_pConnection = g_pLRCore->GetDatabases();
	g_SMAPI->Format(g_sTableName, sizeof(g_sTableName), g_pLRCore->GetTableName());
}

CON_COMMAND_EXTERN(lr_giveexp, OnGiveExp, "");

void OnGiveExp(const CCommandContext& context, const CCommand& args)
{
	auto slot = context.GetPlayerSlot();
	if (args.ArgC() > 2 && args[1][0] && args[2][0] && containsOnlyDigits(args[2]))
	{
		int iCount = std::stoi(args[2]);
		
		for (int i = 0; i < 64; i++)
		{
			if(engine->IsClientFullyAuthenticated(i))
			{
				std::string szSteamID = ConvertSteamID(engine->GetPlayerNetworkIDString(i));
				if(!strcmp(szSteamID.c_str(), args[1]))
				{
					g_pLRCore->ChangeClientValue(i, iCount);
					return;
				}
			}
		}
		char szQuery[1024];
		g_SMAPI->Format(szQuery, sizeof(szQuery), "UPDATE `%s` SET `value` = `value` + '%i' WHERE `steam` = '%s';", g_sTableName, iCount, args[1]);
		g_pConnection->Query(szQuery, [](IMySQLQuery* test){});
	}
	else META_CONPRINTF("[Admin] Usage: lr_giveexp \"STEAM_1:0:84111\" \"1000\"\n");
}


void lr_ge::AllPluginsLoaded()
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
	if(g_pLRCore->CoreIsLoaded()) OnCoreIsReadyHook();
	else g_pLRCore->HookOnCoreIsReady(g_PLID, OnCoreIsReadyHook);
}

const char *lr_ge::GetLicense()
{
	return "Public";
}

const char *lr_ge::GetVersion()
{
	return "1.0";
}

const char *lr_ge::GetDate()
{
	return __DATE__;
}

const char *lr_ge::GetLogTag()
{
	return "[LR-GE]";
}

const char *lr_ge::GetAuthor()
{
	return "Pisex";
}

const char *lr_ge::GetDescription()
{
	return "";
}

const char *lr_ge::GetName()
{
	return "[LR] Give Exp";
}

const char *lr_ge::GetURL()
{
	return "https://discord.com/invite/g798xERK5Y";
}

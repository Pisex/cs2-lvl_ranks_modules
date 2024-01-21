#include <stdio.h>
#include "lr_vip_multi.h"

lr_vip_multi g_lr_vip_multi;

ILRApi* g_pLRCore;
IVIPApi* g_pVIPCore;

IVEngineServer2* engine = nullptr;
CSchemaSystem* g_pCSchemaSystem = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;

PLUGIN_EXPOSE(lr_vip_multi, g_lr_vip_multi);
bool lr_vip_multi::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();
	GET_V_IFACE_ANY(GetEngineFactory, g_pCSchemaSystem, CSchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
	g_SMAPI->AddListener( this, this );
	return true;
}

bool lr_vip_multi::Unload(char *error, size_t maxlen)
{
	delete g_pLRCore;
	return true;
}

void OnPlayerKilledPreHook(IGameEvent* hEvent, int &iExpCaused, int iExpVictim, int iExpAttacker)
{
	int iClient = hEvent->GetInt("attacker");
	float fValue = g_pVIPCore->VIP_GetClientFeatureFloat(iClient, "LrBoost");
    if(g_pVIPCore->VIP_IsClientVIP(iClient))
    {
        iExpCaused = int(iExpCaused * fValue);
    }
}

void OnCoreIsReadyHook()
{
	g_pLRCore->HookOnPlayerKilledPre(g_PLID, OnPlayerKilledPreHook);
}

void lr_vip_multi::AllPluginsLoaded()
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
	g_pVIPCore = (IVIPApi*)g_SMAPI->MetaFactory(VIP_INTERFACE, &ret, NULL);

	if (ret == META_IFACE_FAILED)
	{
		char error[64];
		V_strncpy(error, "Failed to lookup vip core. Aborting", 64);
		ConColorMsg(Color(255, 0, 0, 255), "[%s] %s\n", GetLogTag(), error);
		std::string sBuffer = "meta unload "+std::to_string(g_PLID);
		engine->ServerCommand(sBuffer.c_str());
		return;
	}
	g_pLRCore->HookOnCoreIsReady(g_PLID, OnCoreIsReadyHook);
	g_pVIPCore->VIP_RegisterFeature("LrBoost", VIP_BOOL, TOGGLABLE);
}

const char *lr_vip_multi::GetLicense()
{
	return "Public";
}

const char *lr_vip_multi::GetVersion()
{
	return "1.0";
}

const char *lr_vip_multi::GetDate()
{
	return __DATE__;
}

const char *lr_vip_multi::GetLogTag()
{
	return "[LR-VIP-MULTI]";
}

const char *lr_vip_multi::GetAuthor()
{
	return "Pisex";
}

const char *lr_vip_multi::GetDescription()
{
	return "";
}

const char *lr_vip_multi::GetName()
{
	return "[LR][VIP] Multi";
}

const char *lr_vip_multi::GetURL()
{
	return "https://discord.com/invite/g798xERK5Y";
}

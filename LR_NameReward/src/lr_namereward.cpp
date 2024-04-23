#include <stdio.h>
#include "lr_namereward.h"

lr_namereward g_lr_namereward;

ILRApi* g_pLRCore;
IVIPApi* g_pVIPCore;

IVEngineServer2* engine = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;

float fMulti;
char pszName[64];

PLUGIN_EXPOSE(lr_namereward, g_lr_namereward);
bool lr_namereward::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();
	GET_V_IFACE_CURRENT(GetFileSystemFactory, g_pFullFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);

	{
		KeyValues* hKv = new KeyValues("LR");
		const char *pszPath = "addons/configs/levels_ranks/namereward.ini";

		if (!hKv->LoadFromFile(g_pFullFileSystem, pszPath))
		{
			Warning("Failed to load %s\n", pszPath);
			return false;
		}

		fMulti = hKv->GetFloat("multi");
		g_SMAPI->Format(pszName, sizeof(pszName), hKv->GetString("name"));
		delete hKv;
	}
	g_SMAPI->AddListener( this, this );
	return true;
}

bool lr_namereward::Unload(char *error, size_t maxlen)
{
	delete g_pLRCore;
	return true;
}

void OnPlayerKilledPreHook(IGameEvent* hEvent, int &iExpCaused, int iExpVictim, int iExpAttacker)
{
	int iClient = hEvent->GetInt("attacker");
    if(strstr(engine->GetClientConVarValue(iClient, "name"), pszName))
    {
        iExpCaused = int(iExpCaused * fMulti);
    }
}

void OnCoreIsReadyHook()
{
	g_pLRCore->HookOnPlayerKilledPre(g_PLID, OnPlayerKilledPreHook);
}

void lr_namereward::AllPluginsLoaded()
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
	g_pLRCore->HookOnCoreIsReady(g_PLID, OnCoreIsReadyHook);
}

const char *lr_namereward::GetLicense()
{
	return "Public";
}

const char *lr_namereward::GetVersion()
{
	return "1.0";
}

const char *lr_namereward::GetDate()
{
	return __DATE__;
}

const char *lr_namereward::GetLogTag()
{
	return "[LR-NAME-REWARD]";
}

const char *lr_namereward::GetAuthor()
{
	return "Pisex";
}

const char *lr_namereward::GetDescription()
{
	return "";
}

const char *lr_namereward::GetName()
{
	return "[LR] Name Reward";
}

const char *lr_namereward::GetURL()
{
	return "https://discord.com/invite/g798xERK5Y";
}

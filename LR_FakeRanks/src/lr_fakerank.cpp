#include "lr_fakerank.h"
#include <convar.h>
#include <networksystem/inetworkserializer.h>
#include <networksystem/inetworkmessages.h>
#include <inetchannel.h>
#include "schemasystem/schemasystem.h"
#include "protobuf/generated/cstrike15_usermessages.pb.h"

LR_FakeRank g_LR_FakeRank;
PLUGIN_EXPOSE(LR_FakeRank, g_LR_FakeRank);


ILRApi* g_pLRCore;

IVEngineServer2* engine = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;
IGameEventSystem *g_pGameEventSystem = nullptr;
IGameResourceService* g_pGameResourceService = nullptr;

class GameSessionConfiguration_t { };
SH_DECL_HOOK3_void(INetworkServerService, StartupServer, SH_NOATTRIB, 0, const GameSessionConfiguration_t&, ISource2WorldSession*, const char*);
SH_DECL_HOOK3_void(IServerGameDLL, GameFrame, SH_NOATTRIB, 0, bool, bool, bool);

uint64_t iOldButtons[64];

int g_iType;
bool bLoaded = false;
std::map<int, int> g_Ranks;

void LoadConfig()
{
	g_Ranks.clear();
	{
		KeyValues* hKv = new KeyValues("LR_FakeRank");
		const char *pszPath = "addons/configs/levels_ranks/fakerank.ini";

		if (!hKv->LoadFromFile(g_pFullFileSystem, pszPath))
		{
			Warning("Failed to load %s\n", pszPath);
			return;
		}

		switch(hKv->GetInt("Type", 0))
		{
			case 0:
			{
				g_iType = 12;
				break;
			}
			case 1:
			{
				g_iType = 7;
				break;
			}
			case 2:
			{
				g_iType = 10;
				break;
			}
			case 3:
			{
				g_iType = 11;
				break;
			}
			case 4:
			{
				g_iType = 13;
				break;
			}
		}
		hKv = hKv->FindKey("FakeRank", false);
		FOR_EACH_VALUE(hKv, pValue)
		{
			g_Ranks[std::stoi(pValue->GetName())] = pValue->GetInt(nullptr);
		}
		delete hKv;
	}
}

bool LR_FakeRank::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION)
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pNetworkMessages, INetworkMessages, NETWORKMESSAGES_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pGameEventSystem, IGameEventSystem, GAMEEVENTSYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetServerFactory, g_pSource2Server, ISource2Server, SOURCE2SERVER_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pNetworkServerService, INetworkServerService, NETWORKSERVERSERVICE_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pGameResourceService, IGameResourceService, GAMERESOURCESERVICESERVER_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetFileSystemFactory, g_pFullFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);

	SH_ADD_HOOK(IServerGameDLL, GameFrame, g_pSource2Server, SH_MEMBER(this, &LR_FakeRank::GameFrame), true);
	SH_ADD_HOOK(INetworkServerService, StartupServer, g_pNetworkServerService, SH_MEMBER(this, &LR_FakeRank::StartupServer), true);

	ConVar_Register(FCVAR_RELEASE | FCVAR_SERVER_CAN_EXECUTE | FCVAR_GAMEDLL);

	LoadConfig();
	return true;
}

CON_COMMAND_EXTERN(mm_reload_fakerank, ReloadCommand, "Reload Fakerank config");

void ReloadCommand(const CCommandContext& context, const CCommand& args)
{
	LoadConfig();
}

// CON_COMMAND_EXTERN(mm_value, ValueCommand, "Change map");

// void ValueCommand(const CCommandContext& context, const CCommand& args)
// {
// 	auto slot = context.GetPlayerSlot();
// 	g_iValue[slot.Get()] = std::stol(args[1]);
// }

bool LR_FakeRank::Unload(char *error, size_t maxlen)
{
	SH_REMOVE_HOOK(IServerGameDLL, GameFrame, g_pSource2Server, SH_MEMBER(this, &LR_FakeRank::GameFrame), true);
	SH_REMOVE_HOOK(INetworkServerService, StartupServer, g_pNetworkServerService, SH_MEMBER(this, &LR_FakeRank::StartupServer), true);
	return true;
}

void LR_FakeRank::GameFrame(bool simulating, bool bFirstTick, bool bLastTick)
{
	if(bLoaded)
	{
		CRecipientFilter filter;
		for (int i = 0; i < 64; i++)
		{
			CCSPlayerController* pPlayerController =  CCSPlayerController::FromSlot(i);
			if(!pPlayerController || !pPlayerController->m_hPawn() || !pPlayerController->m_hPlayerPawn()) continue;
			if(pPlayerController->m_steamID() <= 0)
				continue;
			pPlayerController->m_iCompetitiveWins() = 111;
			switch (g_iType)
			{
			case 11:
				pPlayerController->m_iCompetitiveRanking() = g_pLRCore->GetClientInfo(i, ST_EXP);
				pPlayerController->m_iCompetitiveRankType() = g_iType;
				break;
			case 13:
				pPlayerController->m_iCompetitiveRanking() = g_Ranks[g_pLRCore->GetClientInfo(i, ST_RANK)];
				pPlayerController->m_iCompetitiveRankType() = 11;
				break;
			default:
				pPlayerController->m_iCompetitiveRanking() = g_Ranks[g_pLRCore->GetClientInfo(i, ST_RANK)];
				pPlayerController->m_iCompetitiveRankType() = g_iType;
				break;
			}
			uint64_t iButtons = pPlayerController->m_hPlayerPawn()->m_pMovementServices()->m_nButtons().m_pButtonStates()[0];
			if(std::to_string(iButtons).find("858993") != std::string::npos && !(std::to_string(iOldButtons[i]).find("858993") != std::string::npos))
			{
				filter.AddRecipient(CPlayerSlot(i));
			}
			iOldButtons[i] = iButtons;
		}
		if(filter.GetRecipientCount() > 0)
		{
			static INetworkSerializable* message_type = g_pNetworkMessages->FindNetworkMessagePartial("CCSUsrMsg_ServerRankRevealAll");
			CCSUsrMsg_ServerRankRevealAll message;
			g_pGameEventSystem->PostEventAbstract(0, false, &filter, message_type, &message, 0);
		}
	}
}

void CoreIsReady()
{
	bLoaded = true;
}

void LR_FakeRank::AllPluginsLoaded()
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
	g_pLRCore->HookOnCoreIsReady(g_PLID, CoreIsReady);
}

CGameEntitySystem* GameEntitySystem()
{
	g_pGameEntitySystem = *reinterpret_cast<CGameEntitySystem**>(reinterpret_cast<uintptr_t>(g_pGameResourceService) + 0x50);
	return g_pGameEntitySystem;
}

void LR_FakeRank::StartupServer(const GameSessionConfiguration_t& config, ISource2WorldSession*, const char*)
{
	g_pEntitySystem = GameEntitySystem();
}

///////////////////////////////////////
const char* LR_FakeRank::GetLicense()
{
	return "GPL";
}

const char* LR_FakeRank::GetVersion()
{
	return "1.1.0";
}

const char* LR_FakeRank::GetDate()
{
	return __DATE__;
}

const char* LR_FakeRank::GetLogTag()
{
	return "[LR-FR]";
}

const char* LR_FakeRank::GetAuthor()
{
	return "Pisex";
}

const char* LR_FakeRank::GetDescription()
{
	return "[LR] FakeRank";
}

const char* LR_FakeRank::GetName()
{
	return "[LR] FakeRank";
}

const char* LR_FakeRank::GetURL()
{
	return "https://discord.gg/g798xERK5Y";
}


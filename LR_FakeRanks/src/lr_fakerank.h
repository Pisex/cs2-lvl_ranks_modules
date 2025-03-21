﻿#ifndef _INCLUDE_METAMOD_SOURCE_STUB_PLUGIN_H_
#define _INCLUDE_METAMOD_SOURCE_STUB_PLUGIN_H_

class CGameEntitySystem;

#include <ISmmPlugin.h>
#include <sh_vector.h>
#include "utlvector.h"
#include "ehandle.h"
#include <iserver.h>
#include <entity2/entitysystem.h>
#include "igameevents.h"
#include "entitysystem.h"
#include "igameeventsystem.h"
#include "vector.h"
#include "CCSPlayerController.h"
#include "CGameRules.h"
#include "module.h"
#include "include/lvl_ranks.h"
#include "irecipientfilter.h"

class CRecipientFilter : public IRecipientFilter
{
public:
	CRecipientFilter()
	{
		m_nBufType = BUF_RELIABLE;
		m_bInitMessage = false;
	}

	CRecipientFilter(IRecipientFilter *source, int iExcept = -1)
	{
		m_nBufType = source->GetNetworkBufType();
		m_bInitMessage = source->IsInitMessage();
		m_Recipients.RemoveAll();

		for (int i = 0; i < source->GetRecipientCount(); i++)
		{
			if (source->GetRecipientIndex(i).Get() != iExcept)
				m_Recipients.AddToTail(source->GetRecipientIndex(i));
		}
	}

	~CRecipientFilter() override {}

	NetChannelBufType_t GetNetworkBufType(void) const override { return m_nBufType; }
	bool IsInitMessage(void) const override { return m_bInitMessage; }
	int GetRecipientCount(void) const override { return m_Recipients.Count(); }

	CPlayerSlot GetRecipientIndex(int slot) const override
	{
		if (slot < 0 || slot >= GetRecipientCount())
			return CPlayerSlot(-1);

		return m_Recipients[slot];
	}

	void AddRecipient(CPlayerSlot slot)
	{
		// Don't add if it already exists
		if (m_Recipients.Find(slot) != m_Recipients.InvalidIndex())
			return;

		m_Recipients.AddToTail(slot);
	}

private:
	NetChannelBufType_t m_nBufType;
	bool m_bInitMessage;
	CUtlVectorFixed<CPlayerSlot, 65> m_Recipients;
};

class LR_FakeRank final : public ISmmPlugin, public IMetamodListener
{
public:
	bool Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late);
	bool Unload(char* error, size_t maxlen);
	void AllPluginsLoaded();
private:
	const char* GetAuthor();
	const char* GetName();
	const char* GetDescription();
	const char* GetURL();
	const char* GetLicense();
	const char* GetVersion();
	const char* GetDate();
	const char* GetLogTag();
	void StartupServer(const GameSessionConfiguration_t& config, ISource2WorldSession*, const char*);
	void GameFrame(bool simulating, bool bFirstTick, bool bLastTick);
};

#endif //_INCLUDE_METAMOD_SOURCE_STUB_PLUGIN_H_

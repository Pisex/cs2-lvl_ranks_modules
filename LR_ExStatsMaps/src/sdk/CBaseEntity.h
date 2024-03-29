#pragma once
#include <entity2/entityidentity.h>
#include <baseentity.h>
#include "schemasystem.h"

class SC_CBaseEntity : public CBaseEntity
{
public:
	SCHEMA_FIELD(uint8_t, CBaseEntity, m_iTeamNum);
	SCHEMA_FIELD(LifeState_t, CBaseEntity, m_lifeState);
};
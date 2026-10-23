#pragma once

#include <playerslot.h>
#include "igameevents.h"
#include <functional>
#include <string>
#include <vector>
#include <map>

// Used by VIP_GetEntitySystem(), forward-declared so modules don't depend on include order
class CGameEntitySystem;

#define VIP_INTERFACE "IVIPApi"

enum VIP_ValueType
{
	VIP_NULL	= 0,	// Нет данных
	VIP_INT,				// Целочисленный тип
	VIP_FLOAT,				// Числа с плавающей точкой (Дробные)
	VIP_BOOL,			// Логический тип (1/0)
	VIP_STRING				// Строчный тип
};

enum VIP_ToggleState
{
	DISABLED = 0,		// Выключено
	ENABLED,			// Включено
	NO_ACCESS			// Нет доступа
};

enum VIP_FeatureType
{
	TOGGLABLE = 0,		// Вкл/Выкл в меню
	SELECTABLE,			// Только нажатие
	HIDE,				// Скрытый
	DROPDOWN			// Выпадающий список (выбор одного из вариантов), см. VIP_RegisterDropdownFeature
};


// (для типа TOGGLABLE)
typedef std::function<bool(int iSlot, const char* szFeature, VIP_ToggleState eOldStatus, VIP_ToggleState& eNewStatus)> ItemTogglableCallback;
// (для типа SELECTABLE)
typedef std::function<bool(int iSlot, const char* szFeature)> ItemSelectableCallback;

// Визуал
typedef std::function<std::string(int iSlot, const char* szFeature)> ItemDisplayCallback;

// Вариант выпадающего списка (для типа DROPDOWN)
struct VIP_FeatureOption
{
	std::string		sValue;		// Значение (сохраняется в куки игрока)
	std::string		sName;		// Текст в меню
};
// Список вариантов для игрока (может зависеть от его группы)
typedef std::function<std::vector<VIP_FeatureOption>(int iSlot, const char* szFeature)> ItemOptionsCallback;
// Выбор варианта. iNewOption можно изменить. Вернуть true - заблокировать смену
typedef std::function<bool(int iSlot, const char* szFeature, int iOldOption, int& iNewOption)> ItemDropdownCallback;


typedef std::function<void()> ReadyCallbackFunc;
typedef std::function<void(int iSlot, int iTeam, bool bIsVIP)> SpawnCallbackFunc;
typedef std::function<void(int iSlot, bool bIsVIP)> ClientLoadedOrDisconnectCallbackFunc;
typedef std::function<void(int iSlot)> VIPAddCallbackFunc;
typedef std::function<void(int iSlot, int iReason)> VIPRemoveCallbackFunc;
// Вызывается перед открытием VIP меню (команда !vip / VIP_OpenMenu).
// Вызывается для любого игрока, даже без VIP (проверка доступа идёт после хуков).
// Вернуть true - отменить открытие стандартного меню.
typedef std::function<bool(int iSlot)> VIPMenuOpenCallbackFunc;

// Пункт VIP меню игрока
struct VIP_MenuItem
{
	std::string		sFeature;	// Ключ функции (параметр в groups.ini)
	std::string		sName;		// Перевод названия (или ключ, если перевода нет)
	std::string		sValue;		// Значение параметра из группы игрока (без учёта вкл/выкл)
	std::string		sDisplay;	// Готовый текст пункта, как в стандартном меню (с учётом ItemDisplayCallback)
	VIP_ValueType	eValType;
	VIP_FeatureType	eType;
	VIP_ToggleState	eState;		// Текущий статус у игрока
	std::vector<VIP_FeatureOption> vecOptions;	// Варианты (только DROPDOWN)
	int				iOption = -1;				// Выбранный вариант (только DROPDOWN)
};

class IVIPApi
{
public:
	virtual bool VIP_IsVIPLoaded() = 0;
	virtual bool VIP_IsClientVIP(int iSlot) = 0;
	virtual int VIP_GetClientAccessTime(int iSlot) = 0;
	virtual bool VIP_SetClientAccessTime(int iSlot, int iTime, bool bInDB = true) = 0;
	virtual bool VIP_SetClientVIPGroup(int iSlot, const char* szGroup, bool bInDB = true) = 0;
	virtual const char *VIP_GetClientVIPGroup(int iSlot) = 0;

	virtual bool VIP_GiveClientVIP(int iSlot, int iTime, const char* szGroup, bool bAddToDB = 1) = 0;
	virtual bool VIP_RemoveClientVIP(int iSlot, bool bNotify, bool bInDB = 1) = 0;

	virtual int VIP_GetClientFeatureInt(int iSlot, const char* szFeature) = 0;
	virtual bool VIP_GetClientFeatureBool(int iSlot, const char* szFeature) = 0;
	virtual float VIP_GetClientFeatureFloat(int iSlot, const char* szFeature) = 0;
	virtual const char *VIP_GetClientFeatureString(int iSlot, const char* szFeature) = 0;

	virtual CGameEntitySystem* VIP_GetEntitySystem() = 0;
    virtual int VIP_GetTotalRounds() = 0;

	virtual bool VIP_IsValidVIPGroup(const char* szGroup) = 0;

	virtual bool VIP_PistolRound() = 0;
	virtual bool VIP_WarmupPeriod() = 0;

	virtual bool VIP_SetClientCookie(int iSlot, const char* sCookieName, const char* sData) = 0;
	virtual const char *VIP_GetTranslate(const char* phrase) = 0;
	virtual const char *VIP_GetClientCookie(int iSlot, const char* sCookieName) = 0;

	virtual void VIP_PrintToCenter(int iSlot, const char* msg, ...) = 0;

	//Forwards
    virtual void VIP_OnVIPLoaded(ReadyCallbackFunc callback) = 0;
	virtual void VIP_OnClientLoaded(ClientLoadedOrDisconnectCallbackFunc callback) = 0;
	virtual void VIP_OnClientDisconnect(ClientLoadedOrDisconnectCallbackFunc callback) = 0;
	virtual void VIP_OnPlayerSpawn(SpawnCallbackFunc callback) = 0;
	virtual void VIP_OnVIPClientRemoved(VIPRemoveCallbackFunc callback) = 0;
	virtual void VIP_OnVIPClientAdded(VIPAddCallbackFunc callback) = 0;

	
    virtual void VIP_RegisterFeature(const char*			    szFeature,
								VIP_ValueType			eValType				= VIP_NULL,
								VIP_FeatureType			eType					= TOGGLABLE,
								ItemSelectableCallback	Item_select_callback	= nullptr,
								ItemTogglableCallback	Item_togglable_callback	= nullptr,
								ItemDisplayCallback		Item_display_callback	= nullptr) = 0;

	virtual void VIP_OpenMenu(int iSlot) = 0;

	// ===================== Menu API =====================

	// Хук открытия VIP меню. Вернуть true из колбэка - отменить открытие.
	// Внутри хука не вызывайте VIP_OpenMenu (рекурсия), используйте своё меню или VIP_OpenDefaultMenu
	// Хуки привязаны к PluginId и снимаются автоматически при выгрузке плагина
	virtual void VIP_OnMenuOpen(SourceMM::PluginId id, VIPMenuOpenCallbackFunc callback) = 0;
	// Открыть стандартное меню ядра, не вызывая хуки VIP_OnMenuOpen
	virtual void VIP_OpenDefaultMenu(int iSlot) = 0;

	// Все зарегистрированные функции (VIP_RegisterFeature)
	virtual std::vector<std::string> VIP_GetFeatures() = 0;
	virtual bool VIP_IsFeatureRegistered(const char* szFeature) = 0;
	virtual VIP_ValueType VIP_GetFeatureValueType(const char* szFeature) = 0;
	virtual VIP_FeatureType VIP_GetFeatureType(const char* szFeature) = 0;
	virtual void VIP_UnregisterFeature(const char* szFeature) = 0;

	// Пункты VIP меню игрока в том же порядке и с той же фильтрацией, что и в стандартном меню.
	// bIncludeHidden - включить функции типа HIDE
	virtual std::vector<VIP_MenuItem> VIP_GetClientMenuItems(int iSlot, bool bIncludeHidden = false) = 0;
	// Текст пункта меню, как в стандартном меню (пустая строка, если нет доступа)
	virtual std::string VIP_GetClientFeatureDisplay(int iSlot, const char* szFeature) = 0;

	// Статус функции у игрока (вкл / выкл / нет доступа)
	virtual VIP_ToggleState VIP_GetClientFeatureStatus(int iSlot, const char* szFeature) = 0;
	// Установить статус. bCallback - вызвать ItemTogglableCallback (может заблокировать изменение).
	// Возвращает true, если статус изменён
	virtual bool VIP_SetClientFeatureStatus(int iSlot, const char* szFeature, VIP_ToggleState eStatus, bool bCallback = true) = 0;
	// Эмуляция нажатия на пункт меню: TOGGLABLE - переключение, SELECTABLE - вызов ItemSelectableCallback,
	// DROPDOWN - выбор следующего варианта по кругу.
	// Возвращает результат ItemSelectableCallback (true - модуль просит закрыть меню), для остальных - false
	virtual bool VIP_PressClientFeature(int iSlot, const char* szFeature) = 0;

	// Параметры
	// Все параметры игрока (группа + наследование legacy + переопределения VIP_SetClientParam)
	virtual std::map<std::string, std::string> VIP_GetClientParams(int iSlot) = 0;
	// Сырое значение параметра игрока без учёта вкл/выкл
	virtual std::string VIP_GetClientParam(int iSlot, const char* szParam) = 0;
	// Временно переопределить параметр игроку (сбрасывается при выдаче/снятии/смене группы VIP и переподключении).
	// Пустое значение - убрать параметр (функция пропадёт из меню и станет NO_ACCESS)
	virtual bool VIP_SetClientParam(int iSlot, const char* szParam, const char* szValue) = 0;
	// Сбросить переопределения параметров игрока
	virtual void VIP_ResetClientParams(int iSlot) = 0;
	// Список групп и их параметры (как в groups.ini, без наследования)
	virtual std::vector<std::string> VIP_GetGroups() = 0;
	virtual std::map<std::string, std::string> VIP_GetGroupParams(const char* szGroup) = 0;

	// ===================== Выпадающий список (DROPDOWN) =====================
	// Варианты берутся из OptionsCallback. Если он не задан - из значения параметра в groups.ini
	// через ";" (например "red;green;blue"), названия - перевод "<feature>_<value>" или само значение.
	// Выбор сохраняется в куки "<feature>_opt". По умолчанию - вариант, совпадающий со значением
	// параметра группы, иначе первый. Статус DROPDOWN функции - ENABLED или NO_ACCESS.
	virtual void VIP_RegisterDropdownFeature(const char*			szFeature,
											VIP_ValueType			eValType				= VIP_STRING,
											ItemOptionsCallback		Item_options_callback	= nullptr,
											ItemDropdownCallback	Item_dropdown_callback	= nullptr,
											ItemDisplayCallback		Item_display_callback	= nullptr) = 0;
	// Варианты игрока (пусто, если нет доступа или функция не DROPDOWN)
	virtual std::vector<VIP_FeatureOption> VIP_GetClientFeatureOptions(int iSlot, const char* szFeature) = 0;
	// Индекс выбранного варианта (-1, если нет доступа или вариантов)
	virtual int VIP_GetClientFeatureOption(int iSlot, const char* szFeature) = 0;
	// Значение (sValue) выбранного варианта (пустая строка, если нет)
	virtual std::string VIP_GetClientFeatureOptionValue(int iSlot, const char* szFeature) = 0;
	// Выбрать вариант. bCallback - вызвать ItemDropdownCallback. Возвращает true, если выбор изменён
	virtual bool VIP_SetClientFeatureOption(int iSlot, const char* szFeature, int iOption, bool bCallback = true) = 0;
};
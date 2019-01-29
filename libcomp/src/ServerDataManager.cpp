/**
 * @file libcomp/src/ServerDataManager.cpp
 * @ingroup libcomp
 *
 * @author HACKfrost
 *
 * @brief Manages loading and storing server data objects.
 *
 * This file is part of the COMP_hack Library (libcomp).
 *
 * Copyright (C) 2012-2018 COMP_hack Team <compomega@tutanota.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ServerDataManager.h"

// libcomp Includes
#include "DefinitionManager.h"
#include "Log.h"
#include "ScriptEngine.h"

// object Includes
#include <Action.h>
#include <ActionDelay.h>
#include <ActionSpawn.h>
#include <ActionZoneChange.h>
#include <ActionZoneInstance.h>
#include <AILogicGroup.h>
#include <DemonPresent.h>
#include <DemonQuestReward.h>
#include <DropSet.h>
#include <EnchantSetData.h>
#include <EnchantSpecialData.h>
#include <Event.h>
#include <EventPerformActions.h>
#include <MiSItemData.h>
#include <MiSStatusData.h>
#include <MiZoneBasicData.h>
#include <MiZoneData.h>
#include <PlasmaSpawn.h>
#include <PvPInstanceVariant.h>
#include <ServerNPC.h>
#include <ServerObject.h>
#include <ServerShop.h>
#include <ServerShopProduct.h>
#include <ServerShopTab.h>
#include <ServerZone.h>
#include <ServerZoneInstance.h>
#include <ServerZoneInstanceVariant.h>
#include <ServerZonePartial.h>
#include <ServerZoneSpot.h>
#include <ServerZoneTrigger.h>
#include <Spawn.h>
#include <SpawnGroup.h>
#include <SpawnLocationGroup.h>
#include <Tokusei.h>

// Standard C Includes
#include <cmath>

using namespace libcomp;

ServerDataManager::ServerDataManager()
{
}

ServerDataManager::~ServerDataManager()
{
}

const std::shared_ptr<objects::ServerZone> ServerDataManager::GetZoneData(
    uint32_t id, uint32_t dynamicMapID, bool applyPartials,
    std::set<uint32_t> extraPartialIDs)
{
    std::shared_ptr<objects::ServerZone> zone;

    auto iter = mZoneData.find(id);
    if(iter != mZoneData.end())
    {
        if(dynamicMapID != 0)
        {
            auto dIter = iter->second.find(dynamicMapID);
            zone = (dIter != iter->second.end()) ? dIter->second : nullptr;
        }
        else
        {
            // Return first
            zone = iter->second.begin()->second;
        }
    }

    if(applyPartials && zone)
    {
        std::set<uint32_t> partialIDs;

        // Gather all auto-applied partials
        auto partialIter = mZonePartialMap.find(zone->GetDynamicMapID());
        if(partialIter != mZonePartialMap.end())
        {
            partialIDs = partialIter->second;
        }

        // Gather and verify all extra partials
        for(uint32_t partialID : extraPartialIDs)
        {
            auto partial = GetZonePartialData(partialID);
            if(partial && !partial->GetAutoApply() &&
                (partial->DynamicMapIDsCount() == 0 ||
                partial->DynamicMapIDsContains(zone->GetDynamicMapID())))
            {
                partialIDs.insert(partialID);
            }
        }

        if(partialIDs.size() > 0)
        {
            // Copy the definition and apply changes
            libcomp::String zoneStr = libcomp::String("%1%2")
                .Arg(id).Arg(id != dynamicMapID ? libcomp::String(" (%1)")
                    .Arg(dynamicMapID) : "");

            zone = std::make_shared<objects::ServerZone>(*zone);
            for(uint32_t partialID : partialIDs)
            {
                if(!ApplyZonePartial(zone, partialID))
                {
                    // Errored, no zone should be returned
                    return nullptr;
                }
            }

            // Now validate spawn information and correct as needed
            std::set<uint32_t> sgRemoves;
            for(auto sgPair : zone->GetSpawnGroups())
            {
                std::set<uint32_t> missingSpawns;
                for(auto sPair : sgPair.second->GetSpawns())
                {
                    if(!zone->SpawnsKeyExists(sPair.first))
                    {
                        missingSpawns.insert(sPair.first);
                    }
                }

                if(missingSpawns.size() > 0)
                {
                    if(missingSpawns.size() < sgPair.second->SpawnsCount())
                    {
                        // Copy the group and edit the spawns
                        auto sg = std::make_shared<objects::SpawnGroup>(
                            *sgPair.second);
                        for(uint32_t remove : sgRemoves)
                        {
                            sg->RemoveSpawns(remove);
                        }

                        zone->SetSpawnGroups(sgPair.first, sg);
                    }
                    else
                    {
                        sgRemoves.insert(sgPair.first);
                    }
                }
            }

            for(uint32_t sgRemove : sgRemoves)
            {
                LOG_DEBUG(libcomp::String("Removing empty spawn group %1"
                    " when generating zone: %2\n").Arg(sgRemove)
                    .Arg(zoneStr));
                zone->RemoveSpawnGroups(sgRemove);
            }

            std::set<uint32_t> slgRemoves;
            for(auto slgPair : zone->GetSpawnLocationGroups())
            {
                std::set<uint32_t> missingGroups;
                for(uint32_t sgID : slgPair.second->GetGroupIDs())
                {
                    if(!zone->SpawnGroupsKeyExists(sgID))
                    {
                        missingGroups.insert(sgID);
                    }
                }

                if(missingGroups.size() > 0)
                {
                    if(missingGroups.size() < slgPair.second->GroupIDsCount())
                    {
                        // Copy the group and edit the spawns
                        auto slg = std::make_shared<objects::SpawnLocationGroup>(
                            *slgPair.second);
                        for(uint32_t remove : sgRemoves)
                        {
                            slg->RemoveGroupIDs(remove);
                        }

                        zone->SetSpawnLocationGroups(slgPair.first, slg);
                    }
                    else
                    {
                        slgRemoves.insert(slgPair.first);
                    }
                }
            }

            for(uint32_t slgRemove : slgRemoves)
            {
                LOG_DEBUG(libcomp::String("Removing empty spawn location group"
                    " %1 when generating zone: %2\n").Arg(slgRemove)
                    .Arg(zoneStr));
                zone->RemoveSpawnLocationGroups(slgRemove);
            }
        }
    }

    return zone;
}

const std::list<std::pair<uint32_t, uint32_t>> ServerDataManager::GetFieldZoneIDs()
{
    return mFieldZoneIDs;
}

const std::unordered_map<uint32_t, std::set<uint32_t>> ServerDataManager::GetAllZoneIDs()
{
    std::unordered_map<uint32_t, std::set<uint32_t>> zoneIDs;
    for(auto pair : mZoneData)
    {
        for(auto dPair : pair.second)
        {
            zoneIDs[pair.first].insert(dPair.first);
        }
    }

    return zoneIDs;
}

const std::shared_ptr<objects::ServerZoneInstance> ServerDataManager::GetZoneInstanceData(
    uint32_t id)
{
    return GetObjectByID<uint32_t, objects::ServerZoneInstance>(id, mZoneInstanceData);
}

const std::set<uint32_t> ServerDataManager::GetAllZoneInstanceIDs()
{
    std::set<uint32_t> instanceIDs;
    for(auto pair : mZoneInstanceData)
    {
        instanceIDs.insert(pair.first);
    }

    return instanceIDs;
}

bool ServerDataManager::ExistsInInstance(uint32_t instanceID, uint32_t zoneID,
    uint32_t dynamicMapID)
{
    auto instDef = GetZoneInstanceData(instanceID);
    if(instDef)
    {
        for(size_t i = 0; i < instDef->ZoneIDsCount(); i++)
        {
            if(instDef->GetZoneIDs(i) == zoneID &&
               (!dynamicMapID || instDef->GetDynamicMapIDs(i) == dynamicMapID))
            {
                return true;
            }
        }
    }

    return false;
}

const std::shared_ptr<objects::ServerZoneInstanceVariant>
    ServerDataManager::GetZoneInstanceVariantData(uint32_t id)
{
    return GetObjectByID<uint32_t, objects::ServerZoneInstanceVariant>(id,
        mZoneInstanceVariantData);
}

std::set<uint32_t> ServerDataManager::GetStandardPvPVariantIDs(
    uint8_t type) const
{
    auto it = mStandardPvPVariantIDs.find(type);
    return it != mStandardPvPVariantIDs.end()
        ? it->second : std::set<uint32_t>();
}

bool ServerDataManager::VerifyPvPInstance(uint32_t instanceID,
    DefinitionManager* definitionManager)
{
    auto instanceDef = GetZoneInstanceData(instanceID);
    if(instanceDef && definitionManager)
    {
        for(uint32_t zoneID : instanceDef->GetZoneIDs())
        {
            auto zoneDef = definitionManager->GetZoneData(zoneID);
            if(!zoneDef || zoneDef->GetBasic()->GetType() != 7)
            {
                LOG_ERROR(libcomp::String("Instance contains non-PvP zones"
                    " and cannot be used for PvP: %1\n").Arg(instanceID));
                return false;
            }
        }

        return true;
    }

    LOG_ERROR(libcomp::String("Failed to verify PvP instance: %1\n")
        .Arg(instanceID));
    return false;
}

const std::shared_ptr<objects::ServerZonePartial>
    ServerDataManager::GetZonePartialData(uint32_t id)
{
    return GetObjectByID<uint32_t, objects::ServerZonePartial>(id, mZonePartialData);
}

const std::shared_ptr<objects::Event> ServerDataManager::GetEventData(const libcomp::String& id)
{
    return GetObjectByID<std::string, objects::Event>(id.C(), mEventData);
}

const std::shared_ptr<objects::ServerShop> ServerDataManager::GetShopData(uint32_t id)
{
    return GetObjectByID<uint32_t, objects::ServerShop>(id, mShopData);
}

std::list<uint32_t> ServerDataManager::GetCompShopIDs() const
{
    return mCompShopIDs;
}

const std::shared_ptr<objects::AILogicGroup> ServerDataManager::GetAILogicGroup(uint16_t id)
{
    return GetObjectByID<uint16_t, objects::AILogicGroup>(id, mAILogicGroups);
}

const std::shared_ptr<objects::DemonPresent> ServerDataManager::GetDemonPresentData(uint32_t id)
{
    return GetObjectByID<uint32_t, objects::DemonPresent>(id, mDemonPresentData);
}

std::unordered_map<uint32_t,
    std::shared_ptr<objects::DemonQuestReward>> ServerDataManager::GetDemonQuestRewardData()
{
    return mDemonQuestRewardData;
}

const std::shared_ptr<objects::DropSet> ServerDataManager::GetDropSetData(uint32_t id)
{
    return GetObjectByID<uint32_t, objects::DropSet>(id, mDropSetData);
}

const std::shared_ptr<objects::DropSet> ServerDataManager::GetGiftDropSetData(
    uint32_t giftBoxID)
{
    auto it = mGiftDropSetLookup.find(giftBoxID);
    return it != mGiftDropSetLookup.end() ? GetDropSetData(it->second) : nullptr;
}

const std::shared_ptr<ServerScript> ServerDataManager::GetScript(const libcomp::String& name)
{
    return GetObjectByID<std::string, ServerScript>(name.C(), mScripts);
}

const std::shared_ptr<ServerScript> ServerDataManager::GetAIScript(const libcomp::String& name)
{
    return GetObjectByID<std::string, ServerScript>(name.C(), mAIScripts);
}

bool ServerDataManager::LoadData(DataStore *pDataStore,
    DefinitionManager* definitionManager)
{
    bool failure = false;

    if(definitionManager)
    {
        // Load definition dependent server definitions from path or file
        if(!failure)
        {
            LOG_DEBUG("Loading AI logic group server definitions...\n");
            failure = !LoadObjects<objects::AILogicGroup>(
                pDataStore, "/data/ailogicgroup", definitionManager, false,
                true);
        }

        if(!failure)
        {
            LOG_DEBUG("Loading demon present server definitions...\n");
            failure = !LoadObjects<objects::DemonPresent>(
                pDataStore, "/data/demonpresent", definitionManager, false,
                true);
        }

        if(!failure)
        {
            LOG_DEBUG("Loading demon quest reward server definitions...\n");
            failure = !LoadObjects<objects::DemonQuestReward>(
                pDataStore, "/data/demonquestreward", definitionManager, false,
                true);
        }

        if(!failure)
        {
            LOG_DEBUG("Loading drop set server definitions...\n");
            failure = !LoadObjects<objects::DropSet>(
                pDataStore, "/data/dropset", definitionManager, false,
                true);
        }

        if(!failure)
        {
            LOG_DEBUG("Loading enchant set server definitions...\n");
            failure = !LoadObjects<objects::EnchantSetData>(
                pDataStore, "/data/enchantset", definitionManager, false,
                true);
        }

        if(!failure)
        {
            LOG_DEBUG("Loading enchant special server definitions...\n");
            failure = !LoadObjects<objects::EnchantSpecialData>(
                pDataStore, "/data/enchantspecial", definitionManager, false,
                true);
        }

        if(!failure)
        {
            LOG_DEBUG("Loading s-item server definitions...\n");
            failure = !LoadObjects<objects::MiSItemData>(
                pDataStore, "/data/sitemextended", definitionManager, false,
                true);
        }

        if(!failure)
        {
            LOG_DEBUG("Loading s-status server definitions...\n");
            failure = !LoadObjects<objects::MiSStatusData>(
                pDataStore, "/data/sstatus", definitionManager, false,
                true);
        }

        if(!failure)
        {
            LOG_DEBUG("Loading tokusei server definitions...\n");
            failure = !LoadObjects<objects::Tokusei>(
                pDataStore, "/data/tokusei", definitionManager, false,
                true);
        }
    }

    if(!failure)
    {
        LOG_DEBUG("Loading zone server definitions...\n");
        failure = !LoadObjects<objects::ServerZone>(pDataStore, "/zones",
            definitionManager, false, false);
    }

    if(!failure)
    {
        LOG_DEBUG("Loading zone partial server definitions...\n");
        failure = !LoadObjects<objects::ServerZonePartial>(pDataStore,
            "/zones/partial", definitionManager, true, false);
    }

    if(!failure)
    {
        LOG_DEBUG("Loading event server definitions...\n");
        failure = !LoadObjects<objects::Event>(pDataStore, "/events",
            definitionManager, true, false);
    }

    if(!failure)
    {
        LOG_DEBUG("Loading zone instance server definitions...\n");
        failure = !LoadObjects<objects::ServerZoneInstance>(
            pDataStore, "/data/zoneinstance", definitionManager, false,
            true);
    }

    if(!failure)
    {
        LOG_DEBUG("Loading zone instance variant server definitions...\n");
        failure = !LoadObjects<objects::ServerZoneInstanceVariant>(
            pDataStore, "/data/zoneinstancevariant", definitionManager,
            false, true);
    }

    if(!failure)
    {
        LOG_DEBUG("Loading shop server definitions...\n");
        failure = !LoadObjects<objects::ServerShop>(pDataStore, "/shops",
            definitionManager, true, false);
    }

    if(!failure)
    {
        LOG_DEBUG("Loading server scripts...\n");
        failure = !LoadScripts(pDataStore, "/scripts",
            &ServerDataManager::LoadScript);
    }

    return !failure;
}

std::list<std::shared_ptr<ServerScript>> ServerDataManager::LoadScripts(
    DataStore *pDataStore, const libcomp::String& path, bool& success, bool store)
{
    std::list<std::shared_ptr<ServerScript>> scripts;

    auto scriptsOld = mScripts;
    auto aiScriptsOld = mAIScripts;

    success = LoadScripts(pDataStore, path, &ServerDataManager::LoadScript);

    // Return only ones that just loaded
    for(auto& pair : mScripts)
    {
        if(scriptsOld.find(pair.first) == scriptsOld.end())
        {
            scripts.push_back(pair.second);
        }
    }

    for(auto& pair : mAIScripts)
    {
        if(aiScriptsOld.find(pair.first) == aiScriptsOld.end())
        {
            scripts.push_back(pair.second);
        }
    }

    if(!store)
    {
        mScripts = scriptsOld;
        mAIScripts = aiScriptsOld;
    }

    return scripts;
}

bool ServerDataManager::ApplyZonePartial(std::shared_ptr<objects::ServerZone> zone,
    uint32_t partialID)
{
    if(!zone || !partialID)
    {
        return false;
    }

    uint32_t id = zone->GetID();
    uint32_t dynamicMapID = zone->GetDynamicMapID();

    auto originDef = GetZoneData(id, dynamicMapID, false);
    if(originDef == zone)
    {
        LOG_ERROR(libcomp::String("Attempted to apply partial definition to"
            " original zone definition: %1%2\n").Arg(id).Arg(id != dynamicMapID
                ? libcomp::String(" (%1)").Arg(dynamicMapID) : ""));
        return false;
    }

    auto partial = GetZonePartialData(partialID);
    if(!partial)
    {
        LOG_ERROR(libcomp::String("Invalid zone partial ID encountered: %1\n")
            .Arg(partialID));
        return false;
    }

    ApplyZonePartial(zone, partial, true);

    return true;
}

void ServerDataManager::ApplyZonePartial(
    std::shared_ptr<objects::ServerZone> zone,
    const std::shared_ptr<objects::ServerZonePartial>& partial,
    bool positionReplace)
{
    // Add dropsets
    for(uint32_t dropSetID : partial->GetDropSetIDs())
    {
        zone->InsertDropSetIDs(dropSetID);
    }

    // Add whitelist skills
    for(uint32_t skillID : partial->GetSkillWhitelist())
    {
        zone->InsertSkillWhitelist(skillID);
    }

    // Add blacklist skills
    for(uint32_t skillID : partial->GetSkillBlacklist())
    {
        zone->InsertSkillBlacklist(skillID);
    }

    // Build new NPC set
    auto npcs = zone->GetNPCs();
    for(auto& npc : partial->GetNPCs())
    {
        if(positionReplace)
        {
            // Remove any NPCs that share the same spot ID or are within
            // 10 units from the new one (X or Y)
            npcs.remove_if([npc](
                const std::shared_ptr<objects::ServerNPC>& oNPC)
                {
                    return (npc->GetSpotID() &&
                        oNPC->GetSpotID() == npc->GetSpotID()) ||
                        (!npc->GetSpotID() && !oNPC->GetSpotID() &&
                            fabs(oNPC->GetX() - npc->GetX()) < 10.f &&
                            fabs(oNPC->GetY() - npc->GetY()) < 10.f);
                });
        }

        // Removes supported via 0 ID
        if(npc->GetID())
        {
            npcs.push_back(npc);
        }
    }
    zone->SetNPCs(npcs);

    // Build new object set
    auto objects = zone->GetObjects();
    for(auto& obj : partial->GetObjects())
    {
        if(positionReplace)
        {
            // Remove any objects that share the same spot ID or are within
            // 10 units from the new one (X and Y)
            objects.remove_if([obj](
                const std::shared_ptr<objects::ServerObject>& oObj)
                {
                    return (obj->GetSpotID() &&
                        oObj->GetSpotID() == obj->GetSpotID()) ||
                        (!obj->GetSpotID() && !oObj->GetSpotID() &&
                            fabs(oObj->GetX() - obj->GetX()) < 10.f &&
                            fabs(oObj->GetY() - obj->GetY()) < 10.f);
                });
        }

        // Removes supported via 0 ID
        if(obj->GetID())
        {
            objects.push_back(obj);
        }
    }
    zone->SetObjects(objects);

    // Update spawns
    for(auto& sPair : partial->GetSpawns())
    {
        zone->SetSpawns(sPair.first, sPair.second);
    }

    // Update spawn groups
    for(auto& sgPair : partial->GetSpawnGroups())
    {
        zone->SetSpawnGroups(sgPair.first, sgPair.second);
    }

    // Update spawn location groups
    for(auto& slgPair : partial->GetSpawnLocationGroups())
    {
        zone->SetSpawnLocationGroups(slgPair.first, slgPair.second);
    }

    // Update spots
    for(auto& spotPair : partial->GetSpots())
    {
        zone->SetSpots(spotPair.first, spotPair.second);
    }

    // Add triggers
    for(auto& trigger : partial->GetTriggers())
    {
        zone->AppendTriggers(trigger);
    }
}

bool ServerDataManager::LoadScripts(gsl::not_null<DataStore*> pDataStore,
    const libcomp::String& datastorePath,
    std::function<bool(ServerDataManager&,
        const libcomp::String&, const libcomp::String&)> handler)
{
    std::list<libcomp::String> files;
    std::list<libcomp::String> dirs;
    std::list<libcomp::String> symLinks;

    (void)pDataStore->GetListing(datastorePath, files, dirs, symLinks,
        true, true);

    for (auto path : files)
    {
        if (path.Matches("^.*\\.nut$"))
        {
            std::vector<char> data = pDataStore->ReadFile(path);
            if(!handler(*this, path, std::string(data.begin(), data.end())))
            {
                LOG_ERROR(libcomp::String("Failed to load script file: %1\n").Arg(path));
                return false;
            }

            LOG_DEBUG(libcomp::String("Loaded script file: %1\n").Arg(path));
        }
    }


    return true;
}

namespace libcomp
{
    template<>
    ScriptEngine& ScriptEngine::Using<ServerScript>()
    {
        if(!BindingExists("ServerScript", true))
        {
            Sqrat::Class<ServerScript> binding(mVM, "ServerScript");
            binding.Var("Name", &ServerScript::Name);
            binding.Var("Type", &ServerScript::Type);
            Bind<ServerScript>("ServerScript", binding);
        }

        return *this;
    }

    template<>
    bool ServerDataManager::LoadObject<objects::ServerZone>(const tinyxml2::XMLDocument& doc,
        const tinyxml2::XMLElement *objNode, DefinitionManager* definitionManager)
    {
        auto zone = std::shared_ptr<objects::ServerZone>(new objects::ServerZone);
        if(!zone->Load(doc, *objNode))
        {
            return false;
        }

        auto id = zone->GetID();
        auto dynamicMapID = zone->GetDynamicMapID();

        libcomp::String zoneStr = libcomp::String("%1%2")
            .Arg(id).Arg(id != dynamicMapID ? libcomp::String(" (%1)")
                .Arg(dynamicMapID) : "");

        bool isField = false;
        if(definitionManager)
        {
            auto def = definitionManager->GetZoneData(id);
            if(!def)
            {
                LOG_WARNING(libcomp::String("Skipping unknown zone: %1\n")
                    .Arg(zoneStr));
                return true;
            }

            isField = def->GetBasic()->GetType() == 2;
        }

        if(mZoneData.find(id) != mZoneData.end() &&
            mZoneData[id].find(dynamicMapID) != mZoneData[id].end())
        {
            LOG_ERROR(libcomp::String("Duplicate zone encountered: %1\n")
                .Arg(zoneStr));
            return false;
        }

        // Make sure spawns are valid
        if(definitionManager)
        {
            for(auto sPair : zone->GetSpawns())
            {
                if(definitionManager->GetDevilData(
                    sPair.second->GetEnemyType()) == nullptr)
                {
                    LOG_ERROR(libcomp::String("Invalid spawn enemy type"
                        " encountered in zone %1: %2\n").Arg(zoneStr)
                        .Arg(sPair.second->GetEnemyType()));
                    return false;
                }
                else if(sPair.second->GetBossGroup() &&
                    sPair.second->GetCategory() !=
                    objects::Spawn::Category_t::BOSS)
                {
                    LOG_ERROR(libcomp::String("Invalid spawn boss group"
                        " encountered in zone %1: %2\n").Arg(zoneStr)
                        .Arg(sPair.first));
                    return false;
                }
            }
        }

        for(auto sgPair : zone->GetSpawnGroups())
        {
            auto sg = sgPair.second;

            for(auto sPair : sg->GetSpawns())
            {
                if(!zone->SpawnsKeyExists(sPair.first))
                {
                    LOG_ERROR(libcomp::String("Invalid spawn group spawn ID"
                        " encountered in zone %1: %2\n").Arg(zoneStr)
                        .Arg(sPair.first));
                    return false;
                }
            }

            if(!ValidateActions(sg->GetDefeatActions(), libcomp::String(
               "Zone %1, SG %2 Defeat").Arg(zoneStr).Arg(sg->GetID()), false) ||
               !ValidateActions(sg->GetSpawnActions(), libcomp::String(
               "Zone %1, SG %2 Spawn").Arg(zoneStr).Arg(sg->GetID()), false))
            {
                return false;
            }
        }

        for(auto slgPair : zone->GetSpawnLocationGroups())
        {
            for(uint32_t sgID : slgPair.second->GetGroupIDs())
            {
                if(!zone->SpawnGroupsKeyExists(sgID))
                {
                    LOG_ERROR(libcomp::String("Invalid spawn location group"
                        " spawn group ID encountered in zone %1: %2\n")
                        .Arg(zoneStr).Arg(sgID));
                    return false;
                }
            }
        }

        mZoneData[id][dynamicMapID] = zone;

        if(isField)
        {
            mFieldZoneIDs.push_back(std::pair<uint32_t, uint32_t>(id,
                dynamicMapID));
        }

        for(auto npc : zone->GetNPCs())
        {
            if(!ValidateActions(npc->GetActions(), libcomp::String(
                "Zone %1, NPC %2").Arg(zoneStr).Arg(npc->GetID()), false))
            {
                return false;
            }
        }

        for(auto obj : zone->GetObjects())
        {
            if(!ValidateActions(obj->GetActions(), libcomp::String(
                "Zone %1, Object %2").Arg(zoneStr).Arg(obj->GetID()), false))
            {
                return false;
            }
        }

        for(auto& pPair : zone->GetPlasmaSpawns())
        {
            auto plasma = pPair.second;
            if(!ValidateActions(plasma->GetSuccessActions(), libcomp::String(
                "Zone %1, Plasma %2").Arg(zoneStr).Arg(pPair.first), false) ||
               !ValidateActions(plasma->GetFailActions(), libcomp::String(
                "Zone %1, Plasma %2").Arg(zoneStr).Arg(pPair.first), false))
            {
                return false;
            }
        }

        for(auto& spotPair : zone->GetSpots())
        {
            auto spot = spotPair.second;
            if(!ValidateActions(spot->GetActions(), libcomp::String(
                "Zone %1, Spot %2").Arg(zoneStr).Arg(spotPair.first), false) ||
               !ValidateActions(spot->GetLeaveActions(), libcomp::String(
                "Zone %1, Spot %2").Arg(zoneStr).Arg(spotPair.first), false))
            {
                return false;
            }
        }

        for(auto t : zone->GetTriggers())
        {
            if(!ValidateActions(t->GetActions(), libcomp::String(
                "Zone %1 trigger").Arg(zoneStr), TriggerIsAutoContext(t)))
            {
                return false;
            }
        }

        return true;
    }

    template<>
    bool ServerDataManager::LoadObject<objects::ServerZonePartial>(const tinyxml2::XMLDocument& doc,
        const tinyxml2::XMLElement *objNode, DefinitionManager* definitionManager)
    {
        (void)definitionManager;

        auto prt = std::shared_ptr<objects::ServerZonePartial>(new objects::ServerZonePartial);
        if(!prt->Load(doc, *objNode))
        {
            return false;
        }

        auto id = prt->GetID();
        if(mZonePartialData.find(id) != mZonePartialData.end())
        {
            LOG_ERROR(libcomp::String("Duplicate zone partial encountered: %1\n").Arg(id));
            return false;
        }

        if(id == 0)
        {
            // Warn about any unsupported parts (spawns are used for global
            // spawn skills)
            if(prt->DynamicMapIDsCount() || prt->NPCsCount() ||
                prt->ObjectsCount() || prt->SpotsCount())
            {
                LOG_WARNING("Direct global partial zone definitions specified"
                    " but will be ignored\n");
            }
        }
        else
        {
            // Make sure spawns are valid
            if(definitionManager)
            {
                for(auto sPair : prt->GetSpawns())
                {
                    if(definitionManager->GetDevilData(
                        sPair.second->GetEnemyType()) == nullptr)
                    {
                        LOG_ERROR(libcomp::String("Invalid spawn enemy type"
                            " encountered in zone partial %1: %2\n").Arg(id)
                            .Arg(sPair.second->GetEnemyType()));
                        return false;
                    }
                    else if(sPair.second->GetBossGroup() &&
                        sPair.second->GetCategory() !=
                        objects::Spawn::Category_t::BOSS)
                    {
                        LOG_ERROR(libcomp::String("Invalid spawn boss group"
                            " encountered in zone paritial %1: %2\n").Arg(id)
                            .Arg(sPair.first));
                        return false;
                    }
                }
            }

            if(prt->GetAutoApply())
            {
                for(uint32_t dynamicMapID : prt->GetDynamicMapIDs())
                {
                    mZonePartialMap[dynamicMapID].insert(id);
                }
            }
        }

        mZonePartialData[id] = prt;

        for(auto sgPair : prt->GetSpawnGroups())
        {
            auto sg = sgPair.second;
            if(!ValidateActions(sg->GetDefeatActions(), libcomp::String(
               "Partial %1, SG %2 Defeat").Arg(id).Arg(sg->GetID()), false) ||
               !ValidateActions(sg->GetSpawnActions(), libcomp::String(
               "Partial %1, SG %2 Spawn").Arg(id).Arg(sg->GetID()), false))
            {
                return false;
            }
        }

        for(auto npc : prt->GetNPCs())
        {
            if(!ValidateActions(npc->GetActions(), libcomp::String(
                "Partial %1, NPC %2").Arg(id).Arg(npc->GetID()), false))
            {
                return false;
            }
        }

        for(auto obj : prt->GetObjects())
        {
            if(!ValidateActions(obj->GetActions(), libcomp::String(
                "Partial %1, Object %2").Arg(id).Arg(obj->GetID()), false))
            {
                return false;
            }
        }

        for(auto& spotPair : prt->GetSpots())
        {
            auto spot = spotPair.second;
            if(!ValidateActions(spot->GetActions(), libcomp::String(
                "Partial %1, Spot %2").Arg(id).Arg(spotPair.first), false) ||
               !ValidateActions(spot->GetLeaveActions(), libcomp::String(
                "Partial %1, Spot %2").Arg(id).Arg(spotPair.first), false))
            {
                return false;
            }
        }

        for(auto t : prt->GetTriggers())
        {
            if(!ValidateActions(t->GetActions(), libcomp::String(
                "Partial %1 trigger").Arg(id), TriggerIsAutoContext(t)))
            {
                return false;
            }
        }

        return true;
    }

    template<>
    bool ServerDataManager::LoadObject<objects::Event>(const tinyxml2::XMLDocument& doc,
        const tinyxml2::XMLElement *objNode, DefinitionManager* definitionManager)
    {
        (void)definitionManager;

        auto event = objects::Event::InheritedConstruction(objNode->Attribute("name"));
        if(event == nullptr || !event->Load(doc, *objNode))
        {
            return false;
        }

        if(event->GetID().IsEmpty())
        {
            LOG_ERROR("Event with no ID encountered\n");
            return false;
        }

        auto id = std::string(event->GetID().C());
        if(mEventData.find(id) != mEventData.end())
        {
            LOG_ERROR(libcomp::String("Duplicate event encountered: %1\n").Arg(id));
            return false;
        }

        mEventData[id] = event;

        if(event->GetEventType() == objects::Event::EventType_t::PERFORM_ACTIONS)
        {
            auto e = std::dynamic_pointer_cast<objects::EventPerformActions>(event);
            if(e)
            {
                if(!ValidateActions(e->GetActions(), e->GetID(), false, true))
                {
                    return false;
                }
            }
        }

        return true;
    }

    template<>
    bool ServerDataManager::LoadObject<objects::ServerZoneInstance>(const tinyxml2::XMLDocument& doc,
        const tinyxml2::XMLElement *objNode, DefinitionManager* definitionManager)
    {
        auto inst = std::shared_ptr<objects::ServerZoneInstance>(new objects::ServerZoneInstance);
        if(!inst->Load(doc, *objNode))
        {
            return false;
        }

        auto id = inst->GetID();
        if(definitionManager && !definitionManager->GetZoneData(inst->GetLobbyID()))
        {
            LOG_WARNING(libcomp::String("Skipping zone instance with unknown lobby: %1\n")
                .Arg(inst->GetLobbyID()));
            return true;
        }

        // Zone and dynamic map IDs should be parallel lists
        size_t zoneIDCount = inst->ZoneIDsCount();
        if(zoneIDCount != inst->DynamicMapIDsCount())
        {
            LOG_ERROR(libcomp::String("Zone instance encountered with zone and dynamic"
                " map counts that do not match\n"));
            return false;
        }

        for(size_t i = 0; i < zoneIDCount; i++)
        {
            uint32_t zoneID = inst->GetZoneIDs(i);
            uint32_t dynamicMapID = inst->GetDynamicMapIDs(i);

            if(mZoneData.find(zoneID) == mZoneData.end() ||
                mZoneData[zoneID].find(dynamicMapID) == mZoneData[zoneID].end())
            {
                LOG_ERROR(libcomp::String("Invalid zone encountered for instance: %1 (%2)\n")
                    .Arg(zoneID).Arg(dynamicMapID));
                return false;
            }
        }

        if(mZoneInstanceData.find(id) != mZoneInstanceData.end())
        {
            LOG_ERROR(libcomp::String("Duplicate zone instance encountered: %1\n").Arg(id));
            return false;
        }

        mZoneInstanceData[id] = inst;

        return true;
    }

    template<>
    bool ServerDataManager::LoadObject<objects::ServerZoneInstanceVariant>(
        const tinyxml2::XMLDocument& doc, const tinyxml2::XMLElement *objNode,
        DefinitionManager* definitionManager)
    {
        (void)definitionManager;

        auto variant = objects::ServerZoneInstanceVariant::InheritedConstruction(
            objNode->Attribute("name"));
        if(!variant->Load(doc, *objNode))
        {
            return false;
        }

        auto id = variant->GetID();
        if(mZoneInstanceVariantData.find(id) != mZoneInstanceVariantData.end())
        {
            LOG_ERROR(libcomp::String("Duplicate zone instance variant"
                " encountered: %1\n").Arg(id));
            return false;
        }

        size_t timeCount = variant->TimePointsCount();
        switch(variant->GetInstanceType())
        {
        case objects::ServerZoneInstanceVariant::InstanceType_t::TIME_TRIAL:
            if(timeCount != 4)
            {
                LOG_ERROR(libcomp::String("Time trial zone instance variant"
                    " encountered without 4 time points specified: %1\n")
                    .Arg(id));
                return false;
            }
            break;
        case objects::ServerZoneInstanceVariant::InstanceType_t::PVP:
            if(timeCount != 2 && timeCount != 3)
            {
                LOG_ERROR(libcomp::String("PVP zone instance variant"
                    " encountered without 2 or 3 time points specified: %1\n")
                    .Arg(id));
                return false;
            }
            break;
        case objects::ServerZoneInstanceVariant::InstanceType_t::DEMON_ONLY:
            if(timeCount != 3 && timeCount != 4)
            {
                LOG_ERROR(libcomp::String("Demon only zone instance variant"
                    " encountered without 3 or 4 time points specified: %1\n")
                    .Arg(id));
                return false;
            }
            break;
        case objects::ServerZoneInstanceVariant::InstanceType_t::DIASPORA:
            if(timeCount != 2)
            {
                LOG_ERROR(libcomp::String("Diaspora zone instance variant"
                    " encountered without 2 time points specified: %1\n")
                    .Arg(id));
                return false;
            }
            break;
        case objects::ServerZoneInstanceVariant::InstanceType_t::MISSION:
            if(timeCount != 1)
            {
                LOG_ERROR(libcomp::String("Mission zone instance variant"
                    " encountered without time point specified: %1\n")
                    .Arg(id));
                return false;
            }
            break;
        case objects::ServerZoneInstanceVariant::InstanceType_t::PENTALPHA:
            if(variant->GetSubID() >= 5)
            {
                LOG_ERROR(libcomp::String("Pentalpha zone instance variant"
                    " encountered with invalid sub ID: %1\n")
                    .Arg(id));
                return false;
            }
            break;
        default:
            break;
        }

        auto pvpVar = std::dynamic_pointer_cast<objects::PvPInstanceVariant>(
            variant);
        if(pvpVar)
        {
            if(definitionManager && pvpVar->GetDefaultInstanceID() &&
                !VerifyPvPInstance(pvpVar->GetDefaultInstanceID(),
                    definitionManager))
            {
                return false;
            }

            if(!pvpVar->GetSpecialMode() && pvpVar->GetMatchType() !=
                objects::PvPInstanceVariant::MatchType_t::CUSTOM)
            {
                mStandardPvPVariantIDs[(uint8_t)pvpVar->GetMatchType()]
                    .insert(id);
            }
        }

        mZoneInstanceVariantData[id] = variant;

        return true;
    }

    template<>
    bool ServerDataManager::LoadObject<objects::ServerShop>(const tinyxml2::XMLDocument& doc,
        const tinyxml2::XMLElement *objNode, DefinitionManager* definitionManager)
    {
        (void)definitionManager;

        auto shop = std::shared_ptr<objects::ServerShop>(new objects::ServerShop);
        if(!shop->Load(doc, *objNode))
        {
            return false;
        }

        uint32_t id = (uint32_t)shop->GetShopID();
        if(mShopData.find(id) != mShopData.end())
        {
            LOG_ERROR(libcomp::String("Duplicate shop encountered: %1\n")
                .Arg(id));
            return false;
        }

        // Tab count cannot exceed max s8, apply lower arbitrary limit
        if(shop->TabsCount() > 100)
        {
            LOG_ERROR(libcomp::String("Shop with more than 100 tabs"
                " encountered: %1\n").Arg(id));
            return false;
        }

        mShopData[id] = shop;

        if(shop->GetType() == objects::ServerShop::Type_t::COMP_SHOP)
        {
            mCompShopIDs.push_back(id);
        }

        return true;
    }

    template<>
    bool ServerDataManager::LoadObject<objects::AILogicGroup>(const tinyxml2::XMLDocument& doc,
        const tinyxml2::XMLElement *objNode, DefinitionManager* definitionManager)
    {
        (void)definitionManager;

        auto grp = std::shared_ptr<objects::AILogicGroup>(new objects::AILogicGroup);
        if(!grp->Load(doc, *objNode))
        {
            return false;
        }

        uint16_t id = grp->GetID();
        if(mAILogicGroups.find(id) != mAILogicGroups.end())
        {
            LOG_ERROR(libcomp::String("Duplicate AI logic group entry"
                " encountered: %1\n").Arg(id));
            return false;
        }

        mAILogicGroups[id] = grp;

        return true;
    }

    template<>
    bool ServerDataManager::LoadObject<objects::DemonPresent>(const tinyxml2::XMLDocument& doc,
        const tinyxml2::XMLElement *objNode, DefinitionManager* definitionManager)
    {
        (void)definitionManager;

        auto present = std::shared_ptr<objects::DemonPresent>(new objects::DemonPresent);
        if(!present->Load(doc, *objNode))
        {
            return false;
        }

        uint32_t id = present->GetID();
        if(mDemonPresentData.find(id) != mDemonPresentData.end())
        {
            LOG_ERROR(libcomp::String("Duplicate demon present entry encountered: %1\n").Arg(id));
            return false;
        }

        mDemonPresentData[id] = present;

        return true;
    }

    template<>
    bool ServerDataManager::LoadObject<objects::DemonQuestReward>(const tinyxml2::XMLDocument& doc,
        const tinyxml2::XMLElement *objNode, DefinitionManager* definitionManager)
    {
        (void)definitionManager;

        auto reward = std::shared_ptr<objects::DemonQuestReward>(new objects::DemonQuestReward);
        if(!reward->Load(doc, *objNode))
        {
            return false;
        }

        uint32_t id = reward->GetID();
        if(mDemonQuestRewardData.find(id) != mDemonQuestRewardData.end())
        {
            LOG_ERROR(libcomp::String("Duplicate demon quest reward entry encountered: %1\n").Arg(id));
            return false;
        }

        mDemonQuestRewardData[id] = reward;

        return true;
    }

    template<>
    bool ServerDataManager::LoadObject<objects::DropSet>(const tinyxml2::XMLDocument& doc,
        const tinyxml2::XMLElement *objNode, DefinitionManager* definitionManager)
    {
        (void)definitionManager;

        auto dropSet = std::shared_ptr<objects::DropSet>(new objects::DropSet);
        if(!dropSet->Load(doc, *objNode))
        {
            return false;
        }

        uint32_t id = dropSet->GetID();
        uint32_t giftBoxID = dropSet->GetGiftBoxID();
        if(mDropSetData.find(id) != mDropSetData.end())
        {
            LOG_ERROR(libcomp::String("Duplicate drop set encountered: %1\n").Arg(id));
            return false;
        }

        if(giftBoxID)
        {
            if(mGiftDropSetLookup.find(giftBoxID) != mGiftDropSetLookup.end())
            {
                LOG_ERROR(libcomp::String("Duplicate drop set gift box ID"
                    " encountered: %1\n").Arg(giftBoxID));
                return false;
            }

            mGiftDropSetLookup[giftBoxID] = id;
        }

        mDropSetData[id] = dropSet;

        return true;
    }

    template<>
    bool ServerDataManager::LoadObject<objects::EnchantSetData>(const tinyxml2::XMLDocument& doc,
        const tinyxml2::XMLElement *objNode, DefinitionManager* definitionManager)
    {
        auto eSet = std::shared_ptr<objects::EnchantSetData>(new objects::EnchantSetData);
        if(!eSet->Load(doc, *objNode))
        {
            return false;
        }

        return definitionManager && definitionManager->RegisterServerSideDefinition(eSet);
    }

    template<>
    bool ServerDataManager::LoadObject<objects::EnchantSpecialData>(const tinyxml2::XMLDocument& doc,
        const tinyxml2::XMLElement *objNode, DefinitionManager* definitionManager)
    {
        auto eSpecial = std::shared_ptr<objects::EnchantSpecialData>(new objects::EnchantSpecialData);
        if(!eSpecial->Load(doc, *objNode))
        {
            return false;
        }

        return definitionManager && definitionManager->RegisterServerSideDefinition(eSpecial);
    }

    template<>
    bool ServerDataManager::LoadObject<objects::MiSItemData>(const tinyxml2::XMLDocument& doc,
        const tinyxml2::XMLElement *objNode, DefinitionManager* definitionManager)
    {
        auto sItem = std::shared_ptr<objects::MiSItemData>(new objects::MiSItemData);
        if(!sItem->Load(doc, *objNode))
        {
            return false;
        }

        return definitionManager && definitionManager->RegisterServerSideDefinition(sItem);
    }

    template<>
    bool ServerDataManager::LoadObject<objects::MiSStatusData>(const tinyxml2::XMLDocument& doc,
        const tinyxml2::XMLElement *objNode, DefinitionManager* definitionManager)
    {
        auto sStatus = std::shared_ptr<objects::MiSStatusData>(new objects::MiSStatusData);
        if(!sStatus->Load(doc, *objNode))
        {
            return false;
        }

        return definitionManager && definitionManager->RegisterServerSideDefinition(sStatus);
    }

    template<>
    bool ServerDataManager::LoadObject<objects::Tokusei>(const tinyxml2::XMLDocument& doc,
        const tinyxml2::XMLElement *objNode, DefinitionManager* definitionManager)
    {
        auto tokusei = std::shared_ptr<objects::Tokusei>(new objects::Tokusei);
        if(!tokusei->Load(doc, *objNode))
        {
            return false;
        }

        return definitionManager && definitionManager->RegisterServerSideDefinition(tokusei);
    }
}

bool ServerDataManager::LoadScript(const libcomp::String& path,
    const libcomp::String& source)
{
    ScriptEngine engine;
    engine.Using<ServerScript>();
    if(!engine.Eval(source))
    {
        LOG_ERROR(libcomp::String("Improperly formatted script encountered: %1\n")
            .Arg(path));
        return false;
    }

    auto root = Sqrat::RootTable(engine.GetVM());
    auto fDef = root.GetFunction("define");
    if(fDef.IsNull())
    {
        LOG_ERROR(libcomp::String("Invalid script encountered: %1\n").Arg(path));
        return false;
    }

    auto script = std::make_shared<ServerScript>();
    auto result = fDef.Evaluate<int>(script);
    if(!result || *result != 0 || script->Name.IsEmpty() || script->Type.IsEmpty())
    {
        LOG_ERROR(libcomp::String("Script is not properly defined: %1\n").Arg(path));
        return false;
    }

    script->Path = path;
    script->Source = source;

    if(script->Type.ToLower() == "ai")
    {
        if(mAIScripts.find(script->Name.C()) != mAIScripts.end())
        {
            LOG_ERROR(libcomp::String("Duplicate AI script encountered: %1\n")
                .Arg(script->Name.C()));
            return false;
        }

        fDef = root.GetFunction("prepare");
        if(fDef.IsNull())
        {
            LOG_ERROR(libcomp::String("AI script encountered"
                " with no 'prepare' function: %1\n").Arg(script->Name.C()));
            return false;
        }

        mAIScripts[script->Name.C()] = script;
    }
    else
    {
        if(mScripts.find(script->Name.C()) != mScripts.end())
        {
            LOG_ERROR(libcomp::String("Duplicate script encountered: %1\n")
                .Arg(script->Name.C()));
            return false;
        }

        // Check supported types here
        auto type = script->Type.ToLower();
        if(type == "eventcondition" || type == "eventbranchlogic")
        {
            fDef = root.GetFunction("check");
            if(fDef.IsNull())
            {
                LOG_ERROR(libcomp::String("Event conditional script encountered"
                    " with no 'check' function: %1\n")
                    .Arg(script->Name.C()));
                return false;
            }
        }
        else if(type == "actiontransform" || type == "eventtransform")
        {
            fDef = root.GetFunction("transform");
            if(fDef.IsNull())
            {
                LOG_ERROR(libcomp::String("Transform script encountered"
                    " with no 'transform' function: %1\n")
                    .Arg(script->Name.C()));
                return false;
            }

            fDef = root.GetFunction("prepare");
            if(!fDef.IsNull())
            {
                LOG_ERROR(libcomp::String("Transform script encountered"
                    " with reserved function name 'prepare': %1\n")
                    .Arg(script->Name.C()));
                return false;
            }
        }
        else if(type == "actioncustom")
        {
            fDef = root.GetFunction("run");
            if(fDef.IsNull())
            {
                LOG_ERROR(libcomp::String("Custom action script encountered"
                    " with no 'run' function: %1\n").Arg(script->Name.C()));
                return false;
            }
        }
        else if(type == "webgame")
        {
            fDef = root.GetFunction("start");
            if(fDef.IsNull())
            {
                LOG_ERROR(libcomp::String("Web game script encountered"
                    " with no 'start' function: %1\n").Arg(script->Name.C()));
                return false;
            }
        }
        else
        {
            LOG_ERROR(libcomp::String("Invalid script type encountered: %1\n")
                .Arg(script->Type.C()));
            return false;
        }

        mScripts[script->Name.C()] = script;
    }

    return true;
}

bool ServerDataManager::ValidateActions(const std::list<std::shared_ptr<
    objects::Action>>& actions, const libcomp::String& source,
    bool autoContext, bool inEvent)
{
    size_t current = 0;
    size_t count = actions.size();
    for(auto action : actions)
    {
        current++;

        if(current != count && !inEvent)
        {
            bool warn = false;
            switch(action->GetActionType())
            {
            case objects::Action::ActionType_t::ZONE_CHANGE:
                {
                    auto act = std::dynamic_pointer_cast<
                        objects::ActionZoneChange>(action);
                    warn = act && act->GetZoneID() != 0;
                }
                break;
            case objects::Action::ActionType_t::ZONE_INSTANCE:
                {
                    auto act = std::dynamic_pointer_cast<
                        objects::ActionZoneInstance>(action);
                    if(act)
                    {
                        switch(act->GetMode())
                        {
                        case objects::ActionZoneInstance::Mode_t::JOIN:
                        case objects::ActionZoneInstance::Mode_t::CLAN_JOIN:
                        case objects::ActionZoneInstance::Mode_t::TEAM_JOIN:
                        case objects::ActionZoneInstance::Mode_t::TEAM_PVP:
                            warn = true;
                            break;
                        default:
                            break;
                        }
                    }
                }
                break;
            default:
                break;
            }

            if(warn)
            {
                LOG_WARNING(libcomp::String("Zone change action encountered"
                    " mid-action set in a context outside of an event. This"
                    " can cause unexpected behavior for multi-channel setups."
                    " Move to the end of the set to avoid errors: %1\n")
                    .Arg(source));
            }
        }

        bool autoCtx = autoContext && (action->GetSourceContext() ==
            objects::Action::SourceContext_t::ENEMIES ||
            action->GetSourceContext() ==
            objects::Action::SourceContext_t::SOURCE);
        switch(action->GetActionType())
        {
        case objects::Action::ActionType_t::DELAY:
            {
                auto act = std::dynamic_pointer_cast<objects::ActionDelay>(
                    action);
                if(!ValidateActions(act->GetActions(), libcomp::String(
                    "%1 => Delay Actions").Arg(source), autoCtx))
                {
                    return false;
                }
            }
            break;
        case objects::Action::ActionType_t::SPAWN:
            {
                auto act = std::dynamic_pointer_cast<objects::ActionSpawn>(
                    action);
                if(!ValidateActions(act->GetDefeatActions(), libcomp::String(
                    "%1 => Defeat Actions").Arg(source), autoCtx))
                {
                    return false;
                }
            }
            break;
        case objects::Action::ActionType_t::ADD_REMOVE_ITEMS:
        case objects::Action::ActionType_t::DISPLAY_MESSAGE:
        case objects::Action::ActionType_t::GRANT_SKILLS:
        case objects::Action::ActionType_t::GRANT_XP:
        case objects::Action::ActionType_t::PLAY_BGM:
        case objects::Action::ActionType_t::PLAY_SOUND_EFFECT:
        case objects::Action::ActionType_t::SET_HOMEPOINT:
        case objects::Action::ActionType_t::SPECIAL_DIRECTION:
        case objects::Action::ActionType_t::STAGE_EFFECT:
        case objects::Action::ActionType_t::UPDATE_COMP:
        case objects::Action::ActionType_t::UPDATE_FLAG:
        case objects::Action::ActionType_t::UPDATE_LNC:
        case objects::Action::ActionType_t::UPDATE_QUEST:
        case objects::Action::ActionType_t::ZONE_CHANGE:
        case objects::Action::ActionType_t::ZONE_INSTANCE:
            if(autoCtx)
            {
                LOG_ERROR(libcomp::String("Non-player context with player"
                    " required action type %1 encountered: %2\n")
                    .Arg((int32_t)action->GetActionType()).Arg(source));
                return false;
            }
            break;
        case objects::Action::ActionType_t::ADD_REMOVE_STATUS:
        case objects::Action::ActionType_t::CREATE_LOOT:
        case objects::Action::ActionType_t::RUN_SCRIPT:
        case objects::Action::ActionType_t::SET_NPC_STATE:
        case objects::Action::ActionType_t::START_EVENT:
        case objects::Action::ActionType_t::UPDATE_POINTS:
        case objects::Action::ActionType_t::UPDATE_ZONE_FLAGS:
        default:
            // Nothing special needed
            break;
        }
    }

    return true;
}

bool ServerDataManager::TriggerIsAutoContext(
    const std::shared_ptr<objects::ServerZoneTrigger>& trigger)
{
    // Most triggers use auto-only contexts
    switch(trigger->GetTrigger())
    {
    case objects::ServerZoneTrigger::Trigger_t::ON_DEATH:
    case objects::ServerZoneTrigger::Trigger_t::ON_DIASPORA_BASE_CAPTURE:
    case objects::ServerZoneTrigger::Trigger_t::ON_FLAG_SET:
    case objects::ServerZoneTrigger::Trigger_t::ON_PVP_BASE_CAPTURE:
    case objects::ServerZoneTrigger::Trigger_t::ON_PVP_COMPLETE:
    case objects::ServerZoneTrigger::Trigger_t::ON_REVIVAL:
    case objects::ServerZoneTrigger::Trigger_t::ON_ZONE_IN:
    case objects::ServerZoneTrigger::Trigger_t::ON_ZONE_OUT:
        return false;
    default:
        return true;
    }
}

namespace libcomp
{
    template<>
    ScriptEngine& ScriptEngine::Using<ServerDataManager>()
    {
        if(!BindingExists("ServerDataManager"))
        {
            Sqrat::Class<ServerDataManager> binding(
                mVM, "ServerDataManager");
            Bind<ServerDataManager>("ServerDataManager", binding);

            // These are needed for some methods.
            Using<DefinitionManager>();

            binding
                .Func("LoadData", &ServerDataManager::LoadData)
                ; // Last call to binding
        }

        return *this;
    }
}

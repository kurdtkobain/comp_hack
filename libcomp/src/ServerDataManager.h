/**
 * @file libcomp/src/ServerDataManager.h
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

#ifndef LIBCOMP_SRC_SERVERDATAMANAGER_H
#define LIBCOMP_SRC_SERVERDATAMANAGER_H

// Standard C++14 Includes
#include <PushIgnore.h>
#include <gsl/gsl>
#include <PopIgnore.h>

// libcomp Includes
#include "CString.h"
#include "DataStore.h"
#include "Log.h"

// tinyxml2 Includes
#include "PushIgnore.h"
#include <tinyxml2.h>
#include "PopIgnore.h"

// Standard C++11 Includes
#include <set>
#include <unordered_map>

namespace objects
{
class Action;
class AILogicGroup;
class DemonPresent;
class DemonQuestReward;
class DropSet;
class Event;
class ServerShop;
class ServerZone;
class ServerZoneInstance;
class ServerZoneInstanceVariant;
class ServerZonePartial;
class ServerZoneTrigger;
}

namespace libcomp
{

class DefinitionManager;

/**
 * Container for AI script information.
 */
struct ServerScript
{
    String Name;
    String Path;
    String Source;
    String Type;
};

/**
 * Manager class responsible for loading server specific files such as
 * zones and script files.
 */
class ServerDataManager
{
public:
    /**
     * Create a new ServerDataManager.
     */
    ServerDataManager();

    /**
     * Clean up the ServerDataManager.
     */
    ~ServerDataManager();

    /**
     * Get a server zone by definition ID
     * @param id Definition ID of a zone to retrieve
     * @param dynamicMapID Dynamic map ID of the zone to retrieve
     * @param applyPartials If true, the definition will be re-instanced and
     *  have all self-applied ServerZonePartial definitions applied to it. If
     *  false, the normal definition will be returned.
     * @param extraPartialIDs If applying ServerZonePartial definitions, the
     *  IDs supplied will be loaded as well
     * @return Pointer to the server zone matching the specified id
     */
    const std::shared_ptr<objects::ServerZone> GetZoneData(uint32_t id,
        uint32_t dynamicMapID, bool applyPartials = false,
        std::set<uint32_t> extraPartialIDs = {});

    /**
     * Get all field zone pairs of zone IDs and dynamic map IDs configured
     * for the server
     * @return List of zone ID to dynamic map ID pairs of field zones
     */
    const std::list<std::pair<uint32_t, uint32_t>> GetFieldZoneIDs();

    /**
     * Get all server zone defintion IDs with corresponding dynamic map IDs
     * registered with the manager
     * @return Set of all zone definition IDs registered with the manager
     */
    const std::unordered_map<uint32_t, std::set<uint32_t>> GetAllZoneIDs();

    /**
     * Get a server zone instance by definition ID
     * @param id Definition ID of a zone instance to retrieve
     * @return Pointer to the server zone instance matching the specified id
     */
    const std::shared_ptr<objects::ServerZoneInstance> GetZoneInstanceData(
        uint32_t id);

    /**
     * Get all server zone instance defintion IDs registered with the manager
     * @return Set of all zone instance definition IDs registered with the manager
     */
    const std::set<uint32_t> GetAllZoneInstanceIDs();

    /**
     * Check if the supplied zone ID and dynamicMap ID exist in a specific
     * instance definition
     * @param instanceID Definition ID of a zone instance
     * @param zoneID Zone ID to check
     * @param dynamicMapID Dynamic Map ID to check
     * @return true if it exists, false if it does not
     */
    bool ExistsInInstance(uint32_t instanceID, uint32_t zoneID,
        uint32_t dynamicMapID);

    /**
     * Get a server zone instance variant by definition ID
     * @param id Definition ID of a zone instance variant to retrieve
     * @return Pointer to the server zone instance variant matching the specified id
     */
    const std::shared_ptr<objects::ServerZoneInstanceVariant>
        GetZoneInstanceVariantData(uint32_t id);

    /**
     * Get all standard PvP variant IDs associated to a specific PvP type
     * @param type PvP type
     * @return Set of all variant IDs associated to the type
     */
    std::set<uint32_t> GetStandardPvPVariantIDs(uint8_t type) const;

    /**
     * Verify if the supplied instance is valid for being a PvP variant
     * @param instanceID ID of the instance to check
     * @param definitionManager Pointer to the definition manager to use
     *  when checking information for the instance zones
     */
    bool VerifyPvPInstance(uint32_t instanceID,
        DefinitionManager* definitionManager);

    /**
     * Get a server zone partial by definition ID
     * @param id Definition ID of a zone partial to retrieve
     * @return Pointer to the server zone partial matching the specified id
     */
    const std::shared_ptr<objects::ServerZonePartial> GetZonePartialData(
        uint32_t id);

    /**
     * Get an event by definition ID
     * @param id Definition ID of an event to load
     * @return Pointer to the event matching the specified id
     */
    const std::shared_ptr<objects::Event> GetEventData(const libcomp::String& id);

    /**
     * Get a shop by definition ID
     * @param id Definition ID of a shop to load
     * @return Pointer to the shop matching the specified id
     */
    const std::shared_ptr<objects::ServerShop> GetShopData(uint32_t id);

    /**
     * Get a list of all COMP shop definition IDs
     * @return List of COMP shop definition IDs
     */
    std::list<uint32_t> GetCompShopIDs() const;

    /**
     * Get an AI logic group by definition ID
     * @param id Definition ID of an AI logic group
     * @return Pointer to the AI logic group matching the specified id
     */
    const std::shared_ptr<objects::AILogicGroup> GetAILogicGroup(uint16_t id);

    /**
     * Get a demon present entry by definition ID
     * @param id Definition ID of a demon present entry to load
     * @return Pointer to the demon present entry matching the specified id
     */
    const std::shared_ptr<objects::DemonPresent> GetDemonPresentData(uint32_t id);

    /**
     * Get all demon quest reward definitions
     * @return Map of all demon quest reward definitions by ID
     */
    std::unordered_map<uint32_t,
        std::shared_ptr<objects::DemonQuestReward>> GetDemonQuestRewardData();

    /**
     * Get a drop set by definition ID
     * @param id Definition ID of a drop set to load
     * @return Pointer to the drop set matching the specified id
     */
    const std::shared_ptr<objects::DropSet> GetDropSetData(uint32_t id);

    /**
     * Get a drop set by gift box ID
     * @param giftBoxID Gift box ID of a drop set to load
     * @return Pointer to the drop set matching the specified id
     */
    const std::shared_ptr<objects::DropSet> GetGiftDropSetData(
        uint32_t giftBoxID);

    /**
     * Get a miscellaneous script by name
     * @param name Name of the script to load
     * @return Pointer to the script definition
     */
    const std::shared_ptr<ServerScript> GetScript(const libcomp::String& name);

    /**
     * Get an AI script by name
     * @param name Name of the script to load
     * @return Pointer to the script definition
     */
    const std::shared_ptr<ServerScript> GetAIScript(const libcomp::String& name);

    /**
     * Load all server data defintions in the data store
     * @param pDataStore Pointer to the datastore to load binary files from
     * @param definitionManager Pointer to the definition manager which
     *  will be loaded with any server side definitions. Loading of these
     *  definitions will be skipped if this is null.
     * @return true on success, false on failure
     */
    bool LoadData(DataStore *pDataStore,
        DefinitionManager* definitionManager);

    /**
     * Load all script files in the specified datastore and return them in
     * a list
     * @param pDataStore Pointer to the datastore to use
     * @param path Path within the data store to load scripts from
     * @param success Output result designating success or failure
     * @param store If false, scripts loaded by this function will be unloaded
     *  after they are gathered for the return results
     * @return List of pointers to server script definitions
     */
    std::list<std::shared_ptr<ServerScript>> LoadScripts(DataStore *pDataStore,
        const libcomp::String& path, bool& success, bool store = true);

    /**
     * Apply modifications from a zone partial to an instanced zone definition.
     * Unique IDs and NPCs/objects in the same spot that already exist on the
     * definition will be replaced by the partial definition, including deletes.
     * If the zone definition supplied for this function matches the pointer
     * stored for the base definition, it will fail.
     * @param zone Pointer to a zone definition to modify
     * @param partialID ID of the zone partial definition to apply to the zone
     * @return true if the modifications were successful, false if they were
     *  not or if the supplied zone definition is not a copy of the original
     */
    bool ApplyZonePartial(std::shared_ptr<objects::ServerZone> zone,
        uint32_t partialID);

    /**
     * Apply modifications from a zone partial to an instanced zone definition.
     * Unique IDs and NPCs/objects in the same spot that already exist on the
     * definition will be replaced by the partial definition, including
     * deletes. The supplied zone being a copy must be checked before this
     * point.
     * @param zone Pointer to a zone definition to modify
     * @param partial Pointer to the zone partial definition to apply to the
     *  zone
     * @param positionReplace If true non-keyed objects at the same position
     *  will be replaced as we go. If false they will all be included.
     */
    static void ApplyZonePartial(std::shared_ptr<objects::ServerZone> zone,
        const std::shared_ptr<objects::ServerZonePartial>& partial,
        bool positionReplace);

private:
    /**
     * Get a server object by ID from the supplied map of the specified
     * key and value type
     * @param id ID of the server object to retrieve from the map
     * @param data Map of server objects to retrieve by ID
     * @return Pointer to the matching server object, null if doesn't exist
     */
    template <class K, class T>
    std::shared_ptr<T> GetObjectByID(K id,
        std::unordered_map<K, std::shared_ptr<T>>& data)
    {
        auto iter = data.find(id);
        if(iter != data.end())
        {
            return iter->second;
        }

        return nullptr;
    }

    /**
     * Load all objects from files in a datastore path
     * @param pDataStore Pointer to the datastore to use
     * @param datastorePath Path within the data store to load files from
     * @param definitionManager Pointer to the definition manager which
     *  will be loaded with any server side definitions
     * @param recursive If true sub-directories will be loaded from as well
     * @param fileOrPath If true and no files are found in the path, the path
     *  will be treated like a file path by appending ".xml"
     * @return true on success, false on failure
     */
    template <class T>
    bool LoadObjects(gsl::not_null<DataStore*> pDataStore,
        const libcomp::String& datastorePath,
        DefinitionManager* definitionManager, bool recursive, bool fileOrPath)
    {
        std::list<libcomp::String> files;
        std::list<libcomp::String> dirs;
        std::list<libcomp::String> symLinks;

        (void)pDataStore->GetListing(datastorePath, files, dirs, symLinks,
            recursive, true);

        bool loaded = false;
        for(auto path : files)
        {
            if(path.Matches("^.*\\.xml$"))
            {
                if(LoadObjectsFromFile<T>(pDataStore, path,
                    definitionManager))
                {
                    loaded = true;
                }
                else
                {
                    return false;
                }
            }
        }

        if(!loaded && fileOrPath)
        {
            // Attempt to load single file from modified path
            return LoadObjectsFromFile<T>(pDataStore, datastorePath + ".xml",
                definitionManager);
        }
        else
        {
            return true;
        }
    }

    /**
     * Load all objects from a specific file in a datastore path
     * @param pDataStore Pointer to the datastore to use
     * @param filePath File path within the data store to load from
     * @param definitionManager Pointer to the definition manager which
     *  will be loaded with any server side definitions
     * @return true on success, false on failure
     */
    template <class T>
    bool LoadObjectsFromFile(gsl::not_null<DataStore*> pDataStore,
        const libcomp::String& filePath,
        DefinitionManager* definitionManager = nullptr)
    {
        tinyxml2::XMLDocument objsDoc;

        std::vector<char> data = pDataStore->ReadFile(filePath);

        if(data.empty())
        {
            LOG_WARNING(libcomp::String("File does not exist or is"
                " empty: %1\n").Arg(filePath));
            return true;
        }

        if(tinyxml2::XML_SUCCESS !=
            objsDoc.Parse(&data[0], data.size()))
        {
            return false;
        }

        const tinyxml2::XMLElement *rootNode = objsDoc.RootElement();
        const tinyxml2::XMLElement *objNode = rootNode->FirstChildElement("object");

        while(nullptr != objNode)
        {
            if(!LoadObject<T>(objsDoc, objNode, definitionManager))
            {
                LOG_ERROR(libcomp::String("Failed to load XML file: %1\n")
                    .Arg(filePath));
                return false;
            }

            objNode = objNode->NextSiblingElement("object");
        }

        LOG_DEBUG(libcomp::String("Loaded XML file: %1\n").Arg(filePath));

        return true;
    }

    /**
     * Load an object of the templated type from an XML node
     * @param doc XML document being loaded from
     * @param objNode XML node being loaded from
     * @param definitionManager Pointer to the definition manager which
     *  will be loaded with any server side definitions
     * @return true on success, false on failure
     */
    template <class T>
    bool LoadObject(const tinyxml2::XMLDocument& doc, const tinyxml2::XMLElement *objNode,
        DefinitionManager* definitionManager = nullptr);

    /**
     * Load all script files in the specified datastore
     * @param pDataStore Pointer to the datastore to use
     * @param datastorePath Path within the data store to load scripts from
     * @param handler Function pointer to call upon successful load
     * @return true on success, false on failure
     */
    bool LoadScripts(gsl::not_null<DataStore*> pDataStore,
        const libcomp::String& datastorePath,
        std::function<bool(ServerDataManager&,
            const libcomp::String&, const libcomp::String&)> handler);

    /**
     * Store a successfully loaded (non-AI) script
     * @param path Path to the script file
     * @param source Source contained within the script file
     * @return true on success, false on failure
     */
    bool LoadScript(const libcomp::String& path, const libcomp::String& source);

    /**
     * Check for any issues in an action set and report any found in the logs
     * @param actions List of actions to validate
     * @param source Text descriptor of where the action is from
     * @param autoContext If true, player only actions should be handled
     *  differently
     * @param inEvent Specifies that the actions are a direct subset of an
     *  event (ex: EventPerformActions)
     * @return false if any critical errors were encountered
     */
    bool ValidateActions(const std::list<
        std::shared_ptr<objects::Action>>& actions,
        const libcomp::String& source, bool autoContext, bool inEvent = false);

    /**
     * Check if the supplied trigger starts in an auto-only context for actions
     * @param trigger Pointer to the trigger definition
     * @return true if the trigger is an auto-only type, false if it is not
     */
    bool TriggerIsAutoContext(
        const std::shared_ptr<objects::ServerZoneTrigger>& trigger);

    /// Map of server zone defintions by zone definition and dynamic map ID
    std::unordered_map<uint32_t, std::unordered_map<uint32_t,
        std::shared_ptr<objects::ServerZone>>> mZoneData;

    /// List of zone ID to dynamic map ID pairs of field zones
    std::list<std::pair<uint32_t, uint32_t>> mFieldZoneIDs;

    /// Map of server zone instance defintions by definition ID
    std::unordered_map<uint32_t, std::shared_ptr<
        objects::ServerZoneInstance>> mZoneInstanceData;

    /// Map of server zone instance variant defintions by definition ID
    std::unordered_map<uint32_t, std::shared_ptr<
        objects::ServerZoneInstanceVariant>> mZoneInstanceVariantData;

    std::unordered_map<uint8_t, std::set<uint32_t>> mStandardPvPVariantIDs;

    /// Map of server zone partial defintions by definition ID
    std::unordered_map<uint32_t, std::shared_ptr<
        objects::ServerZonePartial>> mZonePartialData;

    /// Map of auto apply server zone partial defintion IDs mapped to directly
    /// on the definition by dynamic map ID
    std::unordered_map<uint32_t, std::set<uint32_t>> mZonePartialMap;

    /// Map of events by definition ID
    std::unordered_map<std::string,
        std::shared_ptr<objects::Event>> mEventData;

    /// Map of shops by definition ID
    std::unordered_map<uint32_t,
        std::shared_ptr<objects::ServerShop>> mShopData;

    /// List of all COMP shop definition IDs
    std::list<uint32_t> mCompShopIDs;

    /// Map of AI logic groups by definition ID
    std::unordered_map<uint16_t,
        std::shared_ptr<objects::AILogicGroup>> mAILogicGroups;

    /// Map of demon present entries by definition ID
    std::unordered_map<uint32_t,
        std::shared_ptr<objects::DemonPresent>> mDemonPresentData;

    /// Map of demon quest reward entries by definition ID
    std::unordered_map<uint32_t,
        std::shared_ptr<objects::DemonQuestReward>> mDemonQuestRewardData;

    /// Map of drop sets by definition ID
    std::unordered_map<uint32_t,
        std::shared_ptr<objects::DropSet>> mDropSetData;

    /// Map of drop set defintion IDs by gift box ID
    std::unordered_map<uint32_t, uint32_t> mGiftDropSetLookup;

    /// Map of miscellaneous (non-AI) scripts by name
    std::unordered_map<std::string, std::shared_ptr<ServerScript>> mScripts;

    /// Map of AI scripts by name
    std::unordered_map<std::string, std::shared_ptr<ServerScript>> mAIScripts;
};

} // namspace libcomp

#endif // LIBCOMP_SRC_SERVERDATAMANAGER_H

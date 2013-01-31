/*
 * Copyright 2009-2012 Red Hat, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//condor includes
#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "condor_debug.h"
#include "condor_commands.h"
#include "hashkey.h"
#include "stl_string_utils.h"
#include "hashkey.h"
#include "../condor_collector.V6/collector.h"
#include "condor_sinful.h"

// C++ includes
// enable for debugging classad to ostream
// watch out for unistd clash
//#include <sstream>

//local includes
#include "CollectorObject.h"
#include "Collectables.h"
#include "AviaryConversionMacros.h"
#include "AviaryUtils.h"

using namespace std;
using namespace aviary::collector;
using namespace aviary::util;

CollectorObject* CollectorObject::m_instance = NULL;

CollectorObject::CollectorObject ()
{
    m_codec = new BaseCodec;
}

CollectorObject::~CollectorObject()
{
    delete m_codec;
}

CollectorObject* CollectorObject::getInstance()
{
    if (!m_instance) {
        m_instance = new CollectorObject();
    }
    return m_instance;
}

string 
CollectorObject::getPool() {
    return getPoolName();
}

// private to impl
template<class CollectablesT, class CollectableT>
bool updateCollectable(const ClassAd& ad, CollectablesT& collectables)
{
    string name;
    if (!ad.LookupString(ATTR_NAME,name)) {
        return false;
    }
    typename CollectablesT::iterator it = collectables.find(name);
    if (it == collectables.end()) {
        CollectableT* collectable = new CollectableT;
        collectable->update(ad);
        collectables.insert(make_pair(name,collectable));
        dprintf(D_FULLDEBUG, "Created new %s Collectable for '%s'\n",collectable->MyType.c_str(),collectable->Name.c_str());
    }
    else {
        CollectableT* collectable = (*it).second;
        collectable->update(ad);
        dprintf(D_FULLDEBUG, "Updated %s Collectable '%s'\n",collectable->MyType.c_str(),collectable->Name.c_str());
    }
    return true;
}

template<class CollectablesT>
bool invalidateCollectable(const ClassAd& ad, CollectablesT& collectables)
{
    string name;
    if (!ad.LookupString(ATTR_NAME,name)) {
        dprintf(D_ALWAYS, "Unknown Collectable name for invalidation\n");
        return false;
    }
    typename CollectablesT::iterator it = collectables.find(name);
    if (it == collectables.end()) {
        dprintf(D_ALWAYS, "No Collectable '%s' to invalidate\n",name.c_str());
        return false;
    }
    else {
        dprintf(D_FULLDEBUG, "Deleted %s Collectable for '%s'\n",(*it).second->MyType.c_str(),(*it).second->Name.c_str());
        delete (*it).second;
        collectables.erase(it);
    }
    return true;
}

// slots are special due to their dynamic/partitionable links
// and high volume
bool updateSlot(const ClassAd& ad, SlotMapType& slots)
{
    
    bool status = false;
    status = updateCollectable<SlotMapType,Slot>(ad, slots);
    //TODO: fix association with other slots
    //TODO: update birthdate map
    return status;
}

// masters like slots are high volume
bool updateMaster(const ClassAd& ad, MasterMapType& masters)
{
    bool status = false;
    status = updateCollectable<MasterMapType,Master>(ad, masters);
    //TODO: update birthdate map
    return status;
}

bool invalidateSlot(const ClassAd& ad, SlotMapType& slots)
{
    bool status = false;
    status = invalidateCollectable<SlotMapType>(ad, slots);
    //TODO: disassociate links to dynamic slots
    //TODO: update birthdate map
    return status;
}

bool invalidateMaster(const ClassAd& ad, MasterMapType& masters)
{
    bool status = false;
    status = invalidateCollectable<MasterMapType>(ad, masters);
    //TODO: update birthdate map
    return status;
}

// daemonCore methods
bool
CollectorObject::update(int command, const ClassAd& ad)
{
    bool status = false;
    switch (command) {
        case UPDATE_COLLECTOR_AD:
            status = updateCollectable<CollectorMapType,Collector>(ad, collectors);
            break;
        case UPDATE_MASTER_AD:
            status = updateMaster(ad, masters);
            break;
        case UPDATE_NEGOTIATOR_AD:
            status = updateCollectable<NegotiatorMapType,Negotiator>(ad, negotiators);
            break;
        case UPDATE_SCHEDD_AD:
            status = updateCollectable<SchedulerMapType,Scheduler>(ad, schedulers);
            break;
        case UPDATE_STARTD_AD:
            status = updateSlot(ad, slots);
            break;
        case UPDATE_SUBMITTOR_AD:
            status = updateCollectable<SubmitterMapType,Submitter>(ad, submitters);
            break;
        default:
            // fall through on unknown command
            break;
    }
    return status;
}

bool
CollectorObject::invalidate(int command, const ClassAd& ad)
{
    bool status = false;
    switch (command) {
        case INVALIDATE_COLLECTOR_ADS:
            status = invalidateCollectable<CollectorMapType>(ad, collectors);
            break;
        case INVALIDATE_MASTER_ADS:
            status = invalidateMaster(ad, masters);
            break;
        case INVALIDATE_NEGOTIATOR_ADS:
            status = invalidateCollectable<NegotiatorMapType>(ad, negotiators);
            break;
        case INVALIDATE_SCHEDD_ADS:
            status = invalidateCollectable<SchedulerMapType>(ad, schedulers);
            break;
        case INVALIDATE_STARTD_ADS:
            status = invalidateSlot(ad, slots);
            break;
        case INVALIDATE_SUBMITTOR_ADS:
            status = invalidateCollectable<SubmitterMapType>(ad, submitters);
            break;
        default:
            // fall through on unknown command
            break;
    }
    return status;
}

// private to impl
template<class CollectableMapT, class CollectableSetT>
void findCollectable(const string& name, bool grep, CollectableMapT& coll_map, CollectableSetT& coll_set)
{
    typename CollectableMapT::iterator it;
    if (!grep && !name.empty()) { // exact match
        it = coll_map.find(name);
        if (it != coll_map.end()) {
            coll_set.insert((*it).second);
        }
    }
    else // we are scanning for a partial name match
    {
        bool no_name = name.empty();
        for (it = coll_map.begin(); it != coll_map.end(); it++) {
            // we are scanning for a partial name match or 
            // grabbing all of them by virtue of no name supplied
            if (no_name || (string::npos != (*it).second->Name.find(name))) {
                coll_set.insert((*it).second);
            }
        }
    }
}

// RPC API methods
void
CollectorObject::findCollector(const string& name, bool grep, CollectorSetType& coll_set) 
{
    findCollectable<CollectorMapType,CollectorSetType>(name, grep, collectors, coll_set);
}

void
CollectorObject::findMaster(const string& name, bool grep, MasterSetType& master_set) 
{
    findCollectable<MasterMapType,MasterSetType>(name, grep, masters, master_set);
}

void
CollectorObject::findNegotiator(const string& name, bool grep, NegotiatorSetType& neg_set) 
{
    findCollectable<NegotiatorMapType,NegotiatorSetType>(name, grep, negotiators, neg_set);
}

void
CollectorObject::findScheduler(const string& name, bool grep, SchedulerSetType& schedd_set) 
{
    findCollectable<SchedulerMapType,SchedulerSetType>(name, grep, schedulers, schedd_set);
}

void
CollectorObject::findSlot(const string& name, bool grep, SlotSetType& slot_set) 
{
    findCollectable<SlotMapType,SlotSetType>(name, grep, slots, slot_set);
    // TODO: associations?
}

void
CollectorObject::findSubmitter(const string& name, bool grep, SubmitterSetType& subm_set) 
{
    findCollectable<SubmitterMapType,SubmitterSetType>(name, grep, submitters, subm_set);
}

bool
CollectorObject::findAttribute(AdTypes daemon_type, const string& name, const string& ip_addr, AttributeMapType& attr_map)
{
    // birdbath borrow
    CollectorEngine& engine = CollectorDaemon::getCollector();
    AdNameHashKey hash_key;
    // distill the daemon-specific hashkey using 
    // the crazy legacy logic :-(
    if (daemon_type == STARTD_AD || daemon_type == SCHEDD_AD) {
            Sinful sin_str(ip_addr.c_str());
            hash_key.name = name;
            hash_key.ip_addr = sin_str.getHost();
    }
    else {
        if (daemon_type == COLLECTOR_AD) {
            size_t found = name.rfind("@");
            if (found!=string::npos) {
                hash_key.name = name.substr(found+1,name.length());
            }
        }
        else {
            hash_key.name = name;
        }
        hash_key.ip_addr = "";
    }

    ClassAd* ad = NULL;
    ad = engine.lookup(daemon_type,hash_key);
    if (ad) {
        if (attr_map.empty()) {
            // load all the ad's attributes
            m_codec->classAdToMap(*ad,attr_map);
        }
        else {
            // pick off the attributes requested
            for (AttributeMapIterator it = attr_map.begin(); it!=attr_map.end(); it++) {
                m_codec->addAttributeToMap(*ad,(*it).first.c_str(),attr_map);
            }
        }
        return true;
    }

    dprintf(D_FULLDEBUG, "Unable to find Collectable ClassAd for type '%s' using '%s' and '%s'\n",
            AdTypeToString(daemon_type),hash_key.name.Value(),hash_key.ip_addr.Value());

    return false;
}

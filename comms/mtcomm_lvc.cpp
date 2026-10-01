// mtcomm_lvc.cpp                                                     -*-C++-*-
#include "mtcomm_lvc.h"

namespace mtcomm {

using mtcore::LockGuard;

CacheEntry::CacheEntry()
: updatedMs(0)
, updates(0)
, stale(false)
{
}

LastValueCache::LastValueCache()
: d_totalUpdates(0)
{
}

LastValueCache::~LastValueCache()
{
    for (EntryMap::iterator it = d_entries.begin();
         it != d_entries.end();
         ++it) {
        delete it->second;
    }
    d_entries.clear();
}

void LastValueCache::update(const std::string&     key,
                            const mtcore::Element& fields,
                            bool                   replace,
                            Int64                  nowMs)
{
    LockGuard guard(&d_mutex);
    CacheEntry *entry = NULL;
    EntryMap::iterator it = d_entries.find(key);
    if (it == d_entries.end()) {
        entry = new CacheEntry();
        d_entries[key] = entry;
        replace = true;
    }
    else {
        entry = it->second;
    }
    if (replace) {
        entry->image = fields;
    }
    else {
        entry->image.merge(fields);
    }
    entry->updatedMs = nowMs;
    entry->stale     = false;
    ++entry->updates;
    ++d_totalUpdates;
}

bool LastValueCache::get(mtcore::Element *image, const std::string& key) const
{
    bool stale = false;
    return get(image, &stale, key);
}

bool LastValueCache::get(mtcore::Element    *image,
                         bool               *stale,
                         const std::string&  key) const
{
    LockGuard guard(&d_mutex);
    EntryMap::const_iterator it = d_entries.find(key);
    if (it == d_entries.end()) {
        return false;
    }
    *image = it->second->image;
    *stale = it->second->stale;
    return true;
}

bool LastValueCache::remove(const std::string& key)
{
    LockGuard guard(&d_mutex);
    EntryMap::iterator it = d_entries.find(key);
    if (it == d_entries.end()) {
        return false;
    }
    delete it->second;
    d_entries.erase(it);
    return true;
}

void LastValueCache::markAllStale()
{
    LockGuard guard(&d_mutex);
    for (EntryMap::iterator it = d_entries.begin();
         it != d_entries.end();
         ++it) {
        it->second->stale = true;
    }
}

int LastValueCache::staleCount() const
{
    LockGuard guard(&d_mutex);
    int count = 0;
    for (EntryMap::const_iterator it = d_entries.begin();
         it != d_entries.end();
         ++it) {
        if (it->second->stale) {
            ++count;
        }
    }
    return count;
}

void LastValueCache::keys(std::vector<std::string> *result) const
{
    LockGuard guard(&d_mutex);
    for (EntryMap::const_iterator it = d_entries.begin();
         it != d_entries.end();
         ++it) {
        result->push_back(it->first);
    }
}

int LastValueCache::size() const
{
    LockGuard guard(&d_mutex);
    return (int)d_entries.size();
}

Uint64 LastValueCache::totalUpdates() const
{
    LockGuard guard(&d_mutex);
    return d_totalUpdates;
}

}  // close namespace mtcomm

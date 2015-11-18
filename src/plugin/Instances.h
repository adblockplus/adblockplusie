/*
 * This file is part of Adblock Plus <https://adblockplus.org/>,
 * Copyright (C) 2006-2016 Eyeo GmbH
 *
 * Adblock Plus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * Adblock Plus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Adblock Plus.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _INSTANCES_H_
#define _INSTANCES_H_

#include <mutex>
#include <map>

/**
 * A base class for a synchronized map from threads to BHO instances.
 *
 * This is a base class for 'CurrentThreadMap', defined in PluginClass.cpp.
 * It's separated out here for testability.
 * This class should not be used as polymorphic base class, as it has no virtual base class.
 *
 * The member functions here not simply forwarded versions of the container functions.
 * Rather, they are specialized for tracking BHO calls to SetSite().
 * Their semantics allow for verification that the call pattern is as expected.
 *
 * The key to the map (in the subclass) is the thread ID, thus operations are serialized on a per-key basis.
 * Calls to SetSite() bracket all other calls, so on a per-key basis
 *   the order of operations is always either (insert / find-success / erase) or (find-failure).
 * The library guarantees for std::map seem to indicate that operations on different keys
 *   do not interfer with each other, but there's some ambiguity there.
 * This class is synchronized as a matter of defensive programming.
 */
template<class Key, class T, T nullValue>
class SyncMap
{
  typedef std::lock_guard<std::mutex> SentryType;

  /**
   * Underlying map container
   */
  std::map<Key, T> idMap;

  /**
   * Synchronization primitive
   */
  mutable std::mutex mutex;

public:
  /**
   * Returns true if (as expected) no key of value 'id' was present.
   * Returns false otherwise.
   */
  bool AddIfAbsent(Key id, T p)
  {
    SentryType sentry(mutex);
    auto it = idMap.insert(std::make_pair(id, p));
    return it.second;
    // Assert it.second==true implies the insertion took place,
    //  which means there was no key of value 'id' already present.
  }

  /**
   * Returns true if (as expected) a key of value 'id' was already present.
   * Returns false otherwise.
   */
  bool RemoveIfPresent(Key id)
  {
    SentryType sentry(mutex);
    auto it = idMap.find(id);
    if (it == idMap.end())
    {
      return false;
    }
    idMap.erase(it);
    return true;
  }

  /**
   * Returns a non-nullValue if a key of value 'id' is present.
   * Returns nullValue otherwise.
   */
  T Locate(Key id) const
  {
    SentryType sentry(mutex);
    auto it = idMap.find(id);
    return (it != idMap.end()) ? it->second : nullValue;
  }
};

#endif

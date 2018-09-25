/*
  Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef FABRIC_CACHE_FABRIC_CACHE_INCLUDED
#define FABRIC_CACHE_FABRIC_CACHE_INCLUDED

#include "mysqlrouter/fabric_cache.h"
#include "fabric_factory.h"
#include "utils.h"
#include "mysql_router_thread.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <mutex>
#include <string>
#include <thread>

#include "mysql/harness/logging/logger.h"

using std::string;
using std::thread;
using fabric_cache::ManagedServer;

const int kDefaultTimeToLive = 10;

/** @class FabricCache
 *
 * The FabricCache manages cached information fetched from the
 * MySQL Server.
 *
 */
class FabricCache {

public:
  /** @brief Constructor */
  FabricCache(string host, int port, string user, string password,
              int connection_timeout, int connection_attempts);

  /** Starts the Fabric Cache
   *
   * Starts the Fabric Cache and launch thread.
   */
  void start();
  void stop() noexcept;
  static void* run_thread(void* context);
  void refresh_thread();

  /** @brief Returns list of managed servers in a group
   *
   * Returns list of managed servers in a group.
   *
   * @param group_id The ID of the group being looked up
   * @return std::list containing ManagedServer objects
   */
  list<ManagedServer> group_lookup(const string &group_id);

private:

  /** @brief Fetches all data from Fabric
   *
   * Fetches all data from Fabric and stores it internally.
   */
  void fetch_data();

  /** @brief Refreshes the cache
   *
   * Refreshes the cache.
   */
  void refresh();

  map<string, list<ManagedServer>> group_data_;
  std::chrono::milliseconds ttl_;

  map<string, list<ManagedServer>> group_data_temp_;

  bool terminate_;

  std::shared_ptr<FabricMetaData> fabric_meta_data_;

  mysql_harness::MySQLRouterThread refresh_thread_;

  std::mutex cache_refreshing_mutex_;
};

#endif // FABRIC_CACHE_FABRIC_CACHE_INCLUDED

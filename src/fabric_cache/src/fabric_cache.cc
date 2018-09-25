/*
  Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "common.h"
#include "fabric_cache.h"
#include "mysql/harness/logging/logging.h"

#include <list>
#include <memory>

IMPORT_LOG_FUNCTIONS()

std::map<ManagedServer::Mode, string> ManagedServer::ModeNames{
    {Mode::kOffline,   "offline"},
    {Mode::kReadOnly,  "read-only"},
    {Mode::kWriteOnly, "write-only"},
    {Mode::kReadWrite, "read-write"},
};

std::map<ManagedServer::Status, string> ManagedServer::StatusNames{
    {Status::kFaulty,      "faulty"},
    {Status::kSpare,       "spare"},
    {Status::kSecondary,   "secondary"},
    {Status::kPrimary,     "primary"},
    {Status::kConfiguring, "configuring"},
};

/**
 * Initialize a connection to the MySQL Fabric server.
 *
 * @param host The host on which the fabric server is running.
 * @param port The port number on which the fabric server is listening.
 * @param user The user name used to authenticate to the fabric server.
 * @param password The password used to authenticate to the fabric server.
 * @param fabric_connection_timeout The time after which a connection to the
 *                                  fabric server should timeout.
 * @param connection_attempts The number of times a connection to fabric must be
 *                            attempted, when a connection attempt fails.
 */
FabricCache::FabricCache(string host, int port, string user, string password,
                         int connection_timeout, int connection_attempts) {
  fabric_meta_data_ = get_instance(host, port, user, password,
                                   connection_timeout, connection_attempts);
  ttl_= std::chrono::milliseconds(3000);
  terminate_ = false;

  refresh();
}

void* FabricCache::run_thread(void* context) {
  FabricCache* fabric_cache = static_cast<FabricCache*>(context);
  fabric_cache->refresh_thread();
  return nullptr;
}

void FabricCache::refresh_thread() {
  mysql_harness::rename_thread("Fabric+Cache");

  while (!terminate_) {
    if (fabric_meta_data_->connect())
      refresh();
    else
      fabric_meta_data_->disconnect();
 
    // wait for up to TTL until next refresh, unless some replicaset loses an
    // online (primary or secondary) server - in that case, "emergency mode" is
    // enabled and we refresh every 1s until "emergency mode" is called off.
    if (terminate_) return;
    std::this_thread::sleep_for(ttl_);
  }
}

void FabricCache::start() {
  refresh_thread_.run(&run_thread, this);
}

/**
 * Stop the refresh thread.
 */
void FabricCache::stop() noexcept {
  terminate_ = true;
  refresh_thread_.join();
}

list<ManagedServer> FabricCache::group_lookup(const string &group_id) {
  std::lock_guard<std::mutex> lock(cache_refreshing_mutex_);
  auto group = group_data_.find(group_id);
  if (group == group_data_.end()) {
    log_warning("Fabric Group '%s' not available", group_id.c_str());
    return {};
  }
  list<ManagedServer> servers = group_data_[group_id];
  return servers;
}

void FabricCache::refresh() {
  try {
    fetch_data();
    cache_refreshing_mutex_.lock();
    group_data_ = group_data_temp_;
    cache_refreshing_mutex_.unlock();
  } catch (const fabric_cache::base_error &exc) {
    log_debug("Failed fetching data: %s", exc.what());
  }
}

void FabricCache::fetch_data() {
    group_data_temp_ = fabric_meta_data_->fetch_servers();
    ttl_ = std::chrono::milliseconds(fabric_meta_data_->fetch_ttl() * 1000);
}

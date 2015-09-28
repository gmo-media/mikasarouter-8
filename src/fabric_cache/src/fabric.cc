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

#include "fabric.h"

#include <chrono>
#include <cstdlib>
#include <errmsg.h>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>

#include <mysql.h>
#include "logger.h"

using std::ostringstream;

const int kConnectErrorReportInterval = 5; // consider connection timeout

Fabric::Fabric(const string &host, int port, const string &user,
               const string &password, int connection_timeout,
               int connection_attempts) {
  this->host_ = host;
  this->port_ = port;
  this->user_ = user;
  this->password_ = password;
  this->connection_timeout_ = connection_timeout;
  this->connection_attempts_ = connection_attempts;

  connect();
}

void Fabric::connect() noexcept {

  if (connected_ && mysql_ping(fabric_connection_) > 0) {
    return;
  }

  int attempts = 0;
  unsigned int protocol = MYSQL_PROTOCOL_TCP;
  bool reconnect = false;
  auto host = host_;

  if (host == "localhost") {
    host = "127.0.0.1";
  }

  while (true) {
    fabric_connection_ = mysql_init(nullptr);
    mysql_options(fabric_connection_, MYSQL_OPT_CONNECT_TIMEOUT, &connection_timeout_);
    mysql_options(fabric_connection_, MYSQL_OPT_PROTOCOL, reinterpret_cast<char *> (&protocol));
    mysql_options(fabric_connection_, MYSQL_OPT_RECONNECT, &reconnect);

    ++attempts;
    unsigned long client_flags = (
        CLIENT_LONG_PASSWORD | CLIENT_LONG_FLAG | CLIENT_PROTOCOL_41 | CLIENT_MULTI_RESULTS
    );
    if (mysql_real_connect(fabric_connection_, host.c_str(), user_.c_str(),
                           password_.c_str(), nullptr, static_cast<unsigned int>(port_), nullptr,
                           client_flags)) {
      break;
    }

    if (attempts % kConnectErrorReportInterval == 0 || attempts == 1) {
      log_error("Failed connecting to Fabric; will retry (%s)", mysql_error(fabric_connection_));
      attempts = 10;  // reset, but make sure this is not 1 otherwise we get double message
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  connected_ = true;
}

void Fabric::disconnect() noexcept {
  connected_ = false;
  if (fabric_connection_ != nullptr) {
    mysql_close(fabric_connection_);
  }
}

MYSQL_RES *Fabric::fetch_metadata(string &remote_api) {

  connect();

  int status = 0;
  ostringstream query;
  MYSQL_RES *result;
  MYSQL_ROW row = nullptr;

  query << "CALL " << remote_api << "()";
  status = mysql_query(fabric_connection_, query.str().c_str());
  if (status) {
    ostringstream ss;
    ss << "CALL statement failed: " << remote_api;
    throw fabric_cache::metadata_error(ss.str());
  }

  // The first result set returned by MySQL-RPC will always contain the
  // same information. The UUID of the Fabric Instance, the Time-To-Live
  // and a message such as errors.
  result = mysql_store_result(fabric_connection_);
  row = mysql_fetch_row(result);
  if (row != nullptr) {
    fabric_uuid_ = get_string(row[0]);
    ttl_ = atoi(row[1]);
    message_ = get_string(row[2]);
  }
  else {
    ostringstream ss;
    ss << "Failed fetching row: " << remote_api;
    throw fabric_cache::metadata_error(ss.str());
  }
  mysql_free_result(result);

  // If there are more result sets, fetch the next result set and extract
  // the dump information.
  if (mysql_more_results(fabric_connection_)) {
    status = mysql_next_result(fabric_connection_);
    // Fetching the next result set throws an error. Since the metadata result
    // set cannot be found, we cannot fetch the metadata.
    if (status > 0) {
      ostringstream ss;
      ss << "Failed fetching result: " << remote_api;
      throw fabric_cache::metadata_error(ss.str());
    }
    else if (status == -1) // If the metadata result set cannot be found.
    {
      ostringstream ss;
      ss << "Failed fetching next result: " << remote_api;
      throw fabric_cache::metadata_error(ss.str());
    }
    result = mysql_store_result(fabric_connection_);
    if (result) {
      return result;
    }
    else {
      ostringstream ss;
      ss << "Failed storing results: " << remote_api;
      throw fabric_cache::metadata_error(ss.str());
    }
  }
  else {
    ostringstream ss;
    ss << "Failed fetching multiple results: " << remote_api;
    throw fabric_cache::metadata_error(ss.str());
  }
}

/** @brief Returns relation between group ID and list of servers
 *
 * Returns relation as a std::map between group ID and list of managed servers.
 *
 * @return Map of group ID, server list pairs.
 */
map<string, list<ManagedServer>> Fabric::fetch_servers() {
  string api = "dump.servers";
  map<string, list<ManagedServer>> server_map;

  MYSQL_ROW row = nullptr;
  MYSQL_RES *result = fetch_metadata(api);

  while ((row = mysql_fetch_row(result)) != nullptr) {
    ManagedServer s;
    s.server_uuid = get_string(row[0]);
    s.group_id = get_string(row[1]);
    s.host = get_string(row[2]);
    s.port = atoi(row[3]);
    s.mode = atoi(row[4]);
    s.status = atoi(row[5]);
    s.weight = std::strtof(row[6], nullptr);

    server_map[s.group_id].push_back(s);
  }

  return server_map;
}

map<string, list<ManagedShard>> Fabric::fetch_shards() {
  string api = "dump.sharding_information";

  map<string, list<ManagedShard>> shard_map;

  MYSQL_ROW row = nullptr;
  MYSQL_RES *result = fetch_metadata(api);

  while ((row = mysql_fetch_row(result)) != nullptr) {
    ManagedShard sh;
    sh.schema_name = get_string(row[0]);
    sh.table_name = get_string(row[1]);
    sh.column_name = get_string(row[2]);
    sh.lb = get_string(row[3]);
    sh.shard_id = atoi(row[4]);
    sh.type_name = get_string(row[5]);
    sh.group_id = get_string(row[6]);
    sh.global_group = get_string(row[7]);

    ostringstream ss;
    ss << sh.schema_name << "." << sh.table_name;
    string fully_qualified_table_name = ss.str();
    shard_map[fully_qualified_table_name].push_back(sh);
  }

  return shard_map;
}

int Fabric::fetch_ttl() {
  return ttl_;
}
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

#ifndef MYSQLROUTER_FABRIC_CACHE_INCLUDED
#define MYSQLROUTER_FABRIC_CACHE_INCLUDED

#include <stdexcept>
#include <exception>
#include <list>
#include <map>
#include <string>

#include "mysqlrouter/utils.h"

#ifdef _WIN32
#  ifdef fabric_cache_STATIC
#    define FABRIC_CACHE_API
#  else
#    ifdef fabric_cache_EXPORTS
#      define FABRIC_CACHE_API __declspec(dllexport)
#    else
#      define FABRIC_CACHE_API __declspec(dllimport)
#    endif
#  endif
#else
#  define FABRIC_CACHE_API
#endif

using std::list;
using std::string;

namespace fabric_cache {

extern const uint16_t kDefaultFabricPort;
extern const string kDefaultFabricAddress;
extern const string kDefaultFabricUser;
extern const string kDefaultFabricPassword;

extern std::vector<string> g_fabric_cache_config_sections;

/** @class ManagedServer
 *
 * Class ManagedServer represents a server managed by MySQL Fabric.
 */
class FABRIC_CACHE_API ManagedServer {
public:
  /** @brief The UUID of the server registered with Fabric */
  string server_uuid;
  /** @brief The group ID of the group to which the server belongs */
  string group_id;
  /** @brief The host on which the server is running */
  string host;
  /** @brief The port number on which the mysql server is listening */
  int port;
  /** @brief The mode of the server */
  int mode;
  /** @brief The status of the server */
  int status;
  /** @brief The weight of the server */
  float weight;

  /**
   * Modes for managed servers
   */
  enum class Mode {
    kOffline = 0,
    kReadOnly = 1,
    kWriteOnly = 2,
    kReadWrite = 3,
  };

  /**
   * Statuses for managed servers
   */
  enum class Status {
    kFaulty = 0,
    kSpare = 1,
    kSecondary = 2,
    kPrimary = 3,
    kConfiguring = 4,
  };

  static std::map<Mode, string> ModeNames;
  static std::map<Status, string> StatusNames;
};

/** @class base_error
 *
 * Class base_error is base class for exceptions used by the Fabric Cache
 * module. It is derived from std::runtime_error.
 *
 */
class base_error : public std::runtime_error {
public:
  explicit base_error(const string &what_arg) : std::runtime_error(what_arg) { }
};

/** @class connection_error
 *
 * Class that represents all the exceptions thrown while trying to
 * connect with a Fabric node.
 *
 */
class connection_error : public base_error {
public:
  explicit connection_error(const string &what_arg) : base_error(what_arg) { }
};

/** @class metadata_error
 * Class that represents all the exceptions that are thrown while fetching the
 * metadata from MySQL Fabric.
 *
 */
class metadata_error : public base_error {
public:
  explicit metadata_error(const string &what_arg) : base_error(what_arg) { }
};

/** @class LookupResult
 *
 * Class holding result after looking up data in the cache.
 */
class FABRIC_CACHE_API LookupResult {
public:
  /** @brief Constructor */
  LookupResult(const list<ManagedServer> &server_list_) : server_list(server_list_) { }

  /** @brief List of ManagedServer objects */
  const list<ManagedServer> server_list;
};

/** @brief Initialize a FabricCache object and start caching
 *
 * The fabric_cache::cache_init function will initialize a FabricCache object
 * using the given arguments and store it globally using the given cache_name.
 *
 * Parameters host, port, user, password are used to setup the connection with
 * a MySQL Fabric node.
 *
 * Cache name given by cache_name can be empty, but must be unique.
 *
 * The parameters connection_timeout and connection_attempts are used when
 * connected to the MySQL Fabric node.
 *
 * Throws a fabric_cache::base_error when the cache object was already
 * initialized.
 *
 * @param cache_name Name of the cache object
 * @param host MySQL Fabric host IP or name (default 127.0.0.1)
 * @param port MySQL Fabric port (default 32275, MySQL-RPC)
 * @param user MySQL Fabric username
 * @param password MySQL Fabric password
 */
void FABRIC_CACHE_API cache_init(const string &cache_name, const string &host, const int port,
                const string &user,
                const string &password);
void FABRIC_CACHE_API cache_stop(const string &cache_name);
/** @brief Checks whether the given cache was initialized
 *
 * @param cache_name Name of the cache object
 * @return bool
 **/
bool FABRIC_CACHE_API have_cache(const string &cache_name);



/** @brief Returns list of managed server in a HA group
 *
 * Returns a list of MySQL server managed by MySQL Fabric for the given
 * HA group.
 *
 * @param cache_name Name of the Fabric Cache instance
 * @param group_id ID of the HA group
 * @return List of ManagedServer objects
 */
LookupResult FABRIC_CACHE_API lookup_group(const string &cache_name, const string &group_id);

} // namespace fabric_cache

#endif // MYSQLROUTER_FABRIC_CACHE_INCLUDED

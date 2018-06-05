/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <array>
#include <ctime>     // mktime, gmtime_r, gmtime_s
#include <cstring>   // memset
#include <cstdio>    // sscanf
#include <map>

#include <iostream>  // cerr

#include <event2/util.h>

#include "mysqlrouter/http_common.h"

int time_to_rfc5322_fixdate(time_t ts, char *date_buf, size_t date_buf_len) {
  struct tm t_m;

#ifdef _WIN32
  // returns a errno_t
  if (0 != gmtime_s(&t_m, &ts)) {
    return 0; // no bytes written to output
  }
#else
  if (nullptr == gmtime_r(&ts, &t_m)) {
    return 0; // no bytes written to output
  }
#endif

  constexpr std::array<const char *, 7> kDayNames { { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" } };

  constexpr std::array<const char *, 12> kMonthNames { { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" } };

  return evutil_snprintf(date_buf, date_buf_len, "%s, %02d %s %4d %02d:%02d:%02d GMT",
    kDayNames.at(t_m.tm_wday), t_m.tm_mday, kMonthNames.at(t_m.tm_mon),
    1900 + t_m.tm_year, t_m.tm_hour, t_m.tm_min, t_m.tm_sec);
}

static
time_t time_from_struct_tm_utc(struct tm *t_m) {
#if defined(_WIN32)
  return _mkgmtime(t_m);
#elif defined(__sun)
  // solaris, linux have typeof('timezone') == time_t
  return mktime(t_m) - timezone;
#else
  // linux, freebsd and apple have timegm()
  return timegm(t_m);
#endif
}



time_t time_from_rfc5322_fixdate(const char *date_buf) {
  // we can't use strptime as
  //
  // - it isn't portable
  // - takes locale into account, but we need en_EN all the time
  //
  // std::regex is broken on gcc-4.x
  // sscanf it is.
  //
  struct tm t_m;
  memset(&t_m, 0, sizeof(t_m));

  char wday[4];
  char mon[4];
  if (7 != sscanf(date_buf, "%3s, %2u %3s %4u %2u:%2u:%2u GMT",
        wday,
        &t_m.tm_mday,
        mon,
        &t_m.tm_year,
        &t_m.tm_hour,
        &t_m.tm_min,
        &t_m.tm_sec
        )) {
    throw std::runtime_error("invalid date");
  }

  // throws out-of-range
  std::map<std::string, decltype(t_m.tm_mon)> {
    { "Sun", 0 },
    { "Mon", 1 },
    { "Tue", 2 },
    { "Wed", 3 },
    { "Thu", 4 },
    { "Fri", 5 },
    { "Sat", 6 },
  }.at(wday);

  // throws out-of-range
  t_m.tm_mon = std::map<std::string, decltype(t_m.tm_mon)> {
    { "Jan", 0 },
    { "Feb", 1 },
    { "Mar", 2 },
    { "Apr", 3 },
    { "May", 4 },
    { "Jun", 5 },
    { "Jul", 6 },
    { "Aug", 7 },
    { "Sep", 8 },
    { "Oct", 9 },
    { "Nov", 10 },
    { "Dec", 11 },
  }.at(mon);
  t_m.tm_year -= 1900;

  return time_from_struct_tm_utc(&t_m);
}

bool is_modified_since(const HttpRequest &req, time_t last_modified) {
  auto req_hdrs = req.get_input_headers();

  auto *if_mod_since = req_hdrs.get("If-Modified-Since");
  if (if_mod_since != nullptr) {
    try {
      time_t if_mod_since_ts = time_from_rfc5322_fixdate(if_mod_since);

      if (!(last_modified > if_mod_since_ts)) {
        return false;
      }
    } catch (const std::exception &) {
      return false;
    }
  }
  return true;
}

bool add_last_modified(HttpRequest &req, time_t last_modified) {
  auto out_hdrs = req.get_output_headers();
  char date_buf[50];

  if (sizeof(date_buf) - time_to_rfc5322_fixdate(last_modified, date_buf, sizeof(date_buf)) > 0) {
    out_hdrs.add("Last-Modified", date_buf);

    return true;
  } else {
    return false;
  }
}


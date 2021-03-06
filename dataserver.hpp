// Nonolith Connect
// https://github.com/nonolith/connect
// Top level header
// Released under the terms of the GNU GPLv3+
// (C) 2012 Nonolith Labs, LLC
// Authors:
//   Kevin Mehall <km@kevinmehall.net>

#pragma once

#define INCLUDE_FROM_DATASERVER_HPP

#include <set>
#include <vector>
#include <string>
using std::string;

#include <boost/asio.hpp>
extern boost::asio::io_service io;

#include "libjson/libjson.h"
#include "websocketpp.hpp"
void respondJSON(websocketpp::session_ptr client, JSONNode &n, int status=200);
void respondError(websocketpp::session_ptr client, std::exception& e);

#include "event.hpp"
#include "device.hpp"

extern const char* const server_version;
extern const char* const server_git_version;

extern bool debugFlag;

extern std::set<device_ptr> devices;

extern Event device_list_changed;

void usb_init();
void usb_scan_devices();
void usb_thread_main();

#include "json_helpers.hpp"


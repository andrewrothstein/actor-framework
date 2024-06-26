// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/ipv6_subnet.hpp"

namespace caf {

/// An IP subnetwork. The address family is IPv6 unless `embeds_v4` returns
/// true.
using ip_subnet = ipv6_subnet;

} // namespace caf

// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/http/lower_layer.hpp"

#include "caf/net/http/context.hpp"
#include "caf/net/http/header_fields_map.hpp"
#include "caf/net/http/status.hpp"

#include <string>
#include <string_view>

using namespace std::literals;

namespace caf::net::http {

lower_layer::~lower_layer() {
  // nop
}

bool lower_layer::send_response(context ctx, status code,
                                std::string_view content_type,
                                const_byte_span content) {
  auto content_size = std::to_string(content.size());
  header_fields_map fields;
  fields.emplace("Content-Type"sv, content_type);
  fields.emplace("Content-Length"sv, content_size);
  return send_header(ctx, code, fields) && send_payload(ctx, content);
}

} // namespace caf::net::http
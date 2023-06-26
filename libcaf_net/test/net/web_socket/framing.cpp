// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE net.web_socket.framing

#include "caf/net/web_socket/framing.hpp"

#include "net-test.hpp"

using namespace caf;
using namespace std::literals;

namespace {

class app_t : public net::web_socket::upper_layer {
public:
  static auto make() {
    return std::make_unique<app_t>();
  }

  std::string text_input;

  caf::byte_buffer binary_input;

  error err;

  bool aborted = false;

  net::web_socket::lower_layer* down = nullptr;

  error start(net::web_socket::lower_layer* ll) override {
    down = ll;
    return none;
  }

  void prepare_send() override {
    // nop
  }

  bool done_sending() override {
    return true;
  }

  void abort(const error& reason) override {
    aborted = true;
    err = reason;
    down->shutdown(reason);
  }

  ptrdiff_t consume_text(std::string_view text) override {
    text_input.insert(text_input.end(), text.begin(), text.end());
    return static_cast<ptrdiff_t>(text.size());
  }

  ptrdiff_t consume_binary(byte_span bytes) override {
    binary_input.insert(binary_input.end(), bytes.begin(), bytes.end());
    return static_cast<ptrdiff_t>(bytes.size());
  }
};

struct fixture {
  app_t* app;
  net::web_socket::framing* uut;
  std::unique_ptr<mock_stream_transport> transport;

  fixture() {
    auto app_layer = app_t::make();
    app = app_layer.get();
    auto uut_layer
      = net::web_socket::framing::make_server(std::move(app_layer));
    uut = uut_layer.get();
    transport = mock_stream_transport::make(std::move(uut_layer));
    CHECK_EQ(transport->start(nullptr), error{});
    transport->configure_read(caf::net::receive_policy::up_to(2048u));
  }

  byte_buffer make_test_data(size_t requested_size) {
    return byte_buffer{requested_size, std::byte{0xFF}};
  }
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

SCENARIO("the client sends a ping and receives a pong response") {
  GIVEN("a valid WebSocket connection") {
    std::vector<std::byte> ping_frame;
    std::vector<std::byte> pong_frame;
    WHEN("the client sends a ping with no data") {
      auto data = make_test_data(0);
      detail::rfc6455::assemble_frame(detail::rfc6455::ping, 0x0, data,
                                      ping_frame);
      transport->push(ping_frame);
      CHECK_EQ(transport->handle_input(),
               static_cast<ptrdiff_t>(ping_frame.size()));
      THEN("the server responds with an empty pong") {
        detail::rfc6455::assemble_frame(detail::rfc6455::pong, 0x0, data,
                                        pong_frame);
        CHECK_EQ(transport->output_buffer(), pong_frame);
      }
    }
    transport->output_buffer().clear();
    WHEN("the client sends a ping with some data") {
      auto data = make_test_data(40);
      detail::rfc6455::assemble_frame(detail::rfc6455::ping, 0x0, data,
                                      ping_frame);
      transport->push(ping_frame);
      CHECK_EQ(transport->handle_input(),
               static_cast<ptrdiff_t>(ping_frame.size()));
      THEN("the server echoes the data back to the client") {
        detail::rfc6455::assemble_frame(detail::rfc6455::pong, 0x0, data,
                                        pong_frame);
        CHECK_EQ(transport->output_buffer(), pong_frame);
      }
    }
    transport->output_buffer().clear();
    WHEN("the client sends a ping with the maximum allowed 125 bytes") {
      auto data = make_test_data(125);
      detail::rfc6455::assemble_frame(detail::rfc6455::ping, 0x0, data,
                                      ping_frame);
      transport->push(ping_frame);
      CHECK_EQ(transport->handle_input(),
               static_cast<ptrdiff_t>(ping_frame.size()));
      THEN("the server echoes the data back to the client") {
        detail::rfc6455::assemble_frame(detail::rfc6455::pong, 0x0, data,
                                        pong_frame);
        CHECK_EQ(transport->output_buffer(), pong_frame);
      }
    }
  }
}

TEST_CASE("calling shutdown with protocol_error sets status in close header") {
  uut->shutdown(make_error(sec::protocol_error));
  detail::rfc6455::header hdr;
  detail::rfc6455::decode_header(transport->output_buffer(), hdr);
  CHECK_EQ(hdr.opcode, detail::rfc6455::connection_close);
  CHECK(hdr.payload_len >= 2);
  auto status = (std::to_integer<int>(transport->output_buffer().at(2)) << 8)
                + std::to_integer<int>(transport->output_buffer().at(3));
  CHECK_EQ(status, static_cast<int>(net::web_socket::status::protocol_error));
}

SCENARIO("the client sends an invalid ping that closes the connection") {
  GIVEN("a valid WebSocket connection") {
    std::vector<std::byte> ping_frame;
    WHEN("the client sends a ping with more data than allowed") {
      auto data = make_test_data(126);
      detail::rfc6455::assemble_frame(detail::rfc6455::ping, 0x0, data,
                                      ping_frame);
      transport->push(ping_frame);
      THEN("the server aborts the application") {
        CHECK_EQ(transport->handle_input(), 0);
        CHECK(app->aborted);
        CHECK_EQ(app->err, sec::protocol_error);
        MESSAGE("Aborted with: " << app->err);
      }
      AND("the server closes the connection with a protocol error") {
        detail::rfc6455::header hdr;
        detail::rfc6455::decode_header(transport->output_buffer(), hdr);
        MESSAGE("Buffer: " << transport->output_buffer());
        CHECK_EQ(hdr.opcode, detail::rfc6455::connection_close);
        CHECK(hdr.payload_len >= 2);
        auto status = (std::to_integer<int>(transport->output_buffer()[2]) << 8)
                      + std::to_integer<int>(transport->output_buffer()[3]);
        CHECK_EQ(status,
                 static_cast<int>(net::web_socket::status::protocol_error));
      }
    }
  }
}

END_FIXTURE_SCOPE()

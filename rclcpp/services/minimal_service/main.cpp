// Copyright 2016 Open Source Robotics Foundation, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cinttypes>
#include <memory>

#include "example_interfaces/srv/add_two_ints.hpp"
#include "rclcpp/rclcpp.hpp"

using AddTwoInts = example_interfaces::srv::AddTwoInts;

template<typename ServiceT>
class AsyncService : public rclcpp::Service<ServiceT>
{
  rclcpp::AnyServiceCallback<ServiceT> any_callback_;
  //using rclcpp::Service<ServiceT>::Service;

  void handle_request(std::shared_ptr<rmw_request_id_t> request_header,
                      std::shared_ptr<void> request) override
  {
    auto typed_request = std::static_pointer_cast<typename ServiceT::Request>(request);
    auto response = std::make_shared<typename ServiceT::Response>();
    any_callback_.dispatch(request_header, typed_request, response);
    this->send_response(*request_header, *response);
  }
public:
  AsyncService(std::shared_ptr<rcl_node_t> node_handle, const std::string & service_name, 
	       rclcpp::AnyServiceCallback<ServiceT> any_callback, rcl_service_options_t& service_options)
      : rclcpp::Service<ServiceT>::Service(node_handle, service_name, any_callback, service_options),
	any_callback_(any_callback)
  {}
};

class Node : public rclcpp::Node
{
  using rclcpp::Node::Node;

public:
  template<typename ServiceT, typename CallbackT>
  typename rclcpp::Service<ServiceT>::SharedPtr create_async_service(const std::string& service_name,
                                                      CallbackT&& callback)
  {
    auto qos_profile = rmw_qos_profile_services_default;
    auto group = nullptr;
    auto node_base = get_node_base_interface();
    auto node_services = get_node_services_interface();

    rclcpp::AnyServiceCallback<ServiceT> any_service_callback;
    any_service_callback.set(callback);
    rcl_service_options_t service_options = rcl_service_get_default_options();
    service_options.qos = qos_profile;

    auto serv = std::make_shared<AsyncService<ServiceT>>(
        node_base->get_shared_rcl_node_handle(),
	service_name,
	any_service_callback,
	service_options);

    auto serv_base_ptr = std::dynamic_pointer_cast<rclcpp::ServiceBase>(serv);
    node_services->add_service(serv_base_ptr, group);
    return serv;
  }
};

std::shared_ptr<Node> g_node = nullptr;

void handle_service(
  const std::shared_ptr<rmw_request_id_t> request_header,
  const std::shared_ptr<AddTwoInts::Request> request,
  const std::shared_ptr<AddTwoInts::Response> response)
{
  (void)request_header;
  RCLCPP_INFO(
    g_node->get_logger(),
    "request: %" PRId64 " + %" PRId64, request->a, request->b);
  response->sum = request->a + request->b;
}

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  g_node = std::make_shared<Node>("minimal_service");
  auto server = g_node->create_async_service<AddTwoInts>("add_two_ints", handle_service);
  rclcpp::spin(g_node);
  rclcpp::shutdown();
  g_node = nullptr;
  return 0;
}

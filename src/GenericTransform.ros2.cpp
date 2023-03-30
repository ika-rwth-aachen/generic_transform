/*
==============================================================================
MIT License

Copyright 2023 Institute for Automotive Engineering of RWTH Aachen University.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
==============================================================================
*/


#include <regex>

#include <generic_transform/GenericTransform.ros2.hpp>
#include <generic_transform/message_types.ros2.hpp>


namespace generic_transform {


const std::string GenericTransform::kInputTopic = "~/input";

const std::string GenericTransform::kOutputTopic = "~/transformed";

const std::string GenericTransform::kFrameIdParam = "frame_id";


GenericTransform::GenericTransform() : Node("generic_transform") {

  loadParameters();
  setup();
}


void GenericTransform::loadParameters() {

  rcl_interfaces::msg::ParameterDescriptor param_desc;
  param_desc.description = "Target frame ID to transform to";
  this->declare_parameter(kFrameIdParam, rclcpp::ParameterType::PARAMETER_STRING, param_desc);

  try {
    frame_id_ = this->get_parameter(kFrameIdParam).as_string();
  } catch (rclcpp::exceptions::ParameterUninitializedException&) {
    RCLCPP_FATAL(get_logger(), "Parameter '%s' is required", kFrameIdParam.c_str());
    exit(EXIT_FAILURE);
  }

  RCLCPP_INFO(
    this->get_logger(),
    "Transforming data on topic '%s' to frame '%s' published on topic '%s'",
    this->get_node_topics_interface()->resolve_topic_name(kInputTopic).c_str(),
    frame_id_.c_str(),
    this->get_node_topics_interface()->resolve_topic_name(kOutputTopic).c_str()
  );
}


void GenericTransform::setup() {

  // listen to tf
  tf_buffer_ = std::make_unique<tf2_ros::Buffer>(this->get_clock());
  tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);

  // setup timer to detect subscription type and then subscribe
  detect_message_type_timer_ =
    create_wall_timer(std::chrono::duration<double>(0.001),
                      std::bind(&GenericTransform::detectMessageType, this));
}


void GenericTransform::detectMessageType() {

  // check if topic to subscribe exists
  std::string resolved_input_topic = this->get_node_topics_interface()->resolve_topic_name(kInputTopic);
  const auto all_topics_and_types = get_topic_names_and_types();
  if (all_topics_and_types.count(resolved_input_topic)) {

    detect_message_type_timer_->cancel();
    msg_type_ = all_topics_and_types.at(resolved_input_topic)[0];
    RCLCPP_DEBUG(this->get_logger(), "Detected message type '%s'", msg_type_.c_str());

    // setup publisher with correct message type
    if (false) {}
#define MESSAGE_TYPE(TYPE, NAME)                                               \
    else if (msg_type_ == #NAME) {                                             \
      publisher_ = this->create_publisher<TYPE>(kOutputTopic, 10);             \
    }
#include "generic_transform/message_types.ros2.macro"
#undef MESSAGE_TYPE
    else {
      RCLCPP_ERROR_THROTTLE(this->get_logger(), *this->get_clock(), 1000,
        "Transforming message type '%s' is not supported",
        msg_type_.c_str());
      detect_message_type_timer_->reset();
      return;
    }

    // setup generic subscriber with correct message type (will have missed at least one message)
    subscriber_ = this->create_generic_subscription(
      kInputTopic,
      msg_type_,
      10,
      std::bind(&GenericTransform::transformGeneric, this, std::placeholders::_1)
    );

    RCLCPP_WARN(this->get_logger(),
      "Subscribed to message type '%s' on topic '%s'; first message may have been missed",
      msg_type_.c_str(),
      subscriber_->get_topic_name()
    );
  }
}


void GenericTransform::transformGeneric(const std::shared_ptr<rclcpp::SerializedMessage>& serialized_msg) {

  if (false) {}
#define MESSAGE_TYPE(TYPE, NAME)                                               \
  else if (msg_type_ == #NAME) {                                               \
                                                                               \
    /* instantiate generic message as message of concrete type */              \
    TYPE msg;                                                                  \
    rclcpp::Serialization<TYPE> serializer;                                    \
    serializer.deserialize_message(serialized_msg.get(), &msg);                \
                                                                               \
    /* pass message to transform callback */                                   \
    this->transform<TYPE>(msg);                                                \
  }
#include "generic_transform/message_types.ros2.macro"
#undef MESSAGE_TYPE
}


}  // namespace generic_transform


int main(int argc, char *argv[]) {

  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<generic_transform::GenericTransform>());
  rclcpp::shutdown();

  return 0;
}

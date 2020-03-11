// Copyright 2019 Apex.AI, Inc.
// Co-developed by Tier IV, Inc. and Apex.AI, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef LOCALIZATION_NODES__LOCALIZATION_NODE_HPP_
#define LOCALIZATION_NODES__LOCALIZATION_NODE_HPP_

#include <localization_common/localizer_base.hpp>
#include <localization_common/initialization.hpp>
#include <rclcpp/rclcpp.hpp>
#include <tf2/buffer_core.h>
#include <tf2_ros/transform_listener.h>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <time_utils/time_utils.hpp>
#include <helper_functions/message_adapters.hpp>
#include <localization_nodes/visibility_control.hpp>
#include <memory>
#include <string>
#include <utility>

namespace autoware
{
namespace localization
{
namespace localization_nodes
{
using common::helper_functions::message_field_adapters::get_frame_id;
using common::helper_functions::message_field_adapters::get_stamp;

/// Helper struct that groups topic name and QoS setting for a publisher or subscription
struct TopicQoS
{
  std::string topic;
  rclcpp::QoS qos;
};

/// Base relative localizer node that publishes map->base_link relative
/// transform messages for a given observation source and map.
/// \tparam ObservationMsgT Message type to register against a map.
/// \tparam MapMsgT Map type
/// \tparam LocalizerT Localizer type.
/// \tparam LocalizerConfigT Localizer configuration type.
/// \tparam PoseInitializerT Pose initializer type.
template<typename ObservationMsgT, typename MapMsgT, typename LocalizerT, typename LocalizerConfigT,
  typename PoseInitializerT>
class LOCALIZATION_NODES_PUBLIC RelativeLocalizerNode : public rclcpp::Node
{
public:
  using LocalizerBase = localization_common::RelativeLocalizerBase<ObservationMsgT, MapMsgT>;
  using LocalizerBasePtr = std::unique_ptr<LocalizerBase>;
  using Cloud = sensor_msgs::msg::PointCloud2;
  using PoseWithCovarianceStamped = typename LocalizerBase::PoseWithCovarianceStamped;

  /// Constructor
  /// \param node_name Name of node
  /// \param name_space Namespace of node
  /// \param observation_sub_config topic and QoS setting for the observation subscription.
  /// \param map_sub_config topic and QoS setting for the map subscription.
  /// \param pose_pub_config topic and QoS setting for the output pose publisher.
  /// \param pose_initializer Pose initializer.
  /// \param publish_tf Whether to publish to the `tf` topic. This can be used to publish transform
  /// messages when the relative localizer is the only source of localization.
  RelativeLocalizerNode(
    const std::string & node_name, const std::string & name_space,
    const TopicQoS & observation_sub_config,
    const TopicQoS & map_sub_config,
    const TopicQoS & pose_pub_config,
    const PoseInitializerT & pose_initializer,
    bool publish_tf = false
  )
  : Node(node_name, name_space),
    m_pose_initializer(pose_initializer),
    m_tf_listener(m_tf_buffer, std::shared_ptr<rclcpp::Node>(this, [](auto) {}), false),
    m_observation_sub(create_subscription<ObservationMsgT>(observation_sub_config.topic,
      observation_sub_config.qos,
      [this](typename ObservationMsgT::ConstSharedPtr msg) {observation_callback(msg);})),
    m_map_sub(create_subscription<MapMsgT>(map_sub_config.topic, map_sub_config.qos,
      [this](typename MapMsgT::ConstSharedPtr msg) {map_callback(msg);})),
    m_pose_publisher(create_publisher<PoseWithCovarianceStamped>(pose_pub_config.topic,
      pose_pub_config.qos)),
    m_tf_publisher(create_publisher<tf2_msgs::msg::TFMessage>("tf", pose_pub_config.qos)),
    m_publish_tf{publish_tf} {
  }

  /// Constructor using ros parameters
  /// \param node_name Node name
  /// \param name_space Node namespace
  /// \param pose_initializer Pose initializer
  RelativeLocalizerNode(
    const std::string & node_name, const std::string & name_space,
    const PoseInitializerT & pose_initializer)
  : Node(node_name, name_space),
    m_pose_initializer(pose_initializer),
    m_tf_listener(m_tf_buffer, std::shared_ptr<rclcpp::Node>(this, [](auto) {}), false),
    m_observation_sub(create_subscription<ObservationMsgT>(
        declare_parameter("observation_sub.topic").template get<std::string>(),
        rclcpp::QoS{rclcpp::KeepLast{
            static_cast<size_t>(declare_parameter("observation_sub.history_depth").template
            get<size_t>())}},
        [this](typename ObservationMsgT::ConstSharedPtr msg) {observation_callback(msg);})),
    m_map_sub(create_subscription<MapMsgT>(
        declare_parameter("map_sub.topic").template get<std::string>(),
        rclcpp::QoS{rclcpp::KeepLast{
            static_cast<size_t>(declare_parameter("map_sub.history_depth").
            template get<size_t>())}},
        [this](typename MapMsgT::ConstSharedPtr msg) {map_callback(msg);})),
    m_pose_publisher(create_publisher<PoseWithCovarianceStamped>(
        declare_parameter("pose_pub.topic").template get<std::string>(),
        rclcpp::QoS{rclcpp::KeepLast{
            static_cast<size_t>(declare_parameter(
              "pose_pub.history_depth").template get<size_t>())}})),
    m_tf_publisher(create_publisher<tf2_msgs::msg::TFMessage>("tf",
      rclcpp::QoS{rclcpp::KeepLast{static_cast<size_t>(declare_parameter(
            "pose_pub.history_depth").template get<size_t>())}})),
    m_publish_tf{declare_parameter("publish_tf").template get<bool>()}
  {}

  /// Get a const pointer of the output publisher. Can be used for matching against subscriptions.
  const typename rclcpp::Publisher<PoseWithCovarianceStamped>::ConstSharedPtr get_publisher()
  {
    return m_pose_publisher;
  }

protected:
  /// Set the localizer.
  /// \param localizer_ptr rvalue to the localizer to set.
  void set_localizer(LocalizerBasePtr && localizer_ptr)
  {
    m_localizer_ptr = std::forward<LocalizerBasePtr>(localizer_ptr);
  }

  /// Handle the exceptions during registration.
  virtual void on_bad_registration(std::exception_ptr eptr) // NOLINT
  {
    on_exception(eptr, "on_bad_registration");
  }

  /// Handle the exceptions during map setting.
  virtual void on_bad_map(std::exception_ptr eptr) // NOLINT
  {
    on_exception(eptr, "on_bad_map");
  }

  void on_exception(std::exception_ptr eptr, const std::string & error_source)  // NOLINT
  {
    try {
      if (eptr) {
        std::rethrow_exception(eptr);
      } else {
        RCLCPP_ERROR(get_logger(), error_source + ": error nullptr");
      }
    } catch (const std::exception & e) {
      RCLCPP_ERROR(get_logger(), e.what());
    }
  }

  /// Default behavior when an observation is received with no valid existing map.
  virtual void on_observation_with_invalid_map(typename ObservationMsgT::ConstSharedPtr)
  {
    RCLCPP_WARN(get_logger(), "Received observation without a valid map, "
      "ignoring the observation.");
  }

private:
  /// Callback that registers each received observation and outputs the result.
  /// \param msg_ptr Pointer to the observation message.
  void observation_callback(typename ObservationMsgT::ConstSharedPtr msg_ptr)
  {
    check_localizer();
    if (m_localizer_ptr->map_valid()) {
      try {
        const auto observation_time = ::time_utils::from_message(get_stamp(*msg_ptr));
        const auto & observation_frame = get_frame_id(*msg_ptr);
        const auto & map_frame = m_localizer_ptr->map_frame_id();
        const auto initial_guess =
          m_pose_initializer.guess(m_tf_buffer, observation_time, map_frame, observation_frame);
        const auto pose_out = m_localizer_ptr->register_measurement(*msg_ptr, initial_guess);
        m_pose_publisher->publish(pose_out);
        // This is to be used when no state estimator or alternative source of
        // localization is available.
        if (m_publish_tf) {
          publis_tf(pose_out);
        }
      } catch (...) {
        on_bad_registration(std::current_exception());
      }
    } else {
      on_observation_with_invalid_map(msg_ptr);
    }
  }

  /// Callback that updates the map.
  /// \param msg_ptr Pointer to the map message.
  void map_callback(typename MapMsgT::ConstSharedPtr msg_ptr)
  {
    check_localizer();
    try {
      m_localizer_ptr->set_map(*msg_ptr);
    } catch (...) {
      on_bad_map(std::current_exception());
    }
  }

  /// Publish the pose message as a transform.
  void publis_tf(const PoseWithCovarianceStamped & pose)
  {
    tf2_msgs::msg::TFMessage tf_message;
    geometry_msgs::msg::TransformStamped tf_stamped;
    tf_stamped.header = pose.header;
    tf_stamped.header.frame_id = m_localizer_ptr->map_frame_id();
    tf_stamped.child_frame_id = pose.header.frame_id;
    const auto & pose_trans = pose.pose.pose.position;
    const auto & pose_rot = pose.pose.pose.orientation;
    tf_stamped.transform.translation.set__x(pose_trans.x).set__y(pose_trans.y).set__z(pose_trans.z);
    tf_stamped.transform.rotation.set__x(pose_rot.x).set__y(pose_rot.y).set__z(pose_rot.z).set__w(
      pose_rot.w);
    tf_message.transforms.push_back(tf_stamped);
    m_tf_publisher->publish(tf_message);
  }

  /// Check if localizer exist, throw otherwise.
  void check_localizer() const
  {
    if (!m_localizer_ptr) {
      throw std::runtime_error("Localizer node needs a valid localizer to be set before it "
              "can register measurements. Call `set_localizer(...)` first.");
    }
  }

  LocalizerBasePtr m_localizer_ptr;
  PoseInitializerT m_pose_initializer;
  tf2::BufferCore m_tf_buffer;
  tf2_ros::TransformListener m_tf_listener;
  typename rclcpp::Subscription<ObservationMsgT>::SharedPtr m_observation_sub;
  typename rclcpp::Subscription<MapMsgT>::SharedPtr m_map_sub;
  typename rclcpp::Publisher<PoseWithCovarianceStamped>::SharedPtr m_pose_publisher;
  typename rclcpp::Publisher<tf2_msgs::msg::TFMessage>::SharedPtr m_tf_publisher;
  bool m_publish_tf;
};
}  // namespace localization_nodes
}  // namespace localization
}  // namespace autoware
#endif  // LOCALIZATION_NODES__LOCALIZATION_NODE_HPP_

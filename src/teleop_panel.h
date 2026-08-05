/*
 * Copyright (c) 2011, Willow Garage, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Willow Garage, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TELEOP_PANEL_H
#define TELEOP_PANEL_H

#ifndef Q_MOC_RUN
#include <memory>
#include <string>

#include "geometry_msgs/msg/twist.hpp"
#include "geometry_msgs/msg/twist_stamped.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rviz_common/panel.hpp"
#endif

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QTimer;
class QWidget;

namespace teleop_panel
{

class DriveWidget;

// Selects which message type the panel publishes velocity commands as.
enum class CommandMessageType
{
  Twist,
  TwistStamped
};

class TeleopPanel : public rviz_common::Panel
{
  Q_OBJECT

public:
  explicit TeleopPanel(QWidget * parent = nullptr);

  void load(const rviz_common::Config & config) override;
  void save(rviz_common::Config config) const override;

public Q_SLOTS:
  void setCmdVel(float linear_velocity, float angular_velocity);
  void setTopic(const QString & topic);

protected Q_SLOTS:
  void sendCmdVel();
  void updateTopic();
  void toggledEnabled(bool checked);
  void updateMessageType();
  void refreshTopicSuggestions();

protected:
  void updatePublishingState();
  void resetStopState();
  void recreatePublishers();
  bool hasActivePublisher() const;
  void applyInferredMessageTypeForTopic(const QString & topic);
  int inferMessageTypeComboIndexFromSubscribers(const QString & topic) const;
  std::string getExpectedCommandTopicType() const;
  QString getCurrentCommandTopic() const;

  DriveWidget * drive_widget_;
  QComboBox * command_topic_combo_;
  QCheckBox * enable_cmdvel_;
  QDoubleSpinBox * linear_spin_;
  QDoubleSpinBox * angular_spin_;
  QComboBox * message_type_combo_;
  QWidget * frame_id_row_;
  QLineEdit * frame_id_editor_;
  QTimer * output_timer_;
  QTimer * topic_refresh_timer_;

  QString output_topic_;

  std::shared_ptr<rclcpp::Node> velocity_node_;
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr twist_pub_;
  rclcpp::Publisher<geometry_msgs::msg::TwistStamped>::SharedPtr twist_stamped_pub_;

  CommandMessageType message_type_;

  float linear_velocity_;
  float angular_velocity_;
  bool enabled_;
  bool stop_sent_;
};

}  // namespace teleop_panel

#endif  // TELEOP_PANEL_H

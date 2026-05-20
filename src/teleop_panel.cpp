#include "teleop_panel.h"

#include <cmath>

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include "pluginlib/class_list_macros.hpp"

#include "drive_widget.h"

namespace teleop_panel
{

TeleopPanel::TeleopPanel(QWidget * parent)
: rviz_common::Panel(parent),
  message_type_(CommandMessageType::Twist),
  linear_velocity_(0.0f),
  angular_velocity_(0.0f),
  enabled_(false),
  stop_sent_(false)
{
  // Editable combo box: suggests live topics but the user can always type any
  // topic name manually (NoInsert keeps typed text out of the suggestion list).
  QHBoxLayout * topic_layout = new QHBoxLayout;
  topic_layout->addWidget(new QLabel("Command topic:"));
  command_topic_combo_ = new QComboBox;
  command_topic_combo_->setEditable(true);
  command_topic_combo_->setInsertPolicy(QComboBox::NoInsert);
  command_topic_combo_->setDuplicatesEnabled(false);
  topic_layout->addWidget(command_topic_combo_);

  // Lets the user pick which message type velocity commands are published as.
  QHBoxLayout * message_type_layout = new QHBoxLayout;
  message_type_layout->addWidget(new QLabel("Message type:"));
  message_type_combo_ = new QComboBox;
  message_type_combo_->addItem("Twist");
  message_type_combo_->addItem("TwistStamped");
  message_type_combo_->setCurrentIndex(0);
  message_type_layout->addWidget(message_type_combo_);

  // Self-contained row so it can be shown/hidden as a single unit.
  // Only relevant (and only visible) when publishing TwistStamped.
  frame_id_row_ = new QWidget;
  QHBoxLayout * frame_id_layout = new QHBoxLayout(frame_id_row_);
  frame_id_layout->setContentsMargins(0, 0, 0, 0);
  frame_id_layout->addWidget(new QLabel("Frame id:"));
  frame_id_editor_ = new QLineEdit("base_link");
  frame_id_layout->addWidget(frame_id_editor_);
  frame_id_row_->setVisible(false);

  drive_widget_ = new DriveWidget;

  QVBoxLayout * control_layout = new QVBoxLayout;
  enable_cmdvel_ = new QCheckBox("Enabled");
  control_layout->addWidget(enable_cmdvel_);

  QHBoxLayout * linear_layout = new QHBoxLayout;
  linear_layout->addWidget(new QLabel("Max linear:"));
  linear_spin_ = new QDoubleSpinBox;
  linear_spin_->setDecimals(2);
  linear_spin_->setRange(0.0, 99.0);
  linear_spin_->setSingleStep(0.1);
  linear_spin_->setValue(1.0);
  linear_layout->addWidget(linear_spin_);

  QHBoxLayout * angular_layout = new QHBoxLayout;
  angular_layout->addWidget(new QLabel("Max angular:"));
  angular_spin_ = new QDoubleSpinBox;
  angular_spin_->setDecimals(2);
  angular_spin_->setRange(0.0, 99.0);
  angular_spin_->setSingleStep(0.1);
  angular_spin_->setValue(1.0);
  angular_layout->addWidget(angular_spin_);

  control_layout->addLayout(linear_layout);
  control_layout->addLayout(angular_layout);

  QHBoxLayout * teleop_layout = new QHBoxLayout;
  teleop_layout->addWidget(drive_widget_);
  teleop_layout->addLayout(control_layout);

  QVBoxLayout * layout = new QVBoxLayout;
  layout->addLayout(topic_layout);
  layout->addLayout(message_type_layout);
  layout->addWidget(frame_id_row_);
  layout->addLayout(teleop_layout);
  setLayout(layout);

  output_timer_ = new QTimer(this);

  connect(
    drive_widget_, SIGNAL(outputVelocity(float, float)),
    this, SLOT(setCmdVel(float, float)));
  // editingFinished() fires on Enter/focus-out; activated(int) fires when the
  // user picks a suggestion. Neither fires on programmatic combo box updates.
  connect(
    command_topic_combo_->lineEdit(), SIGNAL(editingFinished()),
    this, SLOT(updateTopic()));
  connect(command_topic_combo_, SIGNAL(activated(int)), this, SLOT(updateTopic()));
  connect(output_timer_, SIGNAL(timeout()), this, SLOT(sendCmdVel()));
  connect(enable_cmdvel_, SIGNAL(toggled(bool)), this, SLOT(toggledEnabled(bool)));
  connect(
    message_type_combo_, SIGNAL(currentIndexChanged(int)),
    this, SLOT(updateMessageType()));

  drive_widget_->setEnabled(false);
  output_timer_->stop();

  velocity_node_ = std::make_shared<rclcpp::Node>("teleop_panel_velocity_node");

  // Periodically refresh the topic suggestions, plus once right now.
  topic_refresh_timer_ = new QTimer(this);
  connect(topic_refresh_timer_, SIGNAL(timeout()), this, SLOT(refreshTopicSuggestions()));
  refreshTopicSuggestions();
  topic_refresh_timer_->start(5000);
}

void TeleopPanel::setCmdVel(float lin, float ang)
{
  constexpr float drive_widget_max_linear = 10.0f;
  constexpr float drive_widget_max_angular = 2.0f;

  linear_velocity_ = lin * std::abs(static_cast<float>(linear_spin_->value()) / drive_widget_max_linear);
  angular_velocity_ = ang * std::abs(static_cast<float>(angular_spin_->value()) / drive_widget_max_angular);

  if (linear_velocity_ != 0.0f || angular_velocity_ != 0.0f) {
    stop_sent_ = false;
  }
}

void TeleopPanel::updateTopic()
{
  setTopic(getCurrentCommandTopic());
}

QString TeleopPanel::getCurrentCommandTopic() const
{
  return command_topic_combo_->currentText();
}

std::string TeleopPanel::getExpectedCommandTopicType() const
{
  return message_type_ == CommandMessageType::TwistStamped
    ? "geometry_msgs/msg/TwistStamped"
    : "geometry_msgs/msg/Twist";
}

void TeleopPanel::refreshTopicSuggestions()
{
  // The dropdown holds live filtered suggestions only -- never a history of
  // previously typed or selected topics.
  //
  // A topic is suggested only when some node *subscribes* to it with the
  // selected message type. Keying the filter on subscribers (the consumers)
  // instead of on the topic's advertised type is deliberate: this panel's own
  // publisher also advertises the topic, so a type-based filter would make the
  // currently selected topic "follow" every message type change and never drop
  // off the list. Subscribers are unaffected by this panel's publisher.
  const std::string expected_type = getExpectedCommandTopicType();

  QStringList suggestions;
  for (const auto & topic_entry : velocity_node_->get_topic_names_and_types()) {
    const std::string & name = topic_entry.first;
    for (const auto & sub : velocity_node_->get_subscriptions_info_by_topic(name)) {
      if (sub.topic_type() == expected_type) {
        suggestions << QString::fromStdString(name);
        break;
      }
    }
  }
  suggestions.sort();
  suggestions.removeDuplicates();

  // Always rebuild the combo box from scratch. Clearing unconditionally -- with
  // no "skip if unchanged" optimization -- guarantees that no stale item from a
  // previous message type can survive a refresh or a message type change.
  // Signals are blocked so this programmatic update cannot recreate publishers;
  // the user's typed text is restored via setEditText() and is never added as a
  // dropdown item.
  const QString current_text = command_topic_combo_->currentText();
  const QSignalBlocker blocker(command_topic_combo_);
  command_topic_combo_->clear();
  command_topic_combo_->addItems(suggestions);
  command_topic_combo_->setEditText(current_text);
}

void TeleopPanel::setTopic(const QString & new_topic)
{
  if (new_topic == output_topic_) {
    return;
  }

  output_topic_ = new_topic;
  recreatePublishers();

  updatePublishingState();
  Q_EMIT configChanged();
}

void TeleopPanel::updateMessageType()
{
  message_type_ = message_type_combo_->currentIndex() == 1
    ? CommandMessageType::TwistStamped
    : CommandMessageType::Twist;

  // The frame id only applies to TwistStamped, so hide its row otherwise.
  frame_id_row_->setVisible(message_type_ == CommandMessageType::TwistStamped);

  recreatePublishers();
  updatePublishingState();

  // Topic suggestions are filtered by message type, so refresh them now.
  refreshTopicSuggestions();

  Q_EMIT configChanged();
}

void TeleopPanel::recreatePublishers()
{
  // Keep only the publisher for the currently selected message type alive,
  // both following the configured topic name.
  twist_pub_.reset();
  twist_stamped_pub_.reset();

  if (output_topic_.isEmpty()) {
    return;
  }

  if (message_type_ == CommandMessageType::TwistStamped) {
    twist_stamped_pub_ = velocity_node_->create_publisher<geometry_msgs::msg::TwistStamped>(
      output_topic_.toStdString(), 1);
  } else {
    twist_pub_ = velocity_node_->create_publisher<geometry_msgs::msg::Twist>(
      output_topic_.toStdString(), 1);
  }
}

bool TeleopPanel::hasActivePublisher() const
{
  return message_type_ == CommandMessageType::TwistStamped
    ? twist_stamped_pub_ != nullptr
    : twist_pub_ != nullptr;
}

void TeleopPanel::sendCmdVel()
{
  if (!enabled_ || !rclcpp::ok() || !hasActivePublisher()) {
    return;
  }

  const bool stopped = linear_velocity_ == 0.0f && angular_velocity_ == 0.0f;
  if (stopped && stop_sent_) {
    return;
  }

  // Compute the velocity command once and reuse it for either message type.
  geometry_msgs::msg::Twist twist;
  twist.linear.x = linear_velocity_;
  twist.angular.z = angular_velocity_;

  if (message_type_ == CommandMessageType::TwistStamped) {
    geometry_msgs::msg::TwistStamped stamped;
    stamped.header.stamp = velocity_node_->now();
    stamped.header.frame_id = frame_id_editor_->text().toStdString();
    stamped.twist = twist;
    twist_stamped_pub_->publish(stamped);
  } else {
    twist_pub_->publish(twist);
  }

  stop_sent_ = stopped;
}

void TeleopPanel::toggledEnabled(bool checked)
{
  enabled_ = checked;

  if (!enabled_) {
    resetStopState();
  }

  updatePublishingState();
}

void TeleopPanel::updatePublishingState()
{
  const bool can_publish = enabled_ && !output_topic_.isEmpty() && hasActivePublisher();

  drive_widget_->setEnabled(can_publish);

  if (can_publish) {
    if (!output_timer_->isActive()) {
      output_timer_->start(100);
    }
  } else {
    output_timer_->stop();
  }
}

void TeleopPanel::resetStopState()
{
  linear_velocity_ = 0.0f;
  angular_velocity_ = 0.0f;
  stop_sent_ = false;
}

void TeleopPanel::save(rviz_common::Config config) const
{
  rviz_common::Panel::save(config);
  config.mapSetValue("Topic", getCurrentCommandTopic());
  config.mapSetValue("Enabled", enabled_);
  config.mapSetValue("MaxLinear", linear_spin_->value());
  config.mapSetValue("MaxAngular", angular_spin_->value());
  config.mapSetValue(
    "CommandMessageType",
    QString(message_type_ == CommandMessageType::TwistStamped ? "TwistStamped" : "Twist"));
  config.mapSetValue("CommandFrameId", frame_id_editor_->text());
}

void TeleopPanel::load(const rviz_common::Config & config)
{
  rviz_common::Panel::load(config);

  QString topic;
  if (config.mapGetString("Topic", &topic) || config.mapGetString("CmdVelTopic", &topic)) {
    command_topic_combo_->setEditText(topic);
    setTopic(topic);
  }

  bool enabled = false;
  if (config.mapGetBool("Enabled", &enabled)) {
    enable_cmdvel_->setChecked(enabled);
  }

  float max_linear = 0.0f;
  if (config.mapGetFloat("MaxLinear", &max_linear)) {
    linear_spin_->setValue(max_linear);
  }

  float max_angular = 0.0f;
  if (config.mapGetFloat("MaxAngular", &max_angular)) {
    angular_spin_->setValue(max_angular);
  }

  // Older config files omit these keys; defaults stay Twist / "base_link".
  QString frame_id;
  if (config.mapGetString("CommandFrameId", &frame_id)) {
    frame_id_editor_->setText(frame_id);
  }

  // Setting the combo index drives updateMessageType(), which restores the
  // frame id row visibility and recreates the matching publisher.
  QString message_type;
  if (config.mapGetString("CommandMessageType", &message_type)) {
    message_type_combo_->setCurrentIndex(message_type == "TwistStamped" ? 1 : 0);
  }

  // Refresh suggestions for the restored message type without touching the
  // topic text that was just loaded.
  refreshTopicSuggestions();
}

}  // namespace teleop_panel

PLUGINLIB_EXPORT_CLASS(teleop_panel::TeleopPanel, rviz_common::Panel)

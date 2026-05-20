#include "teleop_panel.h"

#include <cmath>

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTimer>
#include <QVBoxLayout>

#include "pluginlib/class_list_macros.hpp"

#include "drive_widget.h"

namespace teleop_panel
{

TeleopPanel::TeleopPanel(QWidget * parent)
: rviz_common::Panel(parent),
  linear_velocity_(0.0f),
  angular_velocity_(0.0f),
  enabled_(false),
  stop_sent_(false)
{
  QHBoxLayout * topic_layout = new QHBoxLayout;
  topic_layout->addWidget(new QLabel("Output Topic:"));
  output_topic_editor_ = new QLineEdit;
  topic_layout->addWidget(output_topic_editor_);

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
  layout->addLayout(teleop_layout);
  setLayout(layout);

  output_timer_ = new QTimer(this);

  connect(
    drive_widget_, SIGNAL(outputVelocity(float, float)),
    this, SLOT(setCmdVel(float, float)));
  connect(output_topic_editor_, SIGNAL(editingFinished()), this, SLOT(updateTopic()));
  connect(output_timer_, SIGNAL(timeout()), this, SLOT(sendCmdVel()));
  connect(enable_cmdvel_, SIGNAL(toggled(bool)), this, SLOT(toggledEnabled(bool)));

  drive_widget_->setEnabled(false);
  output_timer_->stop();

  velocity_node_ = std::make_shared<rclcpp::Node>("teleop_panel_velocity_node");
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
  setTopic(output_topic_editor_->text());
}

void TeleopPanel::setTopic(const QString & new_topic)
{
  if (new_topic == output_topic_) {
    return;
  }

  output_topic_ = new_topic;
  velocity_publisher_.reset();

  if (!output_topic_.isEmpty()) {
    velocity_publisher_ = velocity_node_->create_publisher<geometry_msgs::msg::Twist>(
      output_topic_.toStdString(), 1);
  }

  updatePublishingState();
  Q_EMIT configChanged();
}

void TeleopPanel::sendCmdVel()
{
  if (!enabled_ || !rclcpp::ok() || !velocity_publisher_) {
    return;
  }

  const bool stopped = linear_velocity_ == 0.0f && angular_velocity_ == 0.0f;
  if (stopped && stop_sent_) {
    return;
  }

  geometry_msgs::msg::Twist msg;
  msg.linear.x = linear_velocity_;
  msg.angular.z = angular_velocity_;
  velocity_publisher_->publish(msg);

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
  const bool can_publish = enabled_ && !output_topic_.isEmpty() && velocity_publisher_ != nullptr;

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
  config.mapSetValue("Topic", output_topic_);
  config.mapSetValue("Enabled", enabled_);
  config.mapSetValue("MaxLinear", linear_spin_->value());
  config.mapSetValue("MaxAngular", angular_spin_->value());
}

void TeleopPanel::load(const rviz_common::Config & config)
{
  rviz_common::Panel::load(config);

  QString topic;
  if (config.mapGetString("Topic", &topic) || config.mapGetString("CmdVelTopic", &topic)) {
    output_topic_editor_->setText(topic);
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
}

}  // namespace teleop_panel

PLUGINLIB_EXPORT_CLASS(teleop_panel::TeleopPanel, rviz_common::Panel)

#ifndef TELEOP_PANEL_H
#define TELEOP_PANEL_H

#ifndef Q_MOC_RUN
#include <memory>

#include "geometry_msgs/msg/twist.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rviz_common/panel.hpp"
#endif

class QCheckBox;
class QDoubleSpinBox;
class QLineEdit;
class QTimer;

namespace teleop_panel
{

class DriveWidget;

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

protected:
  void updatePublishingState();
  void resetStopState();

  DriveWidget * drive_widget_;
  QLineEdit * output_topic_editor_;
  QCheckBox * enable_cmdvel_;
  QDoubleSpinBox * linear_spin_;
  QDoubleSpinBox * angular_spin_;
  QTimer * output_timer_;

  QString output_topic_;

  std::shared_ptr<rclcpp::Node> velocity_node_;
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr velocity_publisher_;

  float linear_velocity_;
  float angular_velocity_;
  bool enabled_;
  bool stop_sent_;
};

}  // namespace teleop_panel

#endif  // TELEOP_PANEL_H

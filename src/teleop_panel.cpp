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

#include <stdio.h>

#include <QPainter>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QCheckBox>
#include <QDoubleSpinBox>

#include <geometry_msgs/Twist.h>
#include <std_msgs/Bool.h>

#include "drive_widget.h"
#include "teleop_panel.h"

namespace teleop_panel
{
// BEGIN_TUTORIAL
// Here is the implementation of the TeleopPanel class.  TeleopPanel
// has these responsibilities:
//
// - Act as a container for GUI elements DriveWidget and QLineEdit.
// - Publish command velocities 10 times per second (whether 0 or not).
// - Saving and restoring internal state from a config file.
//
// We start with the constructor, doing the standard Qt thing of
// passing the optional *parent* argument on to the superclass
// constructor, and also zero-ing the velocities we will be
// publishing.
TeleopPanel::TeleopPanel(QWidget* parent)
  : rviz::Panel(parent)
  , linear_velocity_(0)
  , angular_velocity_(0)
  , max_linear_velocity_(1.0)
  , max_angular_velocity_(1.0)
  , enabled_(false)
  , latch_sent_(false)
  , estop_enabled_(false)
  , estop_value_(false)
{
  // Next we lay out the "cmdvel topic" text entry field using a
  // QLabel and a QLineEdit in a QHBoxLayout.
  QHBoxLayout* topic_layout = new QHBoxLayout;
  topic_layout->addWidget(new QLabel("Output Topic:"));
  cmdvel_topic_editor_ = new QLineEdit;
  topic_layout->addWidget(cmdvel_topic_editor_);
  // Then create the control widget.
  drive_widget_ = new DriveWidget;

  // Lay out the topic field above the control widget.
  QVBoxLayout* control_layout = new QVBoxLayout;
  enable_cmdvel_ = new QCheckBox("Enabled");
  control_layout->addWidget(enable_cmdvel_);
  latch_cmdvel_ = new QCheckBox("Latched");
  control_layout->addWidget(latch_cmdvel_);

  QHBoxLayout* linear_layout = new QHBoxLayout;
  linear_layout->addWidget(new QLabel("Max linear:"));
  linear_spin_ = new QDoubleSpinBox();
  linear_spin_->setMaximum(99);  // this are the default values
  linear_spin_->setMinimum(0);
  linear_spin_->setValue(1);
  linear_layout->addWidget(linear_spin_);

  QHBoxLayout* angular_layout = new QHBoxLayout;
  angular_layout->addWidget(new QLabel("Max angular:"));
  angular_spin_ = new QDoubleSpinBox();
  angular_spin_->setMaximum(99);  // this are the default values
  angular_spin_->setMinimum(0);
  angular_spin_->setValue(1);
  angular_layout->addWidget(angular_spin_);

  control_layout->addLayout(linear_layout);
  control_layout->addLayout(angular_layout);

  QHBoxLayout* enable_layout = new QHBoxLayout;
  enable_layout->addWidget(drive_widget_);
  enable_layout->addLayout(control_layout);

  QVBoxLayout* layout = new QVBoxLayout;
  layout->addLayout(topic_layout);
  layout->addLayout(enable_layout);

  QHBoxLayout* estop_value_layout = new QHBoxLayout;
  estop_value_layout->addWidget(new QLabel("E-Stop Topic:"));
  estop_topic_editor_ = new QLineEdit;
  estop_value_layout->addWidget(estop_topic_editor_);
  value_estop_ = new QCheckBox("");
  estop_value_layout->addWidget(value_estop_);
  
  QVBoxLayout* estop_layout = new QVBoxLayout;
  estop_layout->addLayout(estop_value_layout);
  enable_estop_ = new QCheckBox("Enabled");
  estop_layout->addWidget(enable_estop_);
  layout->addLayout(estop_layout);


  setLayout(layout);

  // Create a timer for sending the cmdvel.  Motor controllers want to
  // be reassured frequently that they are doing the right thing, so
  // we keep re-sending velocities even when they aren't changing.
  //
  // Here we take advantage of QObject's memory management behavior:
  // since "this" is passed to the new QTimer as its parent, the
  // QTimer is deleted by the QObject destructor when this TeleopPanel
  // object is destroyed.  Therefore we don't need to keep a pointer
  // to the timer.
  cmdvel_timer_ = new QTimer(this);
  estop_timer_ = new QTimer(this);

  // Next we make signal/slot connections.
  connect(drive_widget_, SIGNAL(outputVelocity(float, float)), this, SLOT(setCmdVel(float, float)));
  connect(cmdvel_topic_editor_, SIGNAL(editingFinished()), this, SLOT(updateCmdVelTopic()));
  connect(cmdvel_timer_, SIGNAL(timeout()), this, SLOT(sendCmdVel()));
  connect(estop_timer_, SIGNAL(timeout()), this, SLOT(sendEStop()));
  connect(enable_cmdvel_, SIGNAL(toggled(bool)), this, SLOT(toggledEnabled(bool)));
  connect(estop_topic_editor_, SIGNAL(editingFinished()), this, SLOT(updateEStopTopic()));
  connect(enable_estop_, SIGNAL(toggled(bool)), this, SLOT(toggledEStopEnabled(bool)));
  connect(value_estop_, SIGNAL(toggled(bool)), this, SLOT(toggledEStopValue(bool)));

  // Make the control widget start disabled, since we don't start with an output topic.
  drive_widget_->setEnabled(false);
  value_estop_->setEnabled(false);
}

// setCmdVel() is connected to the DriveWidget's output, which is sent
// whenever it changes due to a mouse event.  This just records the
// values it is given.  The data doesn't actually get sent until the
// next timer callback.
void TeleopPanel::setCmdVel(float lin, float ang)
{
  // DriveWidget velocites are
  // lin = [+-10]
  // and = [+-2]
  // so we scale them with our limits
  float max_linear = 10;
  float max_angular = 2;

  linear_velocity_ = lin * std::abs(linear_spin_->value() / max_linear);
  angular_velocity_ = ang * std::abs(angular_spin_->value() / max_angular);
}

// Read the topic name from the QLineEdit and call setCmdVelTopic() with the
// results.  This is connected to QLineEdit::editingFinished() which
// fires when the user presses Enter or Tab or otherwise moves focus
// away.
void TeleopPanel::updateCmdVelTopic()
{
  setCmdVelTopic(cmdvel_topic_editor_->text());
}

void TeleopPanel::updateEStopTopic()
{
  setEStopTopic(estop_topic_editor_->text());
}

// Set the topic name we are publishing to.
void TeleopPanel::setCmdVelTopic(const QString& new_topic)
{
  // Only take action if the name has changed.
  if (new_topic != cmdvel_topic_)
  {
    cmdvel_topic_ = new_topic;
    // If the topic is the empty string, don't publish anything.
    if (cmdvel_topic_ == "")
    {
      velocity_publisher_.shutdown();
    }
    else
    {
      // The old ``velocity_publisher_`` is destroyed by this assignment,
      // and thus the old topic advertisement is removed.  The call to
      // nh_advertise() says we want to publish data on the new topic
      // name.
      velocity_publisher_ = nh_.advertise<geometry_msgs::Twist>(cmdvel_topic_.toStdString(), 1);
    }
    // rviz::Panel defines the configChanged() signal.  Emitting it
    // tells RViz that something in this panel has changed that will
    // affect a saved config file.  Ultimately this signal can cause
    // QWidget::setWindowModified(true) to be called on the top-level
    // rviz::VisualizationFrame, which causes a little asterisk ("*")
    // to show in the window's title bar indicating unsaved changes.
    Q_EMIT configChanged();
  }

  // Gray out the control widget when the cmdvel topic is empty.
  // drive_widget_->setEnabled(cmdvel_topic_ != "");
}

// Set the topic name we are publishing to.
void TeleopPanel::setEStopTopic(const QString& new_topic)
{
  // Only take action if the name has changed.
  if (new_topic != estop_topic_)
  {
    estop_topic_ = new_topic;
    // If the topic is the empty string, don't publish anything.
    if (estop_topic_ == "")
    {
      estop_publisher_.shutdown();
    }
    else
    {
      // The old ``velocity_publisher_`` is destroyed by this assignment,
      // and thus the old topic advertisement is removed.  The call to
      // nh_advertise() says we want to publish data on the new topic
      // name.
      estop_publisher_ = nh_.advertise<std_msgs::Bool>(estop_topic_.toStdString(), 1);
    }
    // rviz::Panel defines the configChanged() signal.  Emitting it
    // tells RViz that something in this panel has changed that will
    // affect a saved config file.  Ultimately this signal can cause
    // QWidget::setWindowModified(true) to be called on the top-level
    // rviz::VisualizationFrame, which causes a little asterisk ("*")
    // to show in the window's title bar indicating unsaved changes.
    Q_EMIT configChanged();
  }

  // Gray out the control widget when the estop topic is empty.
  // drive_widget_->setEnabled(estop_topic_ != "");
}

// Publish the control velocities if ROS is not shutting down and the
// publisher is ready with a valid topic name.
void TeleopPanel::sendCmdVel()
{
  if (ros::ok() && velocity_publisher_)
  {
    if (linear_velocity_ == 0 and angular_velocity_ == 0)
    {
      if (latch_cmdvel_->isChecked() == false and latch_sent_ == true)
        return;
      else
      {
        latch_sent_ = true;
      }
    }
    else
      latch_sent_ = false;

    geometry_msgs::Twist msg;
    msg.linear.x = linear_velocity_;
    msg.linear.y = 0;
    msg.linear.z = 0;
    msg.angular.x = 0;
    msg.angular.y = 0;
    msg.angular.z = angular_velocity_;
    velocity_publisher_.publish(msg);
  }
}

void TeleopPanel::sendEStop()
{
  if (ros::ok() && estop_publisher_)
  {
    std_msgs::Bool msg;
    msg.data = estop_value_;
    estop_publisher_.publish(msg);
  }
}

// Save all configuration data from this panel to the given
// Config object.  It is important here that you call save()
// on the parent class so the class id and panel name get saved.
void TeleopPanel::save(rviz::Config config) const
{
  rviz::Panel::save(config);
  config.mapSetValue("CmdVelTopic", cmdvel_topic_);
  config.mapSetValue("EStopTopic", estop_topic_);
}

// Load all configuration data for this panel from the given Config object.
void TeleopPanel::load(const rviz::Config& config)
{
  rviz::Panel::load(config);
  QString topic;
  if (config.mapGetString("CmdVelTopic", &topic))
  {
    cmdvel_topic_editor_->setText(topic);
    updateCmdVelTopic();
  }
  if (config.mapGetString("EStopTopic", &topic))
  {
    estop_topic_editor_->setText(topic);
    updateEStopTopic();
  }
}
void TeleopPanel::toggledEnabled(bool checked)
{
  ROS_INFO_STREAM("checked: " << checked);
  drive_widget_->setEnabled(checked);

  if (checked == true)
    // Start the timer.
    cmdvel_timer_->start(100);
  else
    cmdvel_timer_->stop();
}
void TeleopPanel::toggledEStopEnabled(bool checked)
{
  ROS_INFO_STREAM("estop: " << checked);

  estop_enabled_ = checked;
  if (checked == true)
    // Start the timer.
    estop_timer_->start(100);
  else
    estop_timer_->stop();

  value_estop_->setEnabled(checked);
}

void TeleopPanel::toggledEStopValue(bool checked)
{
  ROS_INFO_STREAM("estop value: " << checked);

  estop_value_ = checked;
}
}  // end namespace teleop_panel

// Tell pluginlib about this class.  Every class which should be
// loadable by pluginlib::ClassLoader must have these two lines
// compiled in its .cpp file, outside of any namespace scope.
#include <pluginlib/class_list_macros.h>
PLUGINLIB_EXPORT_CLASS(teleop_panel::TeleopPanel, rviz::Panel)
// END_TUTORIAL

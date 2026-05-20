# teleop_panel

`teleop_panel` is a simple RViz panel plugin for manual robot teleoperation on ROS 2.

It provides:

- A 2D drive widget inside RViz
- Topic selection for velocity commands
- Enable/disable control of the publisher
- Linear and angular velocity scaling

The plugin is intended to replace the old teleoperation panel from `rviz_plugin_tutorials` with a package maintained in the Robotnik workspace.

## Build

Build the package in a ROS 2 workspace with:

```bash
colcon build --packages-select teleop_panel
```

## Use in RViz

Once the workspace is built and sourced, the panel can be added from RViz as:

`Panels` -> `Add New Panel` -> `teleop_panel/Teleop`

In Robotnik simulation, the provided RViz configurations already include this panel.

## Notes

- The panel publishes `geometry_msgs/msg/Twist`
- The topic must match the robot command topic used in the current simulation or deployment
- If multiple teleoperation sources publish to the same topic at the same time, they can interfere with each other

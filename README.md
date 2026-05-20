# teleop_panel

`teleop_panel` is a simple RViz panel plugin for manual robot teleoperation on ROS 2.

It provides:

- A 2D drive widget inside RViz
- Topic selection for velocity commands
- Command message type selection (`Twist` / `TwistStamped`)
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

## Command message type

The panel exposes a **Command message type** combo box with two options:

- **Twist** — publishes `geometry_msgs/msg/Twist`. This is the default, kept for
  backward compatibility with existing setups.
- **TwistStamped** — publishes `geometry_msgs/msg/TwistStamped`. Useful for newer
  `ros2_control` / `diff_drive_controller` / Nav2-style setups that expect a
  stamped command with a header.

When **TwistStamped** is selected, an extra **Command frame id** row appears. Its
value (default `base_link`) is written to `header.frame_id`, and `header.stamp`
is filled with the current ROS time. The frame id row is hidden and the frame id
is unused while **Twist** is selected.

The selected message type and frame id are saved and restored with the RViz
configuration. Older config files without these fields load as `Twist` /
`base_link`.

## Manual test procedure

1. Build:

   ```bash
   colcon build --symlink-install
   ```

2. Source:

   ```bash
   source install/setup.bash
   ```

3. Launch RViz and load the panel (`Panels` -> `Add New Panel` -> `teleop_panel/Teleop`).
4. Check the default mode:
   - **Command message type** should be `Twist`.
   - The **Command frame id** row should be hidden.
   - After publishing, `ros2 topic info /cmd_vel` should show `geometry_msgs/msg/Twist`.
5. Select **Command message type = TwistStamped**:
   - The **Command frame id** row should appear.
   - Set the frame id to `base_link` or another value.
   - After publishing, `ros2 topic info /cmd_vel` should show `geometry_msgs/msg/TwistStamped`.
   - `ros2 topic echo /cmd_vel` should show `header.stamp` and `header.frame_id`.
6. Select **Twist** again:
   - The **Command frame id** row should be hidden.
   - Commands should again be published as `geometry_msgs/msg/Twist`.

## Notes

- The panel publishes `geometry_msgs/msg/Twist` or `geometry_msgs/msg/TwistStamped`
  depending on the selected command message type
- The topic must match the robot command topic used in the current simulation or deployment
- If multiple teleoperation sources publish to the same topic at the same time, they can interfere with each other

^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Changelog for package teleop_panel
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

0.10.4 (2026-05-20)
-------------------
* Ported the package to ROS 2 with `ament_cmake`, `rclcpp` and `rviz_common`.
* Reduced the package scope to the Teleop RViz panel only.
* Added Robotnik teleoperation behavior:
  enable/disable publishing and linear/angular scaling.
* Avoided continuous zero-velocity publishing while the panel remains idle.
* Added basic package documentation.

0.10.3 (2018-05-09)
-------------------
* Fixed a warning due to a publisher which did not use the keyword argument 'queue_size' (`#43 <https://github.com/ros-visualization/visualization_tutorials/issues/43>`_)
* Changed manifest.xml to package.xml in documentation (`#42 <https://github.com/ros-visualization/visualization_tutorials/issues/42>`_)
* Contributors: Zihan Chen

0.10.2 (2018-01-05)
-------------------
* Unified find_package for Qt4 and Qt5. (`#33 <https://github.com/ros-visualization/visualization_tutorials//issues/33>`_)
* Contributors: Robert Haschke, William Woodall

0.10.1 (2016-04-21)
-------------------
* Added qt5 dependencies to the package.xml.
* Contributors: William Woodall

0.10.0 (2016-04-21)
-------------------
* Added support Qt5 in Kinetic.
* Contributors: William Woodall

0.9.2 (2015-09-21)
------------------

0.9.1 (2015-01-26)
------------------
* Added ``#ifndef Q_MOC_RUN`` guard for compatibility with newer boost versions.
* Contributors: Ryohei Ueda, William Woodall

0.9.0 (2014-03-24)
------------------
* set myself (william) as maintainer
* Contributors: William Woodall

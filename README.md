
# **easy_perception_deployment**
[![Build Status](https://travis-ci.com/ros-industrial/easy_perception_deployment.svg?branch=master)](https://travis-ci.com/github/ros-industrial/easy_perception_deployment)[![codecov](https://codecov.io/gh/cardboardcode/easy_perception_deployment/branch/master/graph/badge.svg)](https://codecov.io/gh/cardboardcode/easy_perception_deployment)
[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0) [![Documentation Status](https://readthedocs.org/projects/easy-perception-deployment/badge/?version=latest)](https://easy-perception-deployment.readthedocs.io/en/latest/?badge=latest)


## **What Is This?**

**easy_perception_deployment** is a ROS2 package that accelerates the training and deployment of CV models for industries.

## **Quality Declaration**

This package claims to be in the Quality Level 5 category, see the Quality Declaration for more details.

## **Setup**

This section lists steps on how to build **easy_perception_deployment** package using ROS2 build tools.

``` bash

# Create ROS2 workspace
cd $HOME
mkdir -p epd_ros2_ws/src && cd epd_ros2_ws/src

# Download fast and shallow copy of easy_perception_deployment
git clone https://github.com/ros-industrial/easy_perception_deployment.git \
--depth 1 --single-branch
cd easy_perception_deployment/easy_perception_deployment

# Start up GUI interface.
bash run.bash
```

## **Docs**

[Check out the full documentation here.](https://easy-perception-deployment.readthedocs.io/en/latest/)

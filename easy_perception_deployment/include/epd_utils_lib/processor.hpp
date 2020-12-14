// Copyright 2020 Advanced Remanufacturing and Technology Centre
// Copyright 2020 ROS-Industrial Consortium Asia Pacific Team
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#ifndef EPD_UTILS_LIB__PROCESSOR_HPP_
#define EPD_UTILS_LIB__PROCESSOR_HPP_

#include <chrono>
#include <string>
#include <memory>
#include <functional>

// OpenCV LIB
#include "opencv2/opencv.hpp"

// ROS2 LIB
#include "cv_bridge/cv_bridge.h"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
#include "sensor_msgs/msg/image.hpp"
#include "sensor_msgs/msg/camera_info.hpp"
#include "sensor_msgs/msg/region_of_interest.hpp"
#include "message_filters/subscriber.h"
#include "message_filters/synchronizer.h"
#include "message_filters/sync_policies/approximate_time.h"

// EPD_UTILS LIB
#include "epd_utils_lib/epd_container.hpp"
#include "epd_msgs/msg/epd_image_classification.hpp"
#include "epd_msgs/msg/epd_object_detection.hpp"
#include "epd_msgs/msg/epd_object_localization.hpp"
#include "epd_msgs/srv/epd_object_localization_service.hpp"
#include "epd_msgs/msg/localized_object.hpp"
#include "epd_utils_lib/message_utils.hpp"

#include "epd_msgs/srv/trigger.hpp"

/*! \class Processor
    \brief An Processor class object.
    This class object inherits rclcpp::Node object and acts the main bridge
    between the ROS2 interface and the underlying ort_cpp_lib library that is
    based on ONNXRuntime Library.
*/
class Processor : public rclcpp::Node
{
public:
  /*! \brief A Constructor function*/
  Processor(void);

private:
  /*! \brief A subscriber member variable to receive remote calls to shutdown.*/
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr status_sub;
  /*! \brief A subscriber member variable to receive 2D RGB images to receive.*/
  rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr image_sub;
  /*! \brief A subscriber member variable to receive depth images to receive.*/
  typedef message_filters::sync_policies::ApproximateTime
    <sensor_msgs::msg::Image, sensor_msgs::msg::Image,
      sensor_msgs::msg::CameraInfo> SyncPolicy;

  message_filters::Subscriber<sensor_msgs::msg::Image> localize_image_rgb;
  message_filters::Subscriber<sensor_msgs::msg::Image> localize_image_depth;
  message_filters::Subscriber<sensor_msgs::msg::CameraInfo> localize_cam_info;
  message_filters::Synchronizer<SyncPolicy> sync_;

  /*! \brief A publisher member variable to output visualization of inference
  results*/
  rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr visual_pub;
  /*! \brief A publisher member variable to output Precision-Level 1 (P1)
  specific inference output suitable for external agents.*/
  rclcpp::Publisher<epd_msgs::msg::EPDImageClassification>::SharedPtr p1_pub;
  /*! \brief A publisher member variable to output Precision-Level 2 (P2)
  specific inference output suitable for external agents.*/
  rclcpp::Publisher<epd_msgs::msg::EPDObjectDetection>::SharedPtr p2_pub;
  /*! \brief A publisher member variable to output Precision-Level 3 (P3)
  specific inference output suitable for external agents.*/
  rclcpp::Publisher<epd_msgs::msg::EPDObjectDetection>::SharedPtr p3_pub;
  /*! \brief A publisher member variable to output Precision-Level 3 (P3)
  specific inference output suitable for external agents.*/
  rclcpp::Publisher<epd_msgs::msg::EPDObjectLocalization>::SharedPtr localize_pub;
  /*! \brief A EPDContainer member object that serves as the aforementioned
  bridge.*/
  rclcpp::Service<epd_msgs::srv::Trigger>::SharedPtr srv_;

  mutable EPD::EPDContainer ortAgent_;

  /*! \brief A ROS2 callback function utilized by image_sub.\n
  It gets ortAgent_ to initialize once and only once when the first input image
  is received.\n
  It also populates the appropriate ROS messages with EPDImageClassification/
  EPDObjectDetection when the onlyVisualize boolean flag is set to false.\n
  */
  void localize_callback(
    const sensor_msgs::msg::Image::SharedPtr msg,
    const sensor_msgs::msg::Image::SharedPtr depth_msg,
    const sensor_msgs::msg::CameraInfo::SharedPtr camera_info);

  void image_callback(const sensor_msgs::msg::Image::SharedPtr msg) const;
};

Processor::Processor(void)
: Node("processer"),
  localize_image_rgb(this, "/camera/color/image_raw"),
  localize_image_depth(this, "/camera/aligned_depth_to_color/image_raw"),
  localize_cam_info(this, "/camera/color/camera_info"),
  sync_(SyncPolicy(10), localize_image_rgb, localize_image_depth, localize_cam_info)
{
  // Creating subscriber
  image_sub = this->create_subscription<sensor_msgs::msg::Image>(
    "/processor/image_input",
    10,
    std::bind(&Processor::image_callback, this, std::placeholders::_1));

  // Creating publisher
  visual_pub = this->create_publisher<sensor_msgs::msg::Image>(
    "/processor/output",
    10);
  p1_pub = this->create_publisher<epd_msgs::msg::EPDImageClassification>(
    "/processor/epd_p1_output",
    10);
  p2_pub = this->create_publisher<epd_msgs::msg::EPDObjectDetection>(
    "/processor/epd_p2_output",
    10);
  p3_pub = this->create_publisher<epd_msgs::msg::EPDObjectDetection>(
    "/processor/epd_p3_output",
    10);

  localize_pub = this->create_publisher<epd_msgs::msg::EPDObjectLocalization>(
    "/processor/epd_localize_output",
    10);

  // If useCaseMode is detected to be Localization,
  // Subscribe to all synchronized ROS2 topics.
  if (ortAgent_.useCaseMode == 3) {
    localize_image_rgb.subscribe();
    localize_image_depth.subscribe();
    localize_cam_info.subscribe();
    sync_.registerCallback(&Processor::localize_callback, this);
  }
}

// WARNING: The use of message filter sychronization causes the intake of image
// from a realsense D415 camera to be irregular. In other words, this callback cannot
// be called at a fixed interval.
void Processor::localize_callback(
  const sensor_msgs::msg::Image::SharedPtr msg,
  const sensor_msgs::msg::Image::SharedPtr depth_msg,
  const sensor_msgs::msg::CameraInfo::SharedPtr camera_info)
{
  /* Check if input image is empty or not.
  If empty, discard image and don't process.
  Otherwise, proceed with processing.
  */
  if (msg->height == 0) {
    RCLCPP_WARN(this->get_logger(), "Input image empty. Discarding.");
    return;
  }

  // Convert ROS Image message to cv::Mat for processing.
  std::shared_ptr<cv_bridge::CvImage> imgptr = cv_bridge::toCvCopy(msg, "bgr8");
  cv::Mat img = imgptr->image;

  std::shared_ptr<cv_bridge::CvImage> depth_imageptr = cv_bridge::toCvCopy(
    depth_msg, sensor_msgs::image_encodings::TYPE_16UC1);
  cv::Mat depth_img = depth_imageptr->image;

  if (!ortAgent_.isInit()) {
    ortAgent_.setFrameDimension(img.cols, img.rows);
    ortAgent_.initORTSessionHandler();
    ortAgent_.setInitBoolean(true);
  } else {
    // TODO(cardboardcode) Implement auto reinitialization of Ort Session.
    /*
    Check if height and width has changed or not.
    If either dim changed, throw runtime error.
    Otherwise, proceed.
    */
    if (ortAgent_.getWidth() != img.cols && ortAgent_.getHeight() != img.rows) {
      throw std::runtime_error("Input camera changed. Please restart.");
    }
  }
  // Initialize timer
  std::chrono::high_resolution_clock::time_point begin = std::chrono::high_resolution_clock::now();

  cv::Mat resultImg;
  if (ortAgent_.isVisualize()) {
    resultImg = ortAgent_.p3_ort_session->infer_visualize(img, depth_img, *camera_info);

    sensor_msgs::msg::Image::SharedPtr output_msg =
      cv_bridge::CvImage(std_msgs::msg::Header(), "bgr8", resultImg).toImageMsg();
    visual_pub->publish(*output_msg);

    // DEBUG
    std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
    auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin);
    RCLCPP_INFO(this->get_logger(), "[-FPS-]= %f\n", 1000.0 / elapsedTime.count());

  } else {
    EPD::EPDObjectLocalization result = ortAgent_.p3_ort_session->infer_action(
      img,
      depth_img,
      *camera_info);
    epd_msgs::msg::EPDObjectLocalization output_msg;

    output_msg.header = std_msgs::msg::Header();
    output_msg.frame_width = img.cols;
    output_msg.frame_height = img.rows;
    output_msg.depth_image = *depth_msg;
    output_msg.camera_info = *camera_info;

    output_msg.num_objects = result.data_size;

    // Populate output_msg objects and roi_array
    for (size_t i = 0; i < result.data_size; i++) {
      epd_msgs::msg::LocalizedObject object;
      object.name = result.objects[i].name;
      object.pos = result.objects[i].pos;
      object.roi = result.objects[i].roi;
      object.breadth = result.objects[i].breadth;
      object.length = result.objects[i].length;
      object.height = result.objects[i].height;

      output_msg.objects.push_back(object);
      output_msg.roi_array.push_back(result.objects[i].roi);
    }
    // DEBUG
    std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
    auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin);
    RCLCPP_INFO(this->get_logger(), "[-FPS-]= %f\n", 1000.0 / elapsedTime.count());

    output_msg.process_time = elapsedTime.count();

    localize_pub->publish(output_msg);
  }
}

void Processor::image_callback(const sensor_msgs::msg::Image::SharedPtr msg) const
{
  // RCLCPP_INFO(this->get_logger(), "Image received");

  /* Check if input image is empty or not.
  If empty, discard image and don't process.
  Otherwise, proceed with processing.
  */
  if (msg->height == 0) {
    RCLCPP_WARN(this->get_logger(), "Input image empty. Discarding.");
    return;
  }

  // Convert ROS Image message to cv::Mat for processing.
  std::shared_ptr<cv_bridge::CvImage> imgptr = cv_bridge::toCvCopy(msg, "bgr8");
  cv::Mat img = imgptr->image;

  if (!ortAgent_.isInit()) {
    ortAgent_.setFrameDimension(img.cols, img.rows);
    ortAgent_.initORTSessionHandler();
    ortAgent_.setInitBoolean(true);
  } else {
    // TODO(cardboardcode) Implement auto reinitialization of Ort Session.
    /*
    Check if height and width has changed or not.
    If either dim changed, throw runtime error.
    Otherwise, proceed.
    */
    if (ortAgent_.getWidth() != img.cols && ortAgent_.getHeight() != img.rows) {
      throw std::runtime_error("Input camera changed. Please restart.");
    }
  }
  // DEBUG
  // Initialize timer
  std::chrono::high_resolution_clock::time_point begin = std::chrono::high_resolution_clock::now();

  cv::Mat resultImg;
  switch (ortAgent_.precision_level) {
    case 1:
      {
        epd_msgs::msg::EPDImageClassification output_msg;
        output_msg.object_names = ortAgent_.p1_ort_session->infer(img);

        // TODO(cardboardcode) Populate header information with timestamp
        // output_msg.header = std_msgs::msg::Header();

        p1_pub->publish(output_msg);
        break;
      }
    case 2:
      {
        if (ortAgent_.isVisualize()) {
          resultImg = ortAgent_.p2_ort_session->infer_visualize(img);
          sensor_msgs::msg::Image::SharedPtr output_msg =
            cv_bridge::CvImage(std_msgs::msg::Header(), "bgr8", resultImg).toImageMsg();
          visual_pub->publish(*output_msg);
        } else {
          EPD::EPDObjectDetection result = ortAgent_.p2_ort_session->infer_action(img);
          epd_msgs::msg::EPDObjectDetection output_msg;
          for (size_t i = 0; i < result.data_size; i++) {
            output_msg.class_indices.push_back(result.classIndices[i]);

            output_msg.scores.push_back(result.scores[i]);

            sensor_msgs::msg::RegionOfInterest roi;
            roi.x_offset = result.bboxes[i][0];
            roi.y_offset = result.bboxes[i][1];
            roi.width = result.bboxes[i][2] - result.bboxes[i][0];
            roi.height = result.bboxes[i][3] - result.bboxes[i][1];
            roi.do_rectify = false;
            output_msg.bboxes.push_back(roi);
          }
          p2_pub->publish(output_msg);
        }

        break;
      }
    case 3:
      {
        if (ortAgent_.isVisualize()) {
          resultImg = ortAgent_.p3_ort_session->infer_visualize(img);
          sensor_msgs::msg::Image::SharedPtr output_msg =
            cv_bridge::CvImage(std_msgs::msg::Header(), "bgr8", resultImg).toImageMsg();
          visual_pub->publish(*output_msg);
        } else {
          EPD::EPDObjectDetection result = ortAgent_.p3_ort_session->infer_action(img);
          epd_msgs::msg::EPDObjectDetection output_msg;
          for (size_t i = 0; i < result.data_size; i++) {
            output_msg.class_indices.push_back(result.classIndices[i]);

            output_msg.scores.push_back(result.scores[i]);

            sensor_msgs::msg::RegionOfInterest roi;
            roi.x_offset = result.bboxes[i][0];
            roi.y_offset = result.bboxes[i][1];
            roi.width = result.bboxes[i][2] - result.bboxes[i][0];
            roi.height = result.bboxes[i][3] - result.bboxes[i][1];
            roi.do_rectify = false;
            output_msg.bboxes.push_back(roi);

            sensor_msgs::msg::Image::SharedPtr mask =
              cv_bridge::CvImage(std_msgs::msg::Header(), "32FC1", result.masks[i]).toImageMsg();
            output_msg.masks.push_back(*mask);
          }
          p3_pub->publish(output_msg);
        }

        break;
      }
  }

  // DEBUG
  std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
  auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin);
  RCLCPP_INFO(this->get_logger(), "[-FPS-]= %f\n", 1000.0 / elapsedTime.count());
}

#endif  // EPD_UTILS_LIB__PROCESSOR_HPP_

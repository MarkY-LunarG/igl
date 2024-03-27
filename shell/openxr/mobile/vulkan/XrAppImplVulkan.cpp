/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "XrAppImplVulkan.h"

#include <shell/openxr/src/XrLog.h>

#include <igl/vulkan/Device.h>
#include <igl/vulkan/HWDevice.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanDevice.h>

#include <shell/openxr/src/XrSwapchainProvider.h>
#include <shell/openxr/mobile/vulkan/XrSwapchainProviderImplVulkan.h>

namespace igl::shell::openxr::mobile {
std::vector<const char*> XrAppImplVulkan::getXrRequiredExtensions() const {
  return {
    XR_KHR_VULKAN_ENABLE_EXTENSION_NAME, XR_FB_SWAPCHAIN_UPDATE_STATE_VULKAN_EXTENSION_NAME,
        XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME,
#if defined(IGL_CMAKE_BUILD)
        XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME
#endif
  };
}

void* XrAppImplVulkan::getInstanceCreateExtension() {
#if defined(IGL_CMAKE_BUILD)
  return &instanceCreateInfoAndroid_;
#else
  return nullptr;
#endif
}

std::unique_ptr<igl::IDevice> XrAppImplVulkan::initIGL(XrInstance instance, XrSystemId systemId) {
  // Get the API requirements.
  PFN_xrGetVulkanGraphicsRequirementsKHR pfnGetVulkanGraphicsRequirementsKHR = NULL;
  XR_CHECK(xrGetInstanceProcAddr(instance,
                                 "xrGetVulkanGraphicsRequirementsKHR",
                                 (PFN_xrVoidFunction*)(&pfnGetVulkanGraphicsRequirementsKHR)));

  XR_CHECK(pfnGetVulkanGraphicsRequirementsKHR(instance, systemId, &graphicsRequirements_));

  // Get required instance extensions
  PFN_xrGetVulkanInstanceExtensionsKHR pfnGetVulkanInstanceExtensionsKHR = NULL;
  XR_CHECK(xrGetInstanceProcAddr(instance,
                                 "xrGetVulkanInstanceExtensionsKHR",
                                 (PFN_xrVoidFunction*)(&pfnGetVulkanInstanceExtensionsKHR)));

  uint32_t bufferSize = 0;
  XR_CHECK(pfnGetVulkanInstanceExtensionsKHR(instance, systemId, 0, &bufferSize, NULL));

  requiredVkInstanceExtensionsBuffer_.resize(bufferSize);
  XR_CHECK(pfnGetVulkanInstanceExtensionsKHR(
      instance, systemId, bufferSize, &bufferSize, requiredVkInstanceExtensionsBuffer_.data()));
  requiredVkInstanceExtensions_ = processExtensionsBuffer(requiredVkInstanceExtensionsBuffer_);

  IGL_LOG_INFO("Number of required Vulkan extensions: %d", requiredVkInstanceExtensions_.size());

  // Get the required device extensions.
  bufferSize = 0;
  PFN_xrGetVulkanDeviceExtensionsKHR pfnGetVulkanDeviceExtensionsKHR = NULL;
  XR_CHECK(xrGetInstanceProcAddr(instance,
                                 "xrGetVulkanDeviceExtensionsKHR",
                                 (PFN_xrVoidFunction*)(&pfnGetVulkanDeviceExtensionsKHR)));

  XR_CHECK(pfnGetVulkanDeviceExtensionsKHR(instance, systemId, 0, &bufferSize, NULL));

  requiredVkDeviceExtensionsBuffer_.resize(bufferSize);
  XR_CHECK(pfnGetVulkanDeviceExtensionsKHR(
      instance, systemId, bufferSize, &bufferSize, requiredVkDeviceExtensionsBuffer_.data()));

  requiredVkDeviceExtensions_ = processExtensionsBuffer(requiredVkDeviceExtensionsBuffer_);

  auto context = igl::vulkan::HWDevice::createContext(igl::vulkan::VulkanContextConfig(),
                                                      nullptr,
                                                      requiredVkInstanceExtensions_.size(),
                                                      requiredVkInstanceExtensions_.data());

  PFN_xrGetVulkanGraphicsDeviceKHR pfnGetVulkanGraphicsDeviceKHR = NULL;
  XR_CHECK(xrGetInstanceProcAddr(instance,
                                 "xrGetVulkanGraphicsDeviceKHR",
                                 (PFN_xrVoidFunction*)(&pfnGetVulkanGraphicsDeviceKHR)));

  // Let OpenXR find a suitable Vulkan physical device.
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  XR_CHECK(
      pfnGetVulkanGraphicsDeviceKHR(instance, systemId, context->getVkInstance(), &physicalDevice));
  if (physicalDevice == VK_NULL_HANDLE) {
    IGL_LOG_ERROR("OpenXR: Failed to get vulkan physical device");
    return nullptr;
  }

  igl::HWDeviceDesc hwDevice(reinterpret_cast<uintptr_t>(physicalDevice),
                             igl::HWDeviceType::IntegratedGpu);

  auto device = igl::vulkan::HWDevice::create(std::move(context),
                                              hwDevice,
                                              0,
                                              0,
                                              requiredVkDeviceExtensions_.size(),
                                              requiredVkDeviceExtensions_.data());
  return device;
}

XrSession XrAppImplVulkan::initXrSession(XrInstance instance,
                                         XrSystemId systemId,
                                         igl::IDevice& device) {
  const auto& vkDevice = static_cast<igl::vulkan::Device&>(device); // Downcast is safe here

  // Bind Vulkan to XR session
  XrGraphicsBindingVulkanKHR graphicsBinding = {
      XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR,
      NULL,
      vkDevice.getVulkanContext().getVkInstance(),
      vkDevice.getVulkanContext().getVkPhysicalDevice(),
      vkDevice.getVulkanContext().device_->getVkDevice(),
      vkDevice.getVulkanContext().deviceQueues_.graphicsQueueFamilyIndex,
      0,
  };

  XrSessionCreateInfo sessionCreateInfo = {
      .type = XR_TYPE_SESSION_CREATE_INFO,
      .next = &graphicsBinding,
      .createFlags = 0,
      .systemId = systemId,
  };

  XrResult xrResult;
  XrSession session;
  XR_CHECK(xrResult = xrCreateSession(instance, &sessionCreateInfo, &session));
  if (xrResult != XR_SUCCESS) {
    IGL_LOG_ERROR("Failed to create XR session: %d.", xrResult);
    return XR_NULL_HANDLE;
  }
  IGL_LOG_INFO("XR session created");

  return session;
} // namespace igl::shell::openxr::mobile

std::unique_ptr<impl::XrSwapchainProviderImpl> XrAppImplVulkan::createSwapchainProviderImpl()
    const {
  return std::make_unique<XrSwapchainProviderImplVulkan>();
}

std::vector<const char*> XrAppImplVulkan::processExtensionsBuffer(std::vector<char>& buffer) {
  std::vector<const char*> extensions;
  auto skip = false;
  for (auto& ch : buffer) {
    if (skip) {
      if (ch == ' ') {
        ch = '\0';
        skip = false;
      } else if (ch == '\0') {
        break;
      }
    } else {
      extensions.push_back(&ch);
      skip = true;
    }
  }

  return extensions;
}
} // namespace igl::shell::openxr::mobile

#ifndef __VULKAN_DEMO_H__
#define __VULKAN_DEMO_H__

#include <vulkan/vulkan.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

struct QueueFamilyIndices {
  int graphics_family = -1;
  int present_family = -1;

  bool is_complete() const {
    return graphics_family >= 0 && present_family >= 0;
  }
};

struct SwapChainSupportDetails {
  VkSurfaceCapabilitiesKHR capabilities;
  std::vector<vk::SurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> present_modes;
};

struct UniformBufferObject {
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 proj;
};

class VulkanDemo {
 public:
  void run();

  void set_framebuffer_resized(bool resized) { framebuffer_resized_ = resized; }

 private:
  void InitWindow();
  void InitVulkan();
  void MainLoop();
  void CleanupSwapChain();
  void Cleanup();
  void RecreateSwapChain();
  void CreateInstance();
  void SetupDebugCallback();
  void CreateSurface();
  void PickPhysicalDevice();
  void CreateLogicalDevice();
  void CreateSwapChain();
  void CreateImageViews();
  void CreateRenderPass();
  void CreateDescriptorSetLayout();
  void CreateGraphicsPipeline();
  void CreateFramebuffers();
  void CreateBuffer(size_t buffer_size,
                    VkBufferUsageFlags buffer_usage,
                    VkMemoryPropertyFlags properties,
                    VkBuffer* buffer,
                    VkDeviceMemory* memory);
  void CreateImage(uint32_t width,
                   uint32_t height,
                   VkFormat format,
                   VkImageTiling tiling,
                   VkImageUsageFlags usage,
                   VkMemoryPropertyFlags properties,
                   VkImage& image,
                   VkDeviceMemory& imageMemory);
  void CreateTextureImage();
  VkImageView CreateImageView(VkImage image, VkFormat format);
  void CreateTextureImageView();
  void createTextureSampler();
  void CreateVertexBuffer();
  void CreateIndexBuffer();
  void CreateUniformBuffers();
  void CreateDescriptorPool();
  void CreateDescriptorSets();
  void CreateCommandPool();
  void CreateCommandBuffers();
  void CreateSyncObjects();
  void UpdateUniformBuffer(uint32_t current_image);
  void DrawFrame();
  VkShaderModule CreateShaderModule(const std::vector<char>& code);
  vk::SurfaceFormatKHR ChooseSwapSurfaceFormat(
      const std::vector<vk::SurfaceFormatKHR>& available_formats);
  VkPresentModeKHR ChooseSwapPresentMode(
      const std::vector<VkPresentModeKHR> available_present_modes);
  VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
  SwapChainSupportDetails QuerySwapChainSupport(vk::PhysicalDevice device);
  bool IsDeviceSuitable(vk::PhysicalDevice device);
  bool CheckDeviceExtensionSupport(vk::PhysicalDevice device);
  QueueFamilyIndices FindQueueFamilies(vk::PhysicalDevice device);
  std::vector<const char*> GetRequiredExtensions();
  bool CheckValidationLayerSupport();
  uint32_t FindMemoryType(uint32_t type_filter,
                          VkMemoryPropertyFlags properties);
  VkCommandBuffer beginSingleTimeCommands();
  void endSingleTimeCommands(VkCommandBuffer commandBuffer);
  void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
  void transitionImageLayout(VkImage image,
                             VkFormat format,
                             VkImageLayout oldLayout,
                             VkImageLayout newLayout);
  void copyBufferToImage(VkBuffer buffer,
                         VkImage image,
                         uint32_t width,
                         uint32_t height);

 private:
  GLFWwindow* window_ = nullptr;

  vk::DynamicLoader dl_;
  vk::Instance instance_;
  vk::DebugUtilsMessengerEXT callback_;
  vk::SurfaceKHR surface_;

  vk::PhysicalDevice physical_device_;
  vk::Device device_;

  vk::Queue graphics_queue_;
  vk::Queue present_queue_;

  VkSwapchainKHR swap_chain_ = VK_NULL_HANDLE;
  std::vector<VkImage> swap_chain_images_;
  vk::Format swap_chain_image_format_;
  VkExtent2D swap_chain_extent_;
  std::vector<VkImageView> swap_chain_image_views_;
  std::vector<VkFramebuffer> swap_chain_framebuffers_;

  VkRenderPass render_pass_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout descriptor_set_layout_ = VK_NULL_HANDLE;
  VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
  VkPipeline graphics_pipeline_ = VK_NULL_HANDLE;

  VkCommandPool command_pool_ = VK_NULL_HANDLE;
  std::vector<VkCommandBuffer> command_buffers_;

  VkImage textureImage = VK_NULL_HANDLE;
  VkImageView textureImageView = VK_NULL_HANDLE;
  VkDeviceMemory textureImageMemory = VK_NULL_HANDLE;

  VkBuffer vertex_buffer_ = VK_NULL_HANDLE;
  VkDeviceMemory vertex_buffer_memory_ = VK_NULL_HANDLE;

  VkBuffer index_buffer_ = VK_NULL_HANDLE;
  VkDeviceMemory index_buffer_memory_ = VK_NULL_HANDLE;

  std::vector<VkBuffer> uniform_buffers_;
  std::vector<VkDeviceMemory> uniform_buffers_memory_;

  VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
  std::vector<VkDescriptorSet> descriptor_sets_;

  std::vector<VkSemaphore> image_available_semaphores_;
  std::vector<VkSemaphore> render_finished_semaphores_;
  std::vector<VkFence> in_flight_fences_;
  size_t current_frame_ = 0;

  bool framebuffer_resized_ = false;
};

#endif
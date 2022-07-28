#include "vulkan_demo.h"
#include "vertex.h"

namespace {

const int WIDTH = 800;
const int HEIGHT = 600;

const int MAX_FRAMES_IN_FLIGHT = 2;

const std::vector<const char*> kValidationLayers = {
    "VK_LAYER_KHRONOS_validation"};

const std::vector<const char*> kDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

#ifdef NDEBUG
const bool kEnableValidationLayers = false;
#else
const bool kEnableValidationLayers = true;
#endif

//   0------3
//   | \    |
//   |  \   |
//   |   \  |
//   |    \ |
//   2------1
const std::vector<Vertex> kVertices = {
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
    {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
    {{0.5f, -0.5f}, {1.0f, 0.0f, 1.0f}},
};

const std::array<uint16_t, 6> kIndices = {0, 1, 2, 0, 3, 1};

VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pCallback) {
  auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      instance, "vkCreateDebugUtilsMessengerEXT");
  if (func != nullptr) {
    return func(instance, pCreateInfo, pAllocator, pCallback);
  } else {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance,
                                   VkDebugUtilsMessengerEXT callback,
                                   const VkAllocationCallbacks* pAllocator) {
  auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      instance, "vkDestroyDebugUtilsMessengerEXT");
  if (func != nullptr) {
    func(instance, callback, pAllocator);
  }
}

void FramebufferResizeCallback(GLFWwindow* window, int width, int height) {
  auto app = reinterpret_cast<VulkanDemo*>(glfwGetWindowUserPointer(window));
  app->set_framebuffer_resized(true);
}

VKAPI_ATTR VkBool32 VKAPI_CALL
DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
              VkDebugUtilsMessageTypeFlagsEXT messageType,
              const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
              void* pUserData) {
  std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

  return VK_FALSE;
}

std::vector<uint32_t> ReadFile(const std::string& filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    throw std::runtime_error("failed to open file!");
  }

  size_t file_size = (size_t)file.tellg();
  std::vector<uint32_t> buffer((file_size + 3) / 4);

  file.seekg(0);
  file.read((char*)buffer.data(), file_size);

  file.close();

  return buffer;
}

}  // namespace

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE;

void VulkanDemo::run() {
  InitWindow();
  InitVulkan();
  MainLoop();
  Cleanup();
}

void VulkanDemo::InitWindow() {
  glfwInit();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  window_ = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
  glfwSetWindowUserPointer(window_, this);
  glfwSetFramebufferSizeCallback(window_, FramebufferResizeCallback);
}

void VulkanDemo::InitVulkan() {
  PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr =
      dl_.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
  VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

  CreateInstance();
  SetupDebugCallback();
  CreateSurface();
  PickPhysicalDevice();
  CreateLogicalDevice();
  CreateSwapChain();
  CreateImageViews();
  CreateRenderPass();
  CreateDescriptorSetLayout();
  CreateGraphicsPipeline();
  CreateFramebuffers();
  CreateCommandPool();
  CreateTextureImage();
  CreateTextureImageView();
  CreateVertexBuffer();
  CreateIndexBuffer();
  CreateUniformBuffers();
  CreateDescriptorPool();
  CreateDescriptorSets();
  CreateCommandBuffers();
  CreateSyncObjects();
}

void VulkanDemo::MainLoop() {
  while (!glfwWindowShouldClose(window_)) {
    glfwPollEvents();
    DrawFrame();
  }

  device_.waitIdle();
}

void VulkanDemo::CleanupSwapChain() {
  for (auto framebuffer : swap_chain_framebuffers_) {
    device_.destroyFramebuffer(framebuffer);
  }

  device_.freeCommandBuffers(command_pool_, command_buffers_);
  device_.destroyPipeline(graphics_pipeline_);
  device_.destroyPipelineLayout(pipeline_layout_);
  device_.destroyRenderPass(render_pass_);

  for (auto image_view : swap_chain_image_views_)
    device_.destroyImageView(image_view);

  device_.destroySwapchainKHR(swap_chain_);

  for (size_t i = 0; i < swap_chain_images_.size(); i++) {
    device_.destroyBuffer(uniform_buffers_[i]);
    device_.freeMemory(uniform_buffers_memory_[i]);
  }

  device_.destroyDescriptorPool(descriptor_pool_);
}

void VulkanDemo::Cleanup() {
  CleanupSwapChain();

  device_.destroyBuffer(vertex_buffer_);
  device_.freeMemory(vertex_buffer_memory_);
  device_.destroyBuffer(index_buffer_);
  device_.freeMemory(index_buffer_memory_);
  device_.destroyDescriptorSetLayout(descriptor_set_layout_);

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    device_.destroySemaphore(render_finished_semaphores_[i]);
    device_.destroySemaphore(image_available_semaphores_[i]);
    device_.destroyFence(in_flight_fences_[i]);
  }

  device_.destroyCommandPool(command_pool_);

  device_.destroy();

  if (kEnableValidationLayers) {
    instance_.destroyDebugUtilsMessengerEXT(callback_);
  }

  instance_.destroySurfaceKHR(surface_);
  instance_.destroy();

  glfwDestroyWindow(window_);

  glfwTerminate();
}

void VulkanDemo::RecreateSwapChain() {
  int width = 0, height = 0;
  while (width == 0 || height == 0) {
    glfwGetFramebufferSize(window_, &width, &height);
    glfwWaitEvents();
  }

  vkDeviceWaitIdle(device_);

  CleanupSwapChain();

  CreateSwapChain();
  CreateImageViews();
  CreateRenderPass();
  CreateGraphicsPipeline();
  CreateFramebuffers();
  CreateUniformBuffers();
  CreateDescriptorPool();
  CreateDescriptorSets();
  CreateCommandBuffers();
}

void VulkanDemo::CreateInstance() {
  if (kEnableValidationLayers && !CheckValidationLayerSupport()) {
    throw std::runtime_error("validation layers requested, but not available!");
  }

  vk::ApplicationInfo application_info;
  application_info.setPApplicationName("VulkanDemo")
      .setApplicationVersion(VK_MAKE_VERSION(1, 0, 0))
      .setPEngineName("No Engine")
      .setEngineVersion(VK_MAKE_VERSION(1, 0, 0))
      .setApiVersion(VK_API_VERSION_1_2);

  auto extensions = GetRequiredExtensions();
  vk::InstanceCreateInfo create_info;
  create_info.setPApplicationInfo(&application_info)
      .setPEnabledExtensionNames(extensions);
  if (kEnableValidationLayers)
    create_info.setPEnabledLayerNames(kValidationLayers);

  instance_ = vk::createInstance(create_info);
  VULKAN_HPP_DEFAULT_DISPATCHER.init(instance_);
}

void VulkanDemo::SetupDebugCallback() {
  if (!kEnableValidationLayers)
    return;

  constexpr vk::DebugUtilsMessageSeverityFlagsEXT kServiceFlags =
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
  constexpr vk::DebugUtilsMessageTypeFlagsEXT kMessageTypeFlags =
      vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
      vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
      vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
  vk::DebugUtilsMessengerCreateInfoEXT create_info(
      {}, kServiceFlags, kMessageTypeFlags, DebugCallback, nullptr);
  callback_ = instance_.createDebugUtilsMessengerEXT(create_info);
}

void VulkanDemo::CreateSurface() {
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  if (glfwCreateWindowSurface(instance_, window_, nullptr, &surface) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create window surface!");
  }
  surface_ = surface;
}

void VulkanDemo::PickPhysicalDevice() {
  auto devices = instance_.enumeratePhysicalDevices();

  if (devices.empty()) {
    throw std::runtime_error("failed to find GPUs with Vulkan support!");
  }

  for (const auto& device : devices) {
    if (IsDeviceSuitable(device)) {
      physical_device_ = device;
      break;
    }
  }

  if (!physical_device_) {
    throw std::runtime_error("failed to find a suitable GPU!");
  }
}

void VulkanDemo::CreateLogicalDevice() {
  QueueFamilyIndices indices = FindQueueFamilies(physical_device_);

  std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
  std::set<int> uniqueQueueFamilies = {indices.graphics_family,
                                       indices.present_family};

  float queuePriority = 1.0f;
  for (int queueFamily : uniqueQueueFamilies) {
    vk::DeviceQueueCreateInfo queueCreateInfo({}, queueFamily, 1,
                                              &queuePriority);
    queueCreateInfos.push_back(queueCreateInfo);
  }

  vk::PhysicalDeviceFeatures deviceFeatures;

  vk::DeviceCreateInfo create_info(
      {}, queueCreateInfos.size(), queueCreateInfos.data(),
      kEnableValidationLayers ? kValidationLayers.size() : 0,
      kEnableValidationLayers ? kValidationLayers.data() : nullptr,
      kDeviceExtensions.size(), kDeviceExtensions.data(), &deviceFeatures);

  device_ = physical_device_.createDevice(create_info);

  graphics_queue_ = device_.getQueue(indices.graphics_family, 0);
  present_queue_ = device_.getQueue(indices.present_family, 0);
}

void VulkanDemo::CreateSwapChain() {
  SwapChainSupportDetails swapChainSupport =
      QuerySwapChainSupport(physical_device_);

  vk::SurfaceFormatKHR surfaceFormat =
      ChooseSwapSurfaceFormat(swapChainSupport.formats);
  vk::PresentModeKHR presentMode =
      ChooseSwapPresentMode(swapChainSupport.present_modes);
  vk::Extent2D extent = ChooseSwapExtent(swapChainSupport.capabilities);

  uint32_t image_count = swapChainSupport.capabilities.minImageCount + 1;
  if (swapChainSupport.capabilities.maxImageCount > 0 &&
      image_count > swapChainSupport.capabilities.maxImageCount) {
    image_count = swapChainSupport.capabilities.maxImageCount;
  }

  vk::SwapchainCreateInfoKHR create_info;
  create_info.setSurface(surface_)
      .setMinImageCount(image_count)
      .setImageFormat(surfaceFormat.format)
      .setImageColorSpace(surfaceFormat.colorSpace)
      .setImageExtent(extent)
      .setImageArrayLayers(1)
      .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment);

  QueueFamilyIndices indices = FindQueueFamilies(physical_device_);
  std::array<uint32_t, 2> queueFamilyIndices = {
      (uint32_t)indices.graphics_family, (uint32_t)indices.present_family};

  if (indices.graphics_family != indices.present_family) {
    create_info.setImageSharingMode(vk::SharingMode::eConcurrent)
        .setQueueFamilyIndices(queueFamilyIndices);
  } else {
    create_info.setImageSharingMode(vk::SharingMode::eExclusive);
  }

  create_info.setPreTransform(swapChainSupport.capabilities.currentTransform)
      .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
      .setPresentMode(presentMode)
      .setClipped(true);

  swap_chain_ = device_.createSwapchainKHR(create_info);
  swap_chain_images_ = device_.getSwapchainImagesKHR(swap_chain_);
  swap_chain_image_format_ = surfaceFormat.format;
  swap_chain_extent_ = extent;
}

void VulkanDemo::CreateImageViews() {
  swap_chain_image_views_.resize(swap_chain_images_.size());

  for (size_t i = 0; i < swap_chain_images_.size(); i++) {
    swap_chain_image_views_[i] =
        CreateImageView(swap_chain_images_[i], swap_chain_image_format_);
  }
}

void VulkanDemo::CreateRenderPass() {
  VkAttachmentDescription color_attachment = {};
  color_attachment.format = static_cast<VkFormat>(swap_chain_image_format_);
  color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference color_attachment_ref = {};
  color_attachment_ref.attachment = 0;
  color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_attachment_ref;

  VkSubpassDependency dependency = {};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                             VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo render_pass_info = {};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass_info.attachmentCount = 1;
  render_pass_info.pAttachments = &color_attachment;
  render_pass_info.subpassCount = 1;
  render_pass_info.pSubpasses = &subpass;
  render_pass_info.dependencyCount = 1;
  render_pass_info.pDependencies = &dependency;

  if (vkCreateRenderPass(device_, &render_pass_info, nullptr, &render_pass_) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create render pass!");
  }
}

void VulkanDemo::CreateDescriptorSetLayout() {
  vk::DescriptorSetLayoutBinding ubo_layout_binding;
  ubo_layout_binding.setBinding(0)
      .setDescriptorType(vk::DescriptorType::eUniformBuffer)
      .setDescriptorCount(1)
      .setStageFlags(vk::ShaderStageFlagBits::eVertex);
  vk::DescriptorSetLayoutCreateInfo layout_info;
  layout_info.setBindingCount(1).setPBindings(&ubo_layout_binding);

  descriptor_set_layout_ = device_.createDescriptorSetLayout(layout_info);
}

void VulkanDemo::CreateGraphicsPipeline() {
  auto vert_shader_code = ReadFile("shaders/vert.spv");
  auto frag_shader_code = ReadFile("shaders/frag.spv");

  VkShaderModule vert_shader_module = CreateShaderModule(vert_shader_code);
  VkShaderModule frag_shader_module = CreateShaderModule(frag_shader_code);

  VkPipelineShaderStageCreateInfo vert_shader_stage_info = {};
  vert_shader_stage_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vert_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vert_shader_stage_info.module = vert_shader_module;
  vert_shader_stage_info.pName = "main";

  VkPipelineShaderStageCreateInfo frag_shader_stage_info = {};
  frag_shader_stage_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  frag_shader_stage_info.module = frag_shader_module;
  frag_shader_stage_info.pName = "main";

  VkPipelineShaderStageCreateInfo shader_stages[] = {vert_shader_stage_info,
                                                     frag_shader_stage_info};

  VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
  vertex_input_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  auto binding_description = Vertex::GetBindingDescription();
  vertex_input_info.vertexBindingDescriptionCount = 1;
  vertex_input_info.pVertexBindingDescriptions = &binding_description;

  auto attribute_descriptions = Vertex::GetAttributeDescriptions();
  vertex_input_info.vertexAttributeDescriptionCount =
      attribute_descriptions.size();
  vertex_input_info.pVertexAttributeDescriptions =
      attribute_descriptions.data();

  VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
  inputAssembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  VkViewport viewport = {};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = (float)swap_chain_extent_.width;
  viewport.height = (float)swap_chain_extent_.height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  VkRect2D scissor = {};
  scissor.offset = {0, 0};
  scissor.extent = (VkExtent2D)swap_chain_extent_;

  VkPipelineViewportStateCreateInfo viewport_state = {};
  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.viewportCount = 1;
  viewport_state.pViewports = &viewport;
  viewport_state.scissorCount = 1;
  viewport_state.pScissors = &scissor;

  VkPipelineRasterizationStateCreateInfo rasterizer = {};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.depthBiasEnable = VK_FALSE;

  VkPipelineMultisampleStateCreateInfo multisampling = {};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState color_blend_attachment = {};
  color_blend_attachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  color_blend_attachment.blendEnable = VK_FALSE;

  VkPipelineColorBlendStateCreateInfo color_blending = {};
  color_blending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blending.logicOpEnable = VK_FALSE;
  color_blending.logicOp = VK_LOGIC_OP_COPY;
  color_blending.attachmentCount = 1;
  color_blending.pAttachments = &color_blend_attachment;
  color_blending.blendConstants[0] = 0.0f;
  color_blending.blendConstants[1] = 0.0f;
  color_blending.blendConstants[2] = 0.0f;
  color_blending.blendConstants[3] = 0.0f;

  vk::PipelineLayoutCreateInfo pipeline_layout_info({}, descriptor_set_layout_);
  pipeline_layout_ = device_.createPipelineLayout(pipeline_layout_info);

  VkGraphicsPipelineCreateInfo pipeline_info = {};
  pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_info.stageCount = 2;
  pipeline_info.pStages = shader_stages;
  pipeline_info.pVertexInputState = &vertex_input_info;
  pipeline_info.pInputAssemblyState = &inputAssembly;
  pipeline_info.pViewportState = &viewport_state;
  pipeline_info.pRasterizationState = &rasterizer;
  pipeline_info.pMultisampleState = &multisampling;
  pipeline_info.pColorBlendState = &color_blending;
  pipeline_info.layout = pipeline_layout_;
  pipeline_info.renderPass = render_pass_;
  pipeline_info.subpass = 0;
  pipeline_info.basePipelineHandle = VK_NULL_HANDLE;

  if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipeline_info,
                                nullptr, &graphics_pipeline_) != VK_SUCCESS) {
    throw std::runtime_error("failed to create graphics pipeline!");
  }

  device_.destroyShaderModule(frag_shader_module);
  device_.destroyShaderModule(vert_shader_module);
}

void VulkanDemo::CreateFramebuffers() {
  swap_chain_framebuffers_.resize(swap_chain_image_views_.size());

  for (size_t i = 0; i < swap_chain_image_views_.size(); i++) {
    VkImageView attachments[] = {swap_chain_image_views_[i]};

    VkFramebufferCreateInfo framebuffer_info = {};
    framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_info.renderPass = render_pass_;
    framebuffer_info.attachmentCount = 1;
    framebuffer_info.pAttachments = attachments;
    framebuffer_info.width = swap_chain_extent_.width;
    framebuffer_info.height = swap_chain_extent_.height;
    framebuffer_info.layers = 1;

    if (vkCreateFramebuffer(device_, &framebuffer_info, nullptr,
                            &swap_chain_framebuffers_[i]) != VK_SUCCESS) {
      throw std::runtime_error("failed to create framebuffer!");
    }
  }
}

void VulkanDemo::CreateBuffer(size_t buffer_size,
                              vk::BufferUsageFlags buffer_usage,
                              vk::MemoryPropertyFlags properties,
                              vk::Buffer* buffer,
                              vk::DeviceMemory* memory) {
  vk::BufferCreateInfo buffer_info;
  buffer_info.setSize(buffer_size)
      .setUsage(buffer_usage)
      .setSharingMode(vk::SharingMode::eExclusive);
  *buffer = device_.createBuffer(buffer_info);

  VkMemoryRequirements mem_requirements;
  vkGetBufferMemoryRequirements(device_, *buffer, &mem_requirements);

  vk::MemoryAllocateInfo alloc_info;

  alloc_info.setAllocationSize(mem_requirements.size)
      .setMemoryTypeIndex(FindMemoryType(mem_requirements.memoryTypeBits,
                                         (VkMemoryPropertyFlags)properties));
  *memory = device_.allocateMemory(alloc_info);
  device_.bindBufferMemory(*buffer, *memory, 0);
}

void VulkanDemo::CreateImage(uint32_t width,
                             uint32_t height,
                             vk::Format format,
                             vk::ImageTiling tiling,
                             vk::ImageUsageFlags usage,
                             vk::MemoryPropertyFlags properties,
                             vk::Image& image,
                             vk::DeviceMemory& imageMemory) {
  vk::ImageCreateInfo imageInfo;
  imageInfo.imageType = vk::ImageType::e2D;
  imageInfo.extent.width = width;
  imageInfo.extent.height = height;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.format = format;
  imageInfo.tiling = tiling;
  imageInfo.initialLayout = vk::ImageLayout::eUndefined;
  imageInfo.usage = usage;
  imageInfo.samples = vk::SampleCountFlagBits::e1;
  imageInfo.sharingMode = vk::SharingMode::eExclusive;

  image = device_.createImage(imageInfo);

  vk::MemoryRequirements requirements =
      device_.getImageMemoryRequirements(image);

  vk::MemoryAllocateInfo allocInfo;
  allocInfo.allocationSize = requirements.size;
  allocInfo.memoryTypeIndex = FindMemoryType(requirements.memoryTypeBits,
                                             (VkMemoryPropertyFlags)properties);

  imageMemory = device_.allocateMemory(allocInfo);
  device_.bindImageMemory(image, imageMemory, 0);
}

void VulkanDemo::CreateTextureImage() {
  int texWidth, texHeight, texChannels;
  stbi_uc* pixels = stbi_load("textures/texture.jpg", &texWidth, &texHeight,
                              &texChannels, STBI_rgb_alpha);
  VkDeviceSize imageSize = texWidth * texHeight * 4;

  if (!pixels) {
    throw std::runtime_error("failed to load texture image!");
  }

  vk::Buffer stagingBuffer;
  vk::DeviceMemory stagingBufferMemory;
  CreateBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc,
               vk::MemoryPropertyFlagBits::eHostVisible |
                   vk::MemoryPropertyFlagBits::eHostCoherent,
               &stagingBuffer, &stagingBufferMemory);
  void* data = device_.mapMemory(stagingBufferMemory, 0, imageSize);
  memcpy(data, pixels, static_cast<size_t>(imageSize));
  device_.unmapMemory(stagingBufferMemory);
  stbi_image_free(pixels);

  CreateImage(
      texWidth, texHeight, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal,
      vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
      vk::MemoryPropertyFlagBits::eDeviceLocal, texture_image_,
      texture_image_memory_);

  transitionImageLayout(texture_image_, vk::Format::eR8G8B8A8Srgb,
                        vk::ImageLayout::eUndefined,
                        vk::ImageLayout::eTransferDstOptimal);
  copyBufferToImage(stagingBuffer, texture_image_,
                    static_cast<uint32_t>(texWidth),
                    static_cast<uint32_t>(texHeight));
  transitionImageLayout(texture_image_, vk::Format::eR8G8B8A8Srgb,
                        vk::ImageLayout::eTransferDstOptimal,
                        vk::ImageLayout::eShaderReadOnlyOptimal);
}

vk::ImageView VulkanDemo::CreateImageView(vk::Image image, vk::Format format) {
  vk::ImageViewCreateInfo viewInfo;
  viewInfo.image = image;
  viewInfo.viewType = vk::ImageViewType::e2D;
  viewInfo.format = format;
  viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  return device_.createImageView(viewInfo);
}

void VulkanDemo::CreateTextureImageView() {
  texture_image_view_ =
      CreateImageView(texture_image_, vk::Format::eR8G8B8A8Srgb);
}

void VulkanDemo::createTextureSampler() {
  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;

  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

  samplerInfo.anisotropyEnable = VK_TRUE;
  samplerInfo.maxAnisotropy = 16;

  samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

  samplerInfo.unnormalizedCoordinates = VK_FALSE;

  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerInfo.mipLodBias = 0.0f;
  samplerInfo.minLod = 0.0f;
  samplerInfo.maxLod = 0.0f;

  // if (vkCreateSampler(device_, &samplerInfo, nullptr, &textureSampler) !=
  // VK_SUCCESS)
  // {
  //   throw std::runtime_error("failed to create texture sampler!");
  // }
}

void VulkanDemo::CreateVertexBuffer() {
  auto buffer_size = sizeof(kVertices[0]) * kVertices.size();
  CreateBuffer(buffer_size, vk::BufferUsageFlagBits::eVertexBuffer,
               vk::MemoryPropertyFlagBits::eHostVisible |
                   vk::MemoryPropertyFlagBits::eHostCoherent,
               &vertex_buffer_, &vertex_buffer_memory_);
  void* data = device_.mapMemory(vertex_buffer_memory_, 0, buffer_size);
  memcpy(data, kVertices.data(), buffer_size);
  device_.unmapMemory(vertex_buffer_memory_);
}

void VulkanDemo::CreateIndexBuffer() {
  auto buffer_size = sizeof(kIndices[0]) * kIndices.size();
  CreateBuffer(buffer_size, vk::BufferUsageFlagBits::eIndexBuffer,
               vk::MemoryPropertyFlagBits::eHostVisible |
                   vk::MemoryPropertyFlagBits::eHostCoherent,
               &index_buffer_, &index_buffer_memory_);
  void* data = device_.mapMemory(index_buffer_memory_, 0, buffer_size);
  memcpy(data, kIndices.data(), buffer_size);
  device_.unmapMemory(index_buffer_memory_);
}

void VulkanDemo::CreateUniformBuffers() {
  VkDeviceSize buffer_size = sizeof(UniformBufferObject);

  uniform_buffers_.resize(swap_chain_images_.size());
  uniform_buffers_memory_.resize(swap_chain_images_.size());

  for (size_t i = 0; i < swap_chain_images_.size(); i++) {
    CreateBuffer(buffer_size, vk::BufferUsageFlagBits::eUniformBuffer,
                 vk::MemoryPropertyFlagBits::eHostVisible |
                     vk::MemoryPropertyFlagBits::eHostCoherent,
                 &uniform_buffers_[i], &uniform_buffers_memory_[i]);
  }
}

void VulkanDemo::CreateDescriptorPool() {
  vk::DescriptorPoolSize pool_size(vk::DescriptorType::eUniformBuffer,
                                   (uint32_t)swap_chain_images_.size());
  vk::DescriptorPoolCreateInfo pool_info({}, swap_chain_images_.size(),
                                         pool_size);
  descriptor_pool_ = device_.createDescriptorPool(pool_info);
}

void VulkanDemo::CreateDescriptorSets() {
  std::vector<VkDescriptorSetLayout> layouts(swap_chain_images_.size(),
                                             descriptor_set_layout_);
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = descriptor_pool_;
  allocInfo.descriptorSetCount =
      static_cast<uint32_t>(swap_chain_images_.size());
  allocInfo.pSetLayouts = layouts.data();

  descriptor_sets_.resize(swap_chain_images_.size());
  if (vkAllocateDescriptorSets(device_, &allocInfo, descriptor_sets_.data()) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to allocate descriptor sets!");
  }

  for (size_t i = 0; i < swap_chain_images_.size(); i++) {
    VkDescriptorBufferInfo buffer_info{};
    buffer_info.buffer = uniform_buffers_[i];
    buffer_info.offset = 0;
    buffer_info.range = sizeof(UniformBufferObject);

    VkWriteDescriptorSet descriptor_write{};
    descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_write.dstSet = descriptor_sets_[i];
    descriptor_write.dstBinding = 0;
    descriptor_write.dstArrayElement = 0;

    descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptor_write.descriptorCount = 1;

    descriptor_write.pBufferInfo = &buffer_info;
    descriptor_write.pImageInfo = nullptr;        // Optional
    descriptor_write.pTexelBufferView = nullptr;  // Optional
    vkUpdateDescriptorSets(device_, 1, &descriptor_write, 0, nullptr);
  }
}

void VulkanDemo::CreateCommandPool() {
  QueueFamilyIndices queue_family_indices = FindQueueFamilies(physical_device_);

  VkCommandPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.queueFamilyIndex = queue_family_indices.graphics_family;

  if (vkCreateCommandPool(device_, &pool_info, nullptr, &command_pool_) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create command pool!");
  }
}

void VulkanDemo::CreateCommandBuffers() {
  vk::CommandBufferAllocateInfo alloc_info(command_pool_,
                                           vk::CommandBufferLevel::ePrimary,
                                           swap_chain_framebuffers_.size());
  command_buffers_ = device_.allocateCommandBuffers(alloc_info);

  for (size_t i = 0; i < command_buffers_.size(); i++) {
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

    if (vkBeginCommandBuffer(command_buffers_[i], &begin_info) != VK_SUCCESS) {
      throw std::runtime_error("failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = render_pass_;
    render_pass_info.framebuffer = swap_chain_framebuffers_[i];
    render_pass_info.renderArea.offset = {0, 0};
    render_pass_info.renderArea.extent = (VkExtent2D)swap_chain_extent_;

    VkClearValue clear_color = {0.0f, 0.0f, 0.0f, 1.0f};
    render_pass_info.clearValueCount = 1;
    render_pass_info.pClearValues = &clear_color;

    vkCmdBeginRenderPass(command_buffers_[i], &render_pass_info,
                         VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(command_buffers_[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
                      graphics_pipeline_);

    VkBuffer vertex_buffers[] = {vertex_buffer_};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(command_buffers_[i], 0, 1, vertex_buffers, offsets);
    vkCmdBindIndexBuffer(command_buffers_[i], index_buffer_, 0,
                         VK_INDEX_TYPE_UINT16);
    vkCmdBindDescriptorSets(command_buffers_[i],
                            VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_,
                            0, 1, &descriptor_sets_[i], 0, nullptr);
    vkCmdDrawIndexed(command_buffers_[i],
                     static_cast<uint32_t>(kIndices.size()), 1, 0, 0, 0);

    vkCmdEndRenderPass(command_buffers_[i]);

    if (vkEndCommandBuffer(command_buffers_[i]) != VK_SUCCESS) {
      throw std::runtime_error("failed to record command buffer!");
    }
  }
}

void VulkanDemo::CreateSyncObjects() {
  vk::SemaphoreCreateInfo semaphore_info;
  vk::FenceCreateInfo fence_info(vk::FenceCreateFlagBits::eSignaled);
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    image_available_semaphores_.emplace_back(
        device_.createSemaphore(semaphore_info));
    render_finished_semaphores_.emplace_back(
        device_.createSemaphore(semaphore_info));
    in_flight_fences_.emplace_back(device_.createFence(fence_info));
  }
}

void VulkanDemo::UpdateUniformBuffer(uint32_t current_image) {
  static auto startTime = std::chrono::high_resolution_clock::now();

  auto currentTime = std::chrono::high_resolution_clock::now();
  float time = std::chrono::duration<float, std::chrono::seconds::period>(
                   currentTime - startTime)
                   .count();

  UniformBufferObject ubo{};
  ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f),
                          glm::vec3(0.0f, 0.0f, 1.0f));
  ubo.view =
      glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                  glm::vec3(0.0f, 0.0f, 1.0f));
  ubo.proj = glm::perspective(
      glm::radians(45.0f),
      swap_chain_extent_.width / (float)swap_chain_extent_.height, 0.1f, 10.0f);
  ubo.proj[1][1] *= -1;
  void* data =
      device_.mapMemory(uniform_buffers_memory_[current_image], 0, sizeof(ubo));
  memcpy(data, &ubo, sizeof(ubo));
  device_.unmapMemory(uniform_buffers_memory_[current_image]);
}

void VulkanDemo::DrawFrame() {
  vk::Result result =
      device_.waitForFences(in_flight_fences_[current_frame_], VK_TRUE,
                            std::numeric_limits<uint64_t>::max());

  uint32_t image_index;
  result = device_.acquireNextImageKHR(
      swap_chain_, std::numeric_limits<uint64_t>::max(),
      image_available_semaphores_[current_frame_], {}, &image_index);

  if (result == vk::Result::eErrorOutOfDateKHR) {
    RecreateSwapChain();
    return;
  } else if (result != vk::Result::eSuccess &&
             result != vk::Result::eSuboptimalKHR) {
    throw std::runtime_error("failed to acquire swap chain image!");
  }

  UpdateUniformBuffer(image_index);

  vk::SubmitInfo submit_info;
  submit_info.setWaitSemaphores(image_available_semaphores_[current_frame_]);
  vk::PipelineStageFlags stage_flags = vk::PipelineStageFlagBits::eColorAttachmentOutput;
  submit_info.setPWaitDstStageMask(&stage_flags);
  submit_info.setCommandBuffers(command_buffers_[image_index]);
  submit_info.setSignalSemaphores(render_finished_semaphores_[current_frame_]);
  device_.resetFences(in_flight_fences_[current_frame_]);

  graphics_queue_.submit(submit_info, in_flight_fences_[current_frame_]);

  vk::PresentInfoKHR present_info;
  present_info.setWaitSemaphores(render_finished_semaphores_[current_frame_]);
  present_info.setSwapchains(swap_chain_);
  present_info.setImageIndices(image_index);

  result = present_queue_.presentKHR(present_info);
  if (result == vk::Result::eErrorOutOfDateKHR ||
      result == vk::Result::eSuboptimalKHR || framebuffer_resized_) {
    framebuffer_resized_ = false;
    RecreateSwapChain();
  } else if (result != vk::Result::eSuccess) {
    throw std::runtime_error("failed to present swap chain image!");
  }

  current_frame_ = (current_frame_ + 1) % MAX_FRAMES_IN_FLIGHT;
}

vk::ShaderModule VulkanDemo::CreateShaderModule(
    const std::vector<uint32_t>& code) {
  vk::ShaderModuleCreateInfo create_info({}, code);
  vk::ShaderModule shader_module = device_.createShaderModule(create_info);

  return shader_module;
}

vk::SurfaceFormatKHR VulkanDemo::ChooseSwapSurfaceFormat(
    const std::vector<vk::SurfaceFormatKHR>& available_formats) {
  if (available_formats.size() == 1 &&
      available_formats[0].format == vk::Format::eUndefined) {
    return {vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear};
  }

  for (const auto& available_format : available_formats) {
    if (available_format.format == vk::Format::eB8G8R8A8Unorm &&
        available_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
      return available_format;
    }
  }

  return available_formats[0];
}

vk::PresentModeKHR VulkanDemo::ChooseSwapPresentMode(
    const std::vector<vk::PresentModeKHR> available_present_modes) {
  vk::PresentModeKHR best_mode = vk::PresentModeKHR::eFifo;

  for (const auto& available_present_mode : available_present_modes) {
    if (available_present_mode == vk::PresentModeKHR::eMailbox) {
      return available_present_mode;
    } else if (available_present_mode == vk::PresentModeKHR::eImmediate) {
      best_mode = available_present_mode;
    }
  }

  return best_mode;
}

vk::Extent2D VulkanDemo::ChooseSwapExtent(
    const vk::SurfaceCapabilitiesKHR& capabilities) {
  if (capabilities.currentExtent.width !=
      std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
  } else {
    int width, height;
    glfwGetFramebufferSize(window_, &width, &height);

    vk::Extent2D actual_extent = {static_cast<uint32_t>(width),
                                  static_cast<uint32_t>(height)};

    actual_extent.width = std::max(
        capabilities.minImageExtent.width,
        std::min(capabilities.maxImageExtent.width, actual_extent.width));
    actual_extent.height = std::max(
        capabilities.minImageExtent.height,
        std::min(capabilities.maxImageExtent.height, actual_extent.height));

    return actual_extent;
  }
}

SwapChainSupportDetails VulkanDemo::QuerySwapChainSupport(
    vk::PhysicalDevice device) {
  return {device.getSurfaceCapabilitiesKHR(surface_),
          device.getSurfaceFormatsKHR(surface_),
          device.getSurfacePresentModesKHR(surface_)};
}

bool VulkanDemo::IsDeviceSuitable(vk::PhysicalDevice device) {
  QueueFamilyIndices indices = FindQueueFamilies(device);

  bool extensions_supported = CheckDeviceExtensionSupport(device);

  bool swap_chain_adequate = false;
  if (extensions_supported) {
    SwapChainSupportDetails swap_chain_support = QuerySwapChainSupport(device);
    swap_chain_adequate = !swap_chain_support.formats.empty() &&
                          !swap_chain_support.present_modes.empty();
  }

  return indices.is_complete() && extensions_supported && swap_chain_adequate;
}

bool VulkanDemo::CheckDeviceExtensionSupport(vk::PhysicalDevice device) {
  uint32_t extension_count;
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count,
                                       nullptr);

  std::vector<VkExtensionProperties> available_extensions(extension_count);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count,
                                       available_extensions.data());

  std::set<std::string> required_extensions(kDeviceExtensions.begin(),
                                            kDeviceExtensions.end());

  for (const auto& extension : available_extensions) {
    required_extensions.erase(extension.extensionName);
  }

  return required_extensions.empty();
}

QueueFamilyIndices VulkanDemo::FindQueueFamilies(vk::PhysicalDevice device) {
  QueueFamilyIndices indices;

  uint32_t queue_family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                           nullptr);

  std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                           queue_families.data());

  int i = 0;
  for (const auto& queue_family : queue_families) {
    if (queue_family.queueCount > 0 &&
        queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      indices.graphics_family = i;
    }

    VkBool32 present_support = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &present_support);

    if (queue_family.queueCount > 0 && present_support) {
      indices.present_family = i;
    }

    if (indices.is_complete()) {
      break;
    }

    i++;
  }

  return indices;
}

std::vector<const char*> VulkanDemo::GetRequiredExtensions() {
  uint32_t glfwExtensionCount = 0;
  const char** glfwExtensions;
  glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

  std::vector<const char*> extensions(glfwExtensions,
                                      glfwExtensions + glfwExtensionCount);

  if (kEnableValidationLayers) {
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  return extensions;
}

bool VulkanDemo::CheckValidationLayerSupport() {
  uint32_t layer_count;
  vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

  std::vector<VkLayerProperties> available_layers(layer_count);
  vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

  for (const char* layer_name : kValidationLayers) {
    bool layer_found = false;

    for (const auto& layerProperties : available_layers) {
      if (strcmp(layer_name, layerProperties.layerName) == 0) {
        layer_found = true;
        break;
      }
    }

    if (!layer_found) {
      return false;
    }
  }

  return true;
}

uint32_t VulkanDemo::FindMemoryType(uint32_t type_filter,
                                    VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties mem_properties;
  vkGetPhysicalDeviceMemoryProperties(physical_device_, &mem_properties);
  for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
    if (type_filter & (1 << i) && (mem_properties.memoryTypes[i].propertyFlags &
                                   properties) == properties) {
      return i;
    }
  }

  throw std::runtime_error("failed to find suitable memory type!");
}

VkCommandBuffer VulkanDemo::beginSingleTimeCommands() {
  vk::CommandBufferAllocateInfo alloc_info(command_pool_,
                                           vk::CommandBufferLevel::ePrimary, 1);
  auto command_buffer = device_.allocateCommandBuffers(alloc_info)[0];

  vk::CommandBufferBeginInfo begin_info(
      vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
  command_buffer.begin(begin_info);

  return command_buffer;
}

void VulkanDemo::endSingleTimeCommands(vk::CommandBuffer command_buffer) {
  command_buffer.end();

  vk::SubmitInfo submit_info;
  submit_info.setCommandBuffers(command_buffer);
  graphics_queue_.submit(submit_info);
  graphics_queue_.waitIdle();

  device_.freeCommandBuffers(command_pool_, command_buffer);
}

void VulkanDemo::copyBuffer(VkBuffer src_buffer,
                            VkBuffer dst_buffer,
                            VkDeviceSize size) {
  vk::CommandBuffer command_buffer = beginSingleTimeCommands();

  vk::BufferCopy copy_region(0, 0, size);
  command_buffer.copyBuffer(src_buffer, dst_buffer, copy_region);
  endSingleTimeCommands(command_buffer);
}

void VulkanDemo::transitionImageLayout(vk::Image image,
                                       vk::Format format,
                                       vk::ImageLayout oldLayout,
                                       vk::ImageLayout newLayout) {
  vk::CommandBuffer commandBuffer = beginSingleTimeCommands();
  vk::ImageMemoryBarrier barrier;

  barrier.setOldLayout(oldLayout)
      .setNewLayout(newLayout)
      .setImage(image)
      .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});

  vk::PipelineStageFlags sourceStage;
  vk::PipelineStageFlags destinationStage;
  if (oldLayout == vk::ImageLayout::eUndefined &&
      newLayout == vk::ImageLayout::eTransferDstOptimal) {
    barrier.setSrcAccessMask({});
    barrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
    sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
    destinationStage = vk::PipelineStageFlagBits::eTransfer;
  } else if (oldLayout == vk::ImageLayout::eTransferDstOptimal &&
             newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
    barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
    barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
    sourceStage = vk::PipelineStageFlagBits::eTransfer;
    destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
  } else {
    throw std::invalid_argument("unsupported layout transition!");
  }

  commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, {}, {},
                                barrier);
  endSingleTimeCommands(commandBuffer);
}

void VulkanDemo::copyBufferToImage(vk::Buffer buffer,
                                   vk::Image image,
                                   uint32_t width,
                                   uint32_t height) {
  vk::CommandBuffer commandBuffer = beginSingleTimeCommands();
  vk::BufferImageCopy region;
  region.setBufferOffset(0)
      .setBufferRowLength(0)
      .setBufferImageHeight(0)
      .setImageSubresource({vk::ImageAspectFlagBits::eColor, 0, 0, 1})
      .setImageOffset({0, 0, 0})
      .setImageExtent({width, height, 1});
  commandBuffer.copyBufferToImage(buffer, image,
                                  vk::ImageLayout::eTransferDstOptimal, region);

  endSingleTimeCommands(commandBuffer);
}

int main() {
  VulkanDemo app;

  try {
    app.run();
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

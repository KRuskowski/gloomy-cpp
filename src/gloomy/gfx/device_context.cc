#include "gloomy/gfx/device_context.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

namespace gloomy::gfx {

namespace {

constexpr const char *kValidationLayer = "VK_LAYER_KHRONOS_validation";

auto HasLayer(const char *name) -> bool {
  std::uint32_t count = 0;
  vkEnumerateInstanceLayerProperties(&count, nullptr);
  std::vector<VkLayerProperties> layers(count);
  vkEnumerateInstanceLayerProperties(&count, layers.data());
  for (const VkLayerProperties &layer : layers) {
    if (std::strcmp(layer.layerName, name) == 0) {
      return true;
    }
  }
  return false;
}

auto HasInstanceExtension(const char *name) -> bool {
  std::uint32_t count = 0;
  vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
  std::vector<VkExtensionProperties> extensions(count);
  vkEnumerateInstanceExtensionProperties(nullptr, &count, extensions.data());
  for (const VkExtensionProperties &e : extensions) {
    if (std::strcmp(e.extensionName, name) == 0) {
      return true;
    }
  }
  return false;
}

// Ranks device types: discrete first, CPU last.
auto TypeRank(VkPhysicalDeviceType type) -> int {
  switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
      return 0;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
      return 1;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
      return 2;
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
      return 3;
    default:
      return 4;
  }
}

}  // namespace

void CheckVk(VkResult result, const char *what) {
  if (result != VK_SUCCESS) {
    std::fprintf(stderr, "gloomy: %s failed: VkResult %d\n", what, result);
    std::abort();
  }
}

// Buffer

Buffer::Buffer(Buffer &&other) noexcept
    : device_(std::exchange(other.device_, VK_NULL_HANDLE)),
      buffer_(std::exchange(other.buffer_, VK_NULL_HANDLE)),
      memory_(std::exchange(other.memory_, VK_NULL_HANDLE)),
      size_(std::exchange(other.size_, 0)),
      mapped_(std::exchange(other.mapped_, nullptr)) {}

auto Buffer::operator=(Buffer &&other) noexcept -> Buffer & {
  if (this != &other) {
    Reset();
    device_ = std::exchange(other.device_, VK_NULL_HANDLE);
    buffer_ = std::exchange(other.buffer_, VK_NULL_HANDLE);
    memory_ = std::exchange(other.memory_, VK_NULL_HANDLE);
    size_ = std::exchange(other.size_, 0);
    mapped_ = std::exchange(other.mapped_, nullptr);
  }
  return *this;
}

Buffer::~Buffer() {
  Reset();
}

void Buffer::Reset() {
  if (device_ == VK_NULL_HANDLE) {
    return;
  }
  if (mapped_ != nullptr) {
    vkUnmapMemory(device_, memory_);
  }
  vkDestroyBuffer(device_, buffer_, nullptr);
  vkFreeMemory(device_, memory_, nullptr);
  device_ = VK_NULL_HANDLE;
  buffer_ = VK_NULL_HANDLE;
  memory_ = VK_NULL_HANDLE;
  mapped_ = nullptr;
  size_ = 0;
}

// Image2D

Image2D::Image2D(Image2D &&other) noexcept
    : device_(std::exchange(other.device_, VK_NULL_HANDLE)),
      image_(std::exchange(other.image_, VK_NULL_HANDLE)),
      memory_(std::exchange(other.memory_, VK_NULL_HANDLE)),
      view_(std::exchange(other.view_, VK_NULL_HANDLE)),
      format_(other.format_),
      extent_(other.extent_),
      layers_(other.layers_) {}

auto Image2D::operator=(Image2D &&other) noexcept -> Image2D & {
  if (this != &other) {
    Reset();
    device_ = std::exchange(other.device_, VK_NULL_HANDLE);
    image_ = std::exchange(other.image_, VK_NULL_HANDLE);
    memory_ = std::exchange(other.memory_, VK_NULL_HANDLE);
    view_ = std::exchange(other.view_, VK_NULL_HANDLE);
    format_ = other.format_;
    extent_ = other.extent_;
    layers_ = other.layers_;
  }
  return *this;
}

Image2D::~Image2D() {
  Reset();
}

void Image2D::Reset() {
  if (device_ == VK_NULL_HANDLE) {
    return;
  }
  vkDestroyImageView(device_, view_, nullptr);
  vkDestroyImage(device_, image_, nullptr);
  vkFreeMemory(device_, memory_, nullptr);
  device_ = VK_NULL_HANDLE;
  image_ = VK_NULL_HANDLE;
  memory_ = VK_NULL_HANDLE;
  view_ = VK_NULL_HANDLE;
}

// DeviceContext

auto DeviceContext::Create(const DeviceOptions &options)
    -> std::expected<std::unique_ptr<DeviceContext>, std::string> {
  std::unique_ptr<DeviceContext> ctx(new DeviceContext());

  // Instance.
  std::vector<const char *> layers;
  std::vector<const char *> extensions;
  for (const std::string &e : options.instance_extensions) {
    extensions.push_back(e.c_str());
  }
  const bool validation = options.validation && HasLayer(kValidationLayer) &&
                          HasInstanceExtension("VK_EXT_debug_utils");
  if (options.validation && !validation) {
    std::fprintf(stderr,
                 "gloomy: validation requested but %s is not installed\n",
                 kValidationLayer);
  }
  if (validation) {
    layers.push_back(kValidationLayer);
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  VkApplicationInfo app{};
  app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app.pApplicationName = options.app_name.c_str();
  app.pEngineName = "gloomy";
  app.apiVersion = VK_API_VERSION_1_3;

  VkDebugUtilsMessengerCreateInfoEXT messenger_info{};
  messenger_info.sType =
      VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  messenger_info.messageSeverity =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  messenger_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
  messenger_info.pfnUserCallback = &DeviceContext::OnDebugMessage;
  messenger_info.pUserData = ctx.get();

  VkInstanceCreateInfo instance_info{};
  instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance_info.pApplicationInfo = &app;
  instance_info.enabledLayerCount = static_cast<std::uint32_t>(layers.size());
  instance_info.ppEnabledLayerNames = layers.data();
  instance_info.enabledExtensionCount =
      static_cast<std::uint32_t>(extensions.size());
  instance_info.ppEnabledExtensionNames = extensions.data();
  if (validation) {
    // Also reports problems during instance creation and destruction.
    instance_info.pNext = &messenger_info;
  }
  if (VkResult r = vkCreateInstance(&instance_info, nullptr, &ctx->instance_);
      r != VK_SUCCESS) {
    return std::unexpected("vkCreateInstance failed: " + std::to_string(r));
  }

  if (validation) {
    auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(ctx->instance_,
                              "vkCreateDebugUtilsMessengerEXT"));
    if (create != nullptr) {
      create(ctx->instance_, &messenger_info, nullptr, &ctx->messenger_);
    }
  }

  if (options.create_surface) {
    ctx->surface_ = options.create_surface(ctx->instance_);
    if (ctx->surface_ == VK_NULL_HANDLE) {
      return std::unexpected("creating the window surface failed");
    }
  }

  // Physical device: needs Vulkan 1.3, dynamic rendering, synchronization2
  // and a graphics queue (that presents, if there is a surface).
  std::uint32_t count = 0;
  vkEnumeratePhysicalDevices(ctx->instance_, &count, nullptr);
  std::vector<VkPhysicalDevice> devices(count);
  vkEnumeratePhysicalDevices(ctx->instance_, &count, devices.data());

  int wanted = options.device_index;
  if (const char *env = std::getenv("GLOOMY_DEVICE")) {
    wanted = std::atoi(env);
  }

  int best_rank = 1 << 20;
  for (std::uint32_t i = 0; i < count; ++i) {
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(devices[i], &props);
    if (props.apiVersion < VK_API_VERSION_1_3) {
      continue;
    }
    VkPhysicalDeviceVulkan13Features f13{};
    f13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    VkPhysicalDeviceFeatures2 features{};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features.pNext = &f13;
    vkGetPhysicalDeviceFeatures2(devices[i], &features);
    if (f13.dynamicRendering == VK_FALSE || f13.synchronization2 == VK_FALSE) {
      continue;
    }
    std::uint32_t family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(
        devices[i], &family_count, nullptr);
    std::vector<VkQueueFamilyProperties> families(family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(
        devices[i], &family_count, families.data());
    int family = -1;
    for (std::uint32_t q = 0; q < family_count; ++q) {
      if ((families[q].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) {
        continue;
      }
      if (ctx->surface_ != VK_NULL_HANDLE) {
        VkBool32 present = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(
            devices[i], q, ctx->surface_, &present);
        if (present == VK_FALSE) {
          continue;
        }
      }
      family = static_cast<int>(q);
      break;
    }
    if (family < 0) {
      continue;
    }
    const int rank =
        static_cast<int>(i) == wanted ? -1 : TypeRank(props.deviceType);
    if (rank < best_rank) {
      best_rank = rank;
      ctx->physical_ = devices[i];
      ctx->queue_family_ = static_cast<std::uint32_t>(family);
      ctx->device_name_ = props.deviceName;
    }
  }
  if (ctx->physical_ == VK_NULL_HANDLE) {
    return std::unexpected("no Vulkan 1.3 device with dynamic rendering");
  }
  vkGetPhysicalDeviceMemoryProperties(ctx->physical_, &ctx->memory_props_);

  // Logical device.
  const float priority = 1.0F;
  VkDeviceQueueCreateInfo queue_info{};
  queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_info.queueFamilyIndex = ctx->queue_family_;
  queue_info.queueCount = 1;
  queue_info.pQueuePriorities = &priority;

  VkPhysicalDeviceVulkan13Features f13{};
  f13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  f13.dynamicRendering = VK_TRUE;
  f13.synchronization2 = VK_TRUE;

  std::vector<const char *> device_extensions;
  if (ctx->surface_ != VK_NULL_HANDLE) {
    device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
  }

  VkDeviceCreateInfo device_info{};
  device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_info.pNext = &f13;
  device_info.queueCreateInfoCount = 1;
  device_info.pQueueCreateInfos = &queue_info;
  device_info.enabledExtensionCount =
      static_cast<std::uint32_t>(device_extensions.size());
  device_info.ppEnabledExtensionNames = device_extensions.data();
  if (VkResult r =
          vkCreateDevice(ctx->physical_, &device_info, nullptr, &ctx->device_);
      r != VK_SUCCESS) {
    return std::unexpected("vkCreateDevice failed: " + std::to_string(r));
  }
  vkGetDeviceQueue(ctx->device_, ctx->queue_family_, 0, &ctx->queue_);

  VkCommandPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
  pool_info.queueFamilyIndex = ctx->queue_family_;
  CheckVk(
      vkCreateCommandPool(ctx->device_, &pool_info, nullptr, &ctx->setup_pool_),
      "vkCreateCommandPool");
  return ctx;
}

auto DeviceContext::Wrap(const ExternalDevice &external)
    -> std::unique_ptr<DeviceContext> {
  std::unique_ptr<DeviceContext> ctx(new DeviceContext());
  ctx->owned_ = false;
  ctx->instance_ = external.instance;
  ctx->physical_ = external.physical_device;
  ctx->device_ = external.device;
  ctx->queue_ = external.queue;
  ctx->queue_family_ = external.queue_family;
  VkPhysicalDeviceProperties props{};
  vkGetPhysicalDeviceProperties(ctx->physical_, &props);
  ctx->device_name_ = props.deviceName;
  vkGetPhysicalDeviceMemoryProperties(ctx->physical_, &ctx->memory_props_);
  VkCommandPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
  pool_info.queueFamilyIndex = ctx->queue_family_;
  CheckVk(
      vkCreateCommandPool(ctx->device_, &pool_info, nullptr, &ctx->setup_pool_),
      "vkCreateCommandPool");
  return ctx;
}

DeviceContext::~DeviceContext() {
  if (device_ != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(device_);
    vkDestroyCommandPool(device_, setup_pool_, nullptr);
  }
  if (!owned_) {
    return;
  }
  if (device_ != VK_NULL_HANDLE) {
    vkDestroyDevice(device_, nullptr);
  }
  if (surface_ != VK_NULL_HANDLE) {
    vkDestroySurfaceKHR(instance_, surface_, nullptr);
  }
  if (messenger_ != VK_NULL_HANDLE) {
    auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
    if (destroy != nullptr) {
      destroy(instance_, messenger_, nullptr);
    }
  }
  if (instance_ != VK_NULL_HANDLE) {
    vkDestroyInstance(instance_, nullptr);
  }
}

VKAPI_ATTR auto VKAPI_CALL DeviceContext::OnDebugMessage(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT /*types*/,
    const VkDebugUtilsMessengerCallbackDataEXT *data, void *user) -> VkBool32 {
  auto *ctx = static_cast<DeviceContext *>(user);
  const char *level =
      (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0
          ? "error"
          : "warning";
  std::fprintf(stderr, "gloomy: validation %s: %s\n", level, data->pMessage);
  if (ctx != nullptr) {
    ctx->messages_.fetch_add(1);
    const std::lock_guard lock(ctx->message_mutex_);
    ctx->message_log_.emplace_back(data->pMessage);
  }
  return VK_FALSE;
}

auto DeviceContext::ValidationMessages() const -> std::vector<std::string> {
  const std::lock_guard lock(message_mutex_);
  return message_log_;
}

auto DeviceContext::FindMemoryType(std::uint32_t bits,
                                   VkMemoryPropertyFlags props) const
    -> std::uint32_t {
  for (std::uint32_t i = 0; i < memory_props_.memoryTypeCount; ++i) {
    if ((bits & (1U << i)) != 0 &&
        (memory_props_.memoryTypes[i].propertyFlags & props) == props) {
      return i;
    }
  }
  std::fprintf(stderr, "gloomy: no memory type for flags %u\n", props);
  std::abort();
}

auto DeviceContext::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                 bool host_visible) -> Buffer {
  Buffer buffer;
  buffer.device_ = device_;
  buffer.size_ = size;
  VkBufferCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  info.size = size;
  info.usage = usage;
  info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  CheckVk(vkCreateBuffer(device_, &info, nullptr, &buffer.buffer_),
          "vkCreateBuffer");
  VkMemoryRequirements req{};
  vkGetBufferMemoryRequirements(device_, buffer.buffer_, &req);
  VkMemoryAllocateInfo alloc{};
  alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc.allocationSize = req.size;
  alloc.memoryTypeIndex =
      FindMemoryType(req.memoryTypeBits,
                     host_visible ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                                  : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  CheckVk(vkAllocateMemory(device_, &alloc, nullptr, &buffer.memory_),
          "vkAllocateMemory");
  CheckVk(vkBindBufferMemory(device_, buffer.buffer_, buffer.memory_, 0),
          "vkBindBufferMemory");
  if (host_visible) {
    CheckVk(vkMapMemory(device_, buffer.memory_, 0, size, 0, &buffer.mapped_),
            "vkMapMemory");
  }
  return buffer;
}

auto DeviceContext::CreateImage(VkExtent2D extent, std::uint32_t layers,
                                VkFormat format, VkImageUsageFlags usage,
                                VkImageViewType view_type) -> Image2D {
  Image2D image;
  image.device_ = device_;
  image.format_ = format;
  image.extent_ = extent;
  image.layers_ = layers;
  VkImageCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  info.imageType = VK_IMAGE_TYPE_2D;
  info.format = format;
  info.extent = {extent.width, extent.height, 1};
  info.mipLevels = 1;
  info.arrayLayers = layers;
  info.samples = VK_SAMPLE_COUNT_1_BIT;
  info.tiling = VK_IMAGE_TILING_OPTIMAL;
  info.usage = usage;
  info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  CheckVk(vkCreateImage(device_, &info, nullptr, &image.image_),
          "vkCreateImage");
  VkMemoryRequirements req{};
  vkGetImageMemoryRequirements(device_, image.image_, &req);
  VkMemoryAllocateInfo alloc{};
  alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc.allocationSize = req.size;
  alloc.memoryTypeIndex =
      FindMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  CheckVk(vkAllocateMemory(device_, &alloc, nullptr, &image.memory_),
          "vkAllocateMemory");
  CheckVk(vkBindImageMemory(device_, image.image_, image.memory_, 0),
          "vkBindImageMemory");
  VkImageViewCreateInfo view{};
  view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view.image = image.image_;
  view.viewType = view_type;
  view.format = format;
  view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, layers};
  CheckVk(vkCreateImageView(device_, &view, nullptr, &image.view_),
          "vkCreateImageView");
  return image;
}

void DeviceContext::SubmitAndWait(
    const std::function<void(VkCommandBuffer)> &record) {
  VkCommandBufferAllocateInfo alloc{};
  alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc.commandPool = setup_pool_;
  alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc.commandBufferCount = 1;
  VkCommandBuffer cmd = VK_NULL_HANDLE;
  CheckVk(vkAllocateCommandBuffers(device_, &alloc, &cmd),
          "vkAllocateCommandBuffers");
  VkCommandBufferBeginInfo begin{};
  begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  CheckVk(vkBeginCommandBuffer(cmd, &begin), "vkBeginCommandBuffer");
  record(cmd);
  CheckVk(vkEndCommandBuffer(cmd), "vkEndCommandBuffer");

  VkFenceCreateInfo fence_info{};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  VkFence fence = VK_NULL_HANDLE;
  CheckVk(vkCreateFence(device_, &fence_info, nullptr, &fence),
          "vkCreateFence");
  VkCommandBufferSubmitInfo cmd_info{};
  cmd_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
  cmd_info.commandBuffer = cmd;
  VkSubmitInfo2 submit{};
  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
  submit.commandBufferInfoCount = 1;
  submit.pCommandBufferInfos = &cmd_info;
  CheckVk(vkQueueSubmit2(queue_, 1, &submit, fence), "vkQueueSubmit2");
  CheckVk(vkWaitForFences(device_, 1, &fence, VK_TRUE, UINT64_MAX),
          "vkWaitForFences");
  vkDestroyFence(device_, fence, nullptr);
  vkFreeCommandBuffers(device_, setup_pool_, 1, &cmd);
}

void DeviceContext::WaitIdle() const {
  vkDeviceWaitIdle(device_);
}

}  // namespace gloomy::gfx

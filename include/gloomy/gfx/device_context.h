#ifndef GLOOMY_GFX_DEVICE_CONTEXT_H_
#define GLOOMY_GFX_DEVICE_CONTEXT_H_

#include <vulkan/vulkan.h>

#include <atomic>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace gloomy::gfx {

/// Aborts with a message if `result` is not VK_SUCCESS. For calls whose
/// failure leaves nothing sensible to do (out of memory, device lost).
void CheckVk(VkResult result, const char *what);

/// How DeviceContext::Create() sets Vulkan up.
struct DeviceOptions {
  std::string app_name = "gloomy";
  /// Enable VK_LAYER_KHRONOS_validation if it is installed.
  bool validation = false;
  /// Extra instance extensions, e.g. the ones SDL needs for a surface.
  std::vector<std::string> instance_extensions;
  /// Creates the presentation surface once the instance exists. When set,
  /// the chosen queue family must present to it and the swapchain
  /// extension is enabled.
  std::function<VkSurfaceKHR(VkInstance)> create_surface;
  /// Prefer this physical device index; -1 picks the first discrete GPU,
  /// else the first device that works. GLOOMY_DEVICE overrides it.
  int device_index = -1;
};

/// Handles of a device owned by a host engine, for DeviceContext::Wrap().
struct ExternalDevice {
  VkInstance instance = VK_NULL_HANDLE;
  VkPhysicalDevice physical_device = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;
  VkQueue queue = VK_NULL_HANDLE;
  std::uint32_t queue_family = 0;
};

/// A buffer with its memory. Move-only; destroyed with its context.
class Buffer {
 public:
  Buffer() = default;
  Buffer(const Buffer &) = delete;
  auto operator=(const Buffer &) -> Buffer & = delete;
  Buffer(Buffer &&other) noexcept;
  auto operator=(Buffer &&other) noexcept -> Buffer &;
  ~Buffer();

  auto handle() const -> VkBuffer { return buffer_; }

  auto size() const -> VkDeviceSize { return size_; }

  /// Host pointer for host-visible buffers, else nullptr.
  auto mapped() const -> void * { return mapped_; }

 private:
  friend class DeviceContext;

  void Reset();

  VkDevice device_ = VK_NULL_HANDLE;
  VkBuffer buffer_ = VK_NULL_HANDLE;
  VkDeviceMemory memory_ = VK_NULL_HANDLE;
  VkDeviceSize size_ = 0;
  void *mapped_ = nullptr;
};

/// A 2D (array) image with memory and a view. Move-only.
class Image2D {
 public:
  Image2D() = default;
  Image2D(const Image2D &) = delete;
  auto operator=(const Image2D &) -> Image2D & = delete;
  Image2D(Image2D &&other) noexcept;
  auto operator=(Image2D &&other) noexcept -> Image2D &;
  ~Image2D();

  auto handle() const -> VkImage { return image_; }

  auto view() const -> VkImageView { return view_; }

  auto format() const -> VkFormat { return format_; }

  auto extent() const -> VkExtent2D { return extent_; }

  auto layers() const -> std::uint32_t { return layers_; }

 private:
  friend class DeviceContext;

  void Reset();

  VkDevice device_ = VK_NULL_HANDLE;
  VkImage image_ = VK_NULL_HANDLE;
  VkDeviceMemory memory_ = VK_NULL_HANDLE;
  VkImageView view_ = VK_NULL_HANDLE;
  VkFormat format_ = VK_FORMAT_UNDEFINED;
  VkExtent2D extent_{};
  std::uint32_t layers_ = 1;
};

/// The Vulkan instance, device and graphics queue gloomy renders with.
/// Either created by gloomy or wrapping a host engine's device; in the
/// latter case nothing is destroyed here. See docs/design.md section 6.1.
class DeviceContext {
 public:
  /// Creates instance, device and queue.
  /// @return An error message if no usable device exists.
  static auto Create(const DeviceOptions &options)
      -> std::expected<std::unique_ptr<DeviceContext>, std::string>;

  /// Wraps a host engine's device. The host must have enabled Vulkan 1.3
  /// dynamic rendering and synchronization2.
  static auto Wrap(const ExternalDevice &external)
      -> std::unique_ptr<DeviceContext>;

  DeviceContext(const DeviceContext &) = delete;
  auto operator=(const DeviceContext &) -> DeviceContext & = delete;
  ~DeviceContext();

  auto instance() const -> VkInstance { return instance_; }

  auto physical_device() const -> VkPhysicalDevice { return physical_; }

  auto device() const -> VkDevice { return device_; }

  auto queue() const -> VkQueue { return queue_; }

  auto queue_family() const -> std::uint32_t { return queue_family_; }

  /// The presentation surface, if one was requested.
  auto surface() const -> VkSurfaceKHR { return surface_; }

  /// Name of the physical device, for logs.
  auto device_name() const -> const std::string & { return device_name_; }

  /// True when the validation layer is active.
  auto validation_enabled() const -> bool { return messenger_ != nullptr; }

  /// Validation errors and warnings reported so far. Tests assert zero.
  auto validation_message_count() const -> int { return messages_.load(); }

  /// The validation messages reported so far.
  auto ValidationMessages() const -> std::vector<std::string>;

  /// Creates a buffer. Host-visible buffers are persistently mapped and
  /// coherent.
  auto CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                    bool host_visible) -> Buffer;

  /// Creates a device-local 2D image (array) with a view.
  auto CreateImage(VkExtent2D extent, std::uint32_t layers, VkFormat format,
                   VkImageUsageFlags usage,
                   VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D)
      -> Image2D;

  /// Records commands into a temporary command buffer, submits them and
  /// waits. For setup and tests, never per frame.
  void SubmitAndWait(const std::function<void(VkCommandBuffer)> &record);

  /// Waits until the device is idle.
  void WaitIdle() const;

 private:
  DeviceContext() = default;

  auto FindMemoryType(std::uint32_t bits, VkMemoryPropertyFlags props) const
      -> std::uint32_t;

  static VKAPI_ATTR auto VKAPI_CALL OnDebugMessage(
      VkDebugUtilsMessageSeverityFlagBitsEXT severity,
      VkDebugUtilsMessageTypeFlagsEXT types,
      const VkDebugUtilsMessengerCallbackDataEXT *data, void *user) -> VkBool32;

  bool owned_ = true;
  VkInstance instance_ = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT messenger_ = VK_NULL_HANDLE;
  VkSurfaceKHR surface_ = VK_NULL_HANDLE;
  VkPhysicalDevice physical_ = VK_NULL_HANDLE;
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueue queue_ = VK_NULL_HANDLE;
  std::uint32_t queue_family_ = 0;
  VkCommandPool setup_pool_ = VK_NULL_HANDLE;
  std::string device_name_;
  VkPhysicalDeviceMemoryProperties memory_props_{};
  std::atomic<int> messages_{0};
  mutable std::mutex message_mutex_;
  std::vector<std::string> message_log_;
};

}  // namespace gloomy::gfx

#endif  // GLOOMY_GFX_DEVICE_CONTEXT_H_

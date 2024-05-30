module;

#include <vulkan/vulkan_hpp_macros.hpp>
#include <wayland-client.h>

#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>

export module vulkan;
import vulkan_hpp;

static constexpr std::uint32_t NUM_FRAMES_IN_FLIGHT = 2;

struct FrameData {
    vk::UniqueCommandPool commandPool;
    vk::CommandBuffer commandBuffer;
    vk::UniqueFence fence;
    vk::UniqueSemaphore semaphore;
};

struct ImageData {
    vk::Image image;
    vk::UniqueSemaphore semaphore;
};

export class Renderer {
public:
    Renderer(wl_display *display, wl_surface *surface)
        :Renderer()
    {
        vk::defaultDispatchLoaderDynamic.init();

        const vk::ApplicationInfo applicationInfo {
            .applicationVersion = vk::ApiVersion13
        };
        const std::array instanceExtensions = {
            vk::KHRSurfaceExtensionName,
            vk::KHRWaylandSurfaceExtensionName,
        };
        const vk::InstanceCreateInfo instanceCreateInfo {
            .pApplicationInfo = &applicationInfo,
            .enabledExtensionCount = instanceExtensions.size(),
            .ppEnabledExtensionNames = instanceExtensions.data(),
        };
        _instance = vk::createInstanceUnique(instanceCreateInfo);
        vk::defaultDispatchLoaderDynamic.init(*_instance);

        const vk::WaylandSurfaceCreateInfoKHR surfaceCreateInfo {
            .display = display,
            .surface = surface
        };
        _surface = _instance->createWaylandSurfaceKHRUnique(surfaceCreateInfo);

        const auto physicalDevices = _instance->enumeratePhysicalDevices();
        _physicalDevice = physicalDevices[0]; // FIXME
        const std::uint32_t queueFamilyIndex = 0; // FIXME

        const auto surfaceFormats = _physicalDevice.getSurfaceFormatsKHR(*_surface);
        _surfaceFormat = surfaceFormats[1]; // FIXME

        const float queuePriority = 0.0f;
        const vk::DeviceQueueCreateInfo queueCreateInfo {
            .queueFamilyIndex = queueFamilyIndex,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority
        };
        const std::array deviceExtensions = {
            vk::KHRSwapchainExtensionName,
        };
        const vk::DeviceCreateInfo deviceCreateInfo {
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &queueCreateInfo,
            .enabledExtensionCount = deviceExtensions.size(),
            .ppEnabledExtensionNames = deviceExtensions.data()
        };
        _device = _physicalDevice.createDeviceUnique(deviceCreateInfo);
        vk::defaultDispatchLoaderDynamic.init(*_device);
        
        _queue = _device->getQueue(queueFamilyIndex, 0);
        
        for (auto& frameData : _frameData) {
            const vk::CommandPoolCreateInfo commandPoolCreateInfo {
                .flags = vk::CommandPoolCreateFlagBits::eTransient
            };
            frameData.commandPool = _device->createCommandPoolUnique(commandPoolCreateInfo);

            const vk::CommandBufferAllocateInfo commandBufferAllocateInfo {
                .commandPool = *frameData.commandPool,
                .commandBufferCount = 1
            };
            const auto commandBuffers = _device->allocateCommandBuffers(commandBufferAllocateInfo);
            frameData.commandBuffer = commandBuffers[0];

            const vk::FenceCreateInfo fenceCreateInfo {
                .flags = vk::FenceCreateFlagBits::eSignaled
            };
            frameData.fence = _device->createFenceUnique(fenceCreateInfo);

            const vk::SemaphoreCreateInfo semaphoreCreateInfo;
            frameData.semaphore = _device->createSemaphoreUnique(semaphoreCreateInfo);
        }
    }

    Renderer(const Renderer&) = delete;
    Renderer(Renderer&&) noexcept = default;

    ~Renderer() {
        if (_device) {
            waitAllFences();
        }
    }

    Renderer& operator=(const Renderer&) = delete;
    Renderer& operator=(Renderer&&) noexcept = default;

    void render(std::uint8_t color) {
        if (!_swapchain) {
            rebuild_swapchain();
        }

        _frameIndex = (_frameIndex + 1) % _frameData.size();
        std::ignore = _device->waitForFences(*frame().fence, true, UINT32_MAX);

        try {
            const auto [acquireResult, imageIndex] = _device->acquireNextImageKHR(*_swapchain, UINT32_MAX, *frame().semaphore);
            _imageIndex = imageIndex;

            _device->resetCommandPool(*frame().commandPool);

            const vk::CommandBufferBeginInfo beginInfo {
                .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
            };
            frame().commandBuffer.begin(beginInfo);

            const vk::ImageMemoryBarrier initBarrier {
                .oldLayout = vk::ImageLayout::eUndefined,
                .newLayout = vk::ImageLayout::eTransferDstOptimal,
                .image = image().image,
                .subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
            };
            frame().commandBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer,
                vk::DependencyFlagBits{},
                nullptr, nullptr, initBarrier);

            const auto colorFloat = static_cast<float>(color) / std::numeric_limits<decltype(color)>::max();
            const vk::ClearColorValue clearColor {
                .float32 = {{ colorFloat, colorFloat, colorFloat, 1.0f }}
            };
            const vk::ImageSubresourceRange clearRange { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
            frame().commandBuffer.clearColorImage(
                image().image,
                vk::ImageLayout::eTransferDstOptimal,
                clearColor,
                clearRange);

            const vk::ImageMemoryBarrier finiBarrier{
                .oldLayout = vk::ImageLayout::eTransferDstOptimal,
                .newLayout = vk::ImageLayout::ePresentSrcKHR,
                .image = image().image,
                .subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
            };
            frame().commandBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eBottomOfPipe,
                vk::DependencyFlagBits{},
                nullptr, nullptr, finiBarrier);

            frame().commandBuffer.end();

            const vk::PipelineStageFlags waitStageMask = vk::PipelineStageFlagBits::eTransfer;
            const vk::SubmitInfo submitInfo {
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &*frame().semaphore,
                .pWaitDstStageMask = &waitStageMask,
                .commandBufferCount = 1,
                .pCommandBuffers = &frame().commandBuffer,
                .signalSemaphoreCount = 1,
                .pSignalSemaphores = &*image().semaphore,
            };
            _device->resetFences(*frame().fence);
            _queue.submit(submitInfo, *frame().fence);

            const vk::PresentInfoKHR presentInfo {
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &*image().semaphore,
                .swapchainCount = 1,
                .pSwapchains = &*_swapchain,
                .pImageIndices = &_imageIndex
            };
            const auto presentResult = _queue.presentKHR(presentInfo);

            if (acquireResult == vk::Result::eSuboptimalKHR || presentResult == vk::Result::eSuboptimalKHR) {
                rebuild_swapchain();
            }
        } catch (vk::OutOfDateKHRError) {
            rebuild_swapchain();
        }
    }

    void resize(std::pair<std::uint32_t, std::uint32_t> size) {
        const auto oldExtent = _desiredExtent;
        _desiredExtent = {size.first, size.second};

        if (oldExtent != _desiredExtent) {
            rebuild_swapchain();
        }
    }

private:
    Renderer() = default;

    FrameData& frame() {
        return _frameData[_frameIndex];
    }

    ImageData& image() {
        return _imageData[_imageIndex];
    }

    void rebuild_swapchain() {
        const auto surfaceCaps = _physicalDevice.getSurfaceCapabilitiesKHR(*_surface);

        auto desiredImageCount = std::max(surfaceCaps.minImageCount + 1, NUM_FRAMES_IN_FLIGHT + 1);
        if (surfaceCaps.maxImageCount) {
            desiredImageCount = std::min(surfaceCaps.maxImageCount, desiredImageCount);
        }

        vk::Extent2D desiredExtent = _desiredExtent;
        if (desiredExtent.width == -1) desiredExtent.width = surfaceCaps.currentExtent.width;
        if (desiredExtent.height == -1) desiredExtent.height = surfaceCaps.currentExtent.height;
        if (desiredExtent.width == -1 || desiredExtent.height == -1) {
            throw std::runtime_error("The current Presentation Engine does not report a currentExtent. The application must provide a size via resize().");
        }

        desiredExtent.width = std::min(std::max(desiredExtent.width, surfaceCaps.minImageExtent.width), surfaceCaps.maxImageExtent.width);
        desiredExtent.height = std::min(std::max(desiredExtent.height, surfaceCaps.minImageExtent.height), surfaceCaps.maxImageExtent.height);

        vk::CompositeAlphaFlagBitsKHR compositeAlpha;
        if (surfaceCaps.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eOpaque) {
            compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        } else if (surfaceCaps.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eInherit) {
            compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eInherit;
        } else {
            throw std::runtime_error("The current Presentation Engine does not support opaque swapchains");
        }

        const vk::SwapchainCreateInfoKHR swapchainCreateInfo {
            .surface = *_surface,
            .minImageCount = desiredImageCount,
            .imageFormat = _surfaceFormat.format,
            .imageColorSpace = _surfaceFormat.colorSpace,
            .imageExtent = desiredExtent,
            .imageArrayLayers = 1,
            .imageUsage = vk::ImageUsageFlagBits::eTransferDst,
            .preTransform = surfaceCaps.currentTransform,
            .compositeAlpha = compositeAlpha,
            .presentMode = vk::PresentModeKHR::eMailbox, // Always supported
            .clipped = true,
            .oldSwapchain = *_swapchain
        };
        _swapchain = _device->createSwapchainKHRUnique(swapchainCreateInfo);
        _surfaceExtent = desiredExtent;

        const auto swapchainImages = _device->getSwapchainImagesKHR(*_swapchain);
        
        waitAllFences();
        _imageData.resize(swapchainImages.size());

        for (std::size_t i = 0; i < swapchainImages.size(); ++i) {
            auto& imageData = _imageData[i];

            imageData.image = swapchainImages[i];

            const vk::SemaphoreCreateInfo semaphoreCreateInfo;
            imageData.semaphore = _device->createSemaphoreUnique(semaphoreCreateInfo);
        }
    }

    void waitAllFences() {
        std::array<vk::Fence, NUM_FRAMES_IN_FLIGHT> allFences;
        for (std::size_t i = 0; i < allFences.size(); ++i) {
            allFences[i] = *_frameData[i].fence;
        }
        std::ignore = _device->waitForFences(allFences, true, UINT32_MAX);
    }

private:
    vk::UniqueInstance _instance;
    vk::UniqueSurfaceKHR _surface;

    vk::PhysicalDevice _physicalDevice;
    vk::SurfaceFormatKHR _surfaceFormat;

    vk::UniqueDevice _device;
    vk::Queue _queue;

    std::array<FrameData, NUM_FRAMES_IN_FLIGHT> _frameData;
    std::size_t _frameIndex = 0;

    vk::UniqueSwapchainKHR _swapchain;
    vk::Extent2D _surfaceExtent, _desiredExtent = {UINT32_MAX, UINT32_MAX};
    std::vector<ImageData> _imageData;
    std::uint32_t _imageIndex;
};
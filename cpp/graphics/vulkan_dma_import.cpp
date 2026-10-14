#include "vulkan_video_image.hpp"
#include "vulkan_submission.hpp"
#include "vulkan_foreign_transition.hpp"
#include "vulkan_dma_layout.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <cstdio>
#include <stdexcept>
#include <vector>
#include <algorithm>
namespace lumaframevr {
void VulkanVideoImage::import_dma() {
    const auto& source = frame_.native_frame()->dmabuf;
    auto format = dma_buf_format(source.format, frame_.color().bits);
    if(format == VK_FORMAT_UNDEFINED)
        throw std::runtime_error("Unsupported DMA-BUF FourCC/bit depth");
    crop_ = dma_buf_crop(source.width, source.height, {source.crop[0], source.crop[1], source.crop[2], source.crop[3]});
    if(source.fd < 0 || !source.size)
        throw std::runtime_error("Invalid decoder DMA-BUF descriptor");
    VkDrmFormatModifierPropertiesListEXT modifiers{};
    modifiers.sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT;
    VkFormatProperties2 properties{};
    properties.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
    properties.pNext = &modifiers;
    vkGetPhysicalDeviceFormatProperties2(device_->physical(), format, &properties);
    std::vector<VkDrmFormatModifierPropertiesEXT> entries(modifiers.drmFormatModifierCount);
    modifiers.pDrmFormatModifierProperties = entries.data();
    vkGetPhysicalDeviceFormatProperties2(device_->physical(), format, &properties);
    auto found = std::find_if(entries.begin(), entries.end(), [&](const auto& value) { return value.drmFormatModifier == source.modifier && value.drmFormatModifierPlaneCount == 2; });
    if(found == entries.end())
        throw std::runtime_error("GPU lacks decoder DRM modifier or two-plane layout");
    VkPhysicalDeviceImageDrmFormatModifierInfoEXT modifier{};
    modifier.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT;
    modifier.drmFormatModifier = source.modifier;
    VkPhysicalDeviceExternalImageFormatInfo external{};
    external.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO;
    external.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
    external.pNext = &modifier;
    VkPhysicalDeviceImageFormatInfo2 info{};
    info.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
    info.pNext = &external;
    info.format = format;
    info.type = VK_IMAGE_TYPE_2D;
    info.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
    info.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    VkSamplerYcbcrConversionImageFormatProperties conversion{};
    conversion.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES;
    VkExternalImageFormatProperties importable{};
    importable.sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES;
    importable.pNext = &conversion;
    VkImageFormatProperties2 output{};
    output.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
    output.pNext = &importable;
    vulkan_check(vkGetPhysicalDeviceImageFormatProperties2(device_->physical(), &info, &output), "query decoder DMA-BUF import");
    if(!(importable.externalMemoryProperties.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT))
        throw std::runtime_error("DMA-BUF image is not importable");
    descriptor_count_ = conversion.combinedImageSamplerDescriptorCount;
    // The modifier and the source plane layouts reach vkCreateImage together,
    // built through one helper so a linear requirement is never computed for a
    // modified buffer.
    DmaBufImageCreateInfo create_info;
    build_dma_buf_image_create_info(source, format, external.handleType, create_info);
    vulkan_check(vkCreateImage(device_->handle(), &create_info.image, nullptr, &image_), "create coded DMA-BUF image");
    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(device_->handle(), image_, &requirements);
    if(dma_buf_size_action(source, requirements.size) == DmaBufSizeAction::DeriveLayout) {
        // Only computed layouts may fall back to the same modifier with Vulkan-derived
        // planes. V4L2's negotiated layout is authoritative: dropping its offsets and
        // pitches could bind successfully while sampling a different, corrupt picture.
        std::fprintf(stderr, "Decoder DMA-BUF: explicit layout needs %llu > buffer %llu; retrying without an explicit layout\n", static_cast<unsigned long long>(requirements.size),
                     static_cast<unsigned long long>(source.size));
        vkDestroyImage(device_->handle(), image_, nullptr);
        image_ = VK_NULL_HANDLE;
        VkImageDrmFormatModifierListCreateInfoEXT list{};
        list.sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT;
        list.drmFormatModifierCount = 1;
        list.pDrmFormatModifiers = &source.modifier;
        VkExternalMemoryImageCreateInfo plain{};
        plain.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
        plain.handleTypes = external.handleType;
        plain.pNext = &list;
        create_info.image.pNext = &plain;
        vulkan_check(vkCreateImage(device_->handle(), &create_info.image, nullptr, &image_), "create coded DMA-BUF image without an explicit layout");
        requirements = {};
        vkGetImageMemoryRequirements(device_->handle(), image_, &requirements);
    }
    if(dma_buf_size_action(source, requirements.size, true) == DmaBufSizeAction::Reject) {
        // A negotiated layout cannot be relaxed; a computed layout gets only one retry.
        // Either way, never allocate or bind less memory than Vulkan requires.
        if(source.source_layout)
            std::fprintf(stderr, "Decoder DMA-BUF: preserving source-reported plane layout; no derived-layout retry\n");
        std::fprintf(stderr, "Decoder DMA-BUF: requirement %llu > buffer %llu (extent %dx%d, fourcc %#x, modifier %#llx, plane0 pitch %lu offset %lu, plane1 pitch %lu offset %lu)\n",
                     static_cast<unsigned long long>(requirements.size), static_cast<unsigned long long>(source.size), source.width, source.height, static_cast<unsigned>(source.format),
                     static_cast<unsigned long long>(source.modifier), static_cast<unsigned long>(source.pitches[0]), static_cast<unsigned long>(source.offsets[0]),
                     static_cast<unsigned long>(source.pitches[1]), static_cast<unsigned long>(source.offsets[1]));
        std::fprintf(stderr,
                     "Decoder: this file cannot go through the VPU on this driver at this size. "
                     "Play it with LUMAFRAMEVR_DECODER=software, which skips the VPU for the run.\n");
        throw std::runtime_error("Decoder allocation smaller than Vulkan image requirements");
    }
    auto get_fd = reinterpret_cast<PFN_vkGetMemoryFdPropertiesKHR>(vkGetDeviceProcAddr(device_->handle(), "vkGetMemoryFdPropertiesKHR"));
    if(!get_fd)
        throw std::runtime_error("Vulkan external-memory-fd API is unavailable");
    int fd = fcntl(source.fd, F_DUPFD_CLOEXEC, 0);
    if(fd < 0)
        throw std::runtime_error("Cannot duplicate decoder DMA-BUF fd");
    try {
        VkMemoryFdPropertiesKHR fd_properties{};
        fd_properties.sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR;
        vulkan_check(get_fd(device_->handle(), external.handleType, fd, &fd_properties), "query DMA-BUF fd memory types");
        VkMemoryDedicatedAllocateInfo dedicated{};
        dedicated.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
        dedicated.image = image_;
        VkImportMemoryFdInfoKHR import{};
        import.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR;
        import.handleType = external.handleType;
        import.fd = fd;
        import.pNext = &dedicated;
        VkMemoryAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate.pNext = &import;
        allocate.allocationSize = source.size;
        allocate.memoryTypeIndex = device_->memory_type(requirements.memoryTypeBits & fd_properties.memoryTypeBits, 0);
        vulkan_check(vkAllocateMemory(device_->handle(), &allocate, nullptr, &memory_), "import decoder DMA-BUF memory");
        fd = -1;
        vulkan_check(vkBindImageMemory(device_->handle(), image_, memory_, 0), "bind decoder DMA-BUF memory");
    } catch(...) {
        if(fd >= 0)
            close(fd);
        throw;
    }
    create_conversion(format, found->drmFormatModifierTilingFeatures);
    create_sampling(format);
    VulkanSubmission transition(device_);
    record_foreign_transition(transition.command(), foreign_image_transition(image_, device_->queue_family(), true));
    transition.submit();
    transition.wait();
    foreign_owned_ = true;
}
}  // namespace lumaframevr

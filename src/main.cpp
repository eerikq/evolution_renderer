// fundemental (core)
#include "shader_variables.h"
#include "util/image.h"
#include "util/log.h"
#include "util/model.h"
// application
#include "platform/window.h"
// graphics
#include "renderer/command.h"
#include "renderer/depth.h"
#include "renderer/descriptor.h"
#include "renderer/device.h"
#include "renderer/draw.h"
#include "renderer/pipeline.h"
#include "renderer/swapchain.h"
#include "renderer/sync.h"
#include "renderer/uniform.h"
#include "renderer/vulkan_allocator.h"
#include "renderer/vulkan_instance.h"
// graphics (resources)
#include "renderer/resources/buffer.h"
#include "renderer/resources/sampler.h"
#include "renderer/resources/texture.h"
// contexts (engine)
#include "engine_config.h"
#include "engine_state.h"
// contexts (vulkan)
#include "buffer_context.h"
#include "device_context.h"
#include "rendering_context.h"
#include "vulkan_context.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

EngineConfig engine_config;
EngineState engine_state;

BufferContext buffer_context;
DeviceContext device_context;
RenderingContext rendering_context;
VulkanContext vulkan_context;

ShaderVariables shader_variables {};
SDL_Window* sdl_window;

void init_imgui() {
    ImGui::CreateContext();
    ImGui_ImplSDL3_InitForVulkan(sdl_window);

    VkDescriptorPoolSize pool_sizes[] {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}
    };

    VkDescriptorPoolCreateInfo pool_info {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 1000,
        .poolSizeCount = (uint32_t)std::size(pool_sizes),
        .pPoolSizes = pool_sizes,
    };

    VkDescriptorPool imgui_pool;
    vkCreateDescriptorPool(device_context.logical_device, &pool_info, nullptr, &imgui_pool);

    ImGui_ImplVulkan_InitInfo init_info {
        .Instance = vulkan_context.instance,
        .PhysicalDevice = device_context.physical_device,
        .Device = device_context.logical_device,
        .Queue = device_context.graphics_queue,
        .DescriptorPool = imgui_pool,
        .MinImageCount = 3,
        .ImageCount = 3,
        .PipelineInfoMain =
            {.MSAASamples = VK_SAMPLE_COUNT_1_BIT,
             .PipelineRenderingCreateInfo =
                 {
                     .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                     .colorAttachmentCount = 1,
                     .pColorAttachmentFormats = &rendering_context.swapchain_surface_format.format,
                 }},
        .UseDynamicRendering = true
    };

    ImGui_ImplVulkan_Init(&init_info);
}

void init_vulkan() {
    // instance
    volkInitialize(); // add a check here later to ensure VK_SUCCESS is returned
    vulkanCreateInstance(&vulkan_context.instance);
    volkLoadInstance(vulkan_context.instance);
    vulkanCreateDebugMessenger(vulkan_context.instance, &vulkan_context.debug_messenger);

    // window
    window::CreateSurface(&sdl_window, vulkan_context.instance, &rendering_context.surface);

    // device
    device::PickPhysical(vulkan_context.instance, &device_context.physical_device);
    device::CreateLogical(device_context.physical_device, rendering_context.surface, &device_context.queue_index, &device_context.logical_device);
    volkLoadDevice(device_context.logical_device);

    device::GetLogicalQueue(device_context.logical_device, device_context.queue_index, &device_context.graphics_queue);
    device::GetPhysicalProperties(device_context.physical_device, &device_context.device_properties);
    device::GetPhysicalFeatures(device_context.physical_device, &device_context.device_features);

    // allocator
    vulkanAllocatorCreate(vulkan_context.instance, device_context.physical_device, device_context.logical_device, &vulkan_context.allocator);

    // swapchain
    swapchainCreate(
        &sdl_window, device_context.physical_device, device_context.logical_device, rendering_context.surface, &rendering_context.swapchain,
        &rendering_context.swapchain_extent, &rendering_context.swapchain_surface_format
    );

    uint32_t swapchain_images_count = 0;
    swapchainGetImages(device_context.logical_device, rendering_context.swapchain, nullptr, &swapchain_images_count);
    rendering_context.swapchain_images.resize(swapchain_images_count);
    swapchainGetImages(device_context.logical_device, rendering_context.swapchain, rendering_context.swapchain_images.data(), &swapchain_images_count);

    rendering_context.swapchain_image_views.resize(swapchain_images_count);
    swapchainCreateImageViews(
        device_context.logical_device, rendering_context.swapchain_surface_format.format, rendering_context.swapchain_images.data(),
        rendering_context.swapchain_images.size(), rendering_context.swapchain_image_views.data()
    );

    // depth
    VkFormat depth_format = depth::FindDepthFormat(device_context.physical_device);
    texture::Create(
        device_context.logical_device, depth_format, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, rendering_context.swapchain_extent.width,
        rendering_context.swapchain_extent.height, vulkan_context.allocator, 0, &buffer_context.depth
    );
    texture::CreateView(device_context.logical_device, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT, &buffer_context.depth);

    // pipeline
    descriptor::CreateSetLayout(device_context.logical_device, &vulkan_context.descriptor_layout);

    pipeline::CreateGraphics(
        device_context.logical_device, rendering_context.swapchain_surface_format, depth_format, vulkan_context.descriptor_layout,
        &vulkan_context.graphics_pipeline, &vulkan_context.pipeline_layout
    );

    // command
    vulkan_context.command_buffers.resize(swapchain_images_count);
    commandCreatePool(device_context.logical_device, device_context.queue_index, &vulkan_context.command_pool);
    commandAllocateBuffers(device_context.logical_device, vulkan_context.command_pool, vulkan_context.command_buffers.data());

    // model
    model::Load("assets/viking_room.obj", &buffer_context.vertices, &buffer_context.indices);

    // vertex buffer
    buffer::Create(
        sizeof(buffer_context.vertices[0]) * buffer_context.vertices.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 0,
        vulkan_context.allocator, &buffer_context.vertex_buffer
    );
    buffer::Write(
        vulkan_context.allocator, device_context.graphics_queue, vulkan_context.command_pool, device_context.logical_device,
        sizeof(buffer_context.vertices[0]) * buffer_context.vertices.size(), buffer_context.vertices.data(), &buffer_context.vertex_buffer
    );

    // index buffer
    buffer::Create(
        sizeof(buffer_context.indices[0]) * buffer_context.indices.size(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 0,
        vulkan_context.allocator, &buffer_context.index_buffer
    );
    buffer::Write(
        vulkan_context.allocator, device_context.graphics_queue, vulkan_context.command_pool, device_context.logical_device,
        sizeof(buffer_context.indices[0]) * buffer_context.indices.size(), buffer_context.indices.data(), &buffer_context.index_buffer
    );

    // image
    if (!image::LoadFromFile("assets/viking_room.png", &buffer_context.image)) return;

    // texture
    texture::CreateFromImage(device_context.logical_device, vulkan_context.allocator, 0, &buffer_context.image, &buffer_context.texture);
    texture::LoadImage(
        device_context.logical_device, device_context.graphics_queue, vulkan_context.command_pool, vulkan_context.allocator, &buffer_context.image,
        &buffer_context.texture
    );
    texture::CreateView(device_context.logical_device, VkFormat::VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, &buffer_context.texture);

    // sampler
    sampler::Create(device_context.logical_device, &device_context.device_properties, &buffer_context.sampler);

    // uniform buffer
    buffer_context.uniform_buffers.resize(swapchain_images_count);
    buffer_context.uniform_buffers_mapped.resize(swapchain_images_count);
    uniform::CreateBuffers(vulkan_context.allocator, buffer_context.uniform_buffers.data(), buffer_context.uniform_buffers_mapped.data());

    vulkan_context.descriptor_sets.resize(EngineConfig::max_frames_in_flight);
    descriptor::CreatePool(device_context.logical_device, &vulkan_context.descriptor_pool);
    descriptor::CreateSets(
        device_context.logical_device, vulkan_context.descriptor_layout, vulkan_context.descriptor_pool, vulkan_context.descriptor_sets.data()
    );
    descriptor::ConfigureSets(
        device_context.logical_device, buffer_context.uniform_buffers.data(), buffer_context.texture.view, buffer_context.sampler,
        vulkan_context.descriptor_sets.data()
    );

    // sync
    syncCreateObjects(
        device_context.logical_device, &rendering_context.in_flight_fences, &rendering_context.render_finished_semaphores,
        &rendering_context.present_complete_semaphores
    );

    engine_state.is_running = true;
}

void main_loop() {
    while (engine_state.is_running) {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EventType::SDL_EVENT_QUIT) {
                engine_state.is_running = false;
            } else if (event.type == SDL_EventType::SDL_EVENT_WINDOW_RESIZED) {
                engine_state.framebuffer_resized = true;
            }

            ImGui_ImplSDL3_ProcessEvent(&event);
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        ImGui::SliderFloat3("Sun Direction", shader_variables.sun_direction, -180.0f, 180.0f);
        ImGui::SliderFloat3("Sun Color", shader_variables.sun_color, 0.0f, 1.0f);
        ImGui::SliderFloat("Sun Intensity", &shader_variables.sun_intensity, 0.0f, 64.0f);

        ImGui::Render();

        drawFrame(
            &engine_state.frame_index, device_context.logical_device, rendering_context.in_flight_fences.data(),
            static_cast<uint32_t>(rendering_context.in_flight_fences.size()), rendering_context.present_complete_semaphores.data(),
            rendering_context.render_finished_semaphores.data(), rendering_context.swapchain, device_context.graphics_queue,
            vulkan_context.command_buffers.data(), rendering_context.swapchain_images.data(), rendering_context.swapchain_image_views.data(),
            &buffer_context.depth.data, &buffer_context.depth.view, vulkan_context.graphics_pipeline, vulkan_context.pipeline_layout,
            vulkan_context.descriptor_sets.data(), rendering_context.swapchain_extent, &buffer_context.vertex_buffer.data, &buffer_context.index_buffer.data,
            buffer_context.uniform_buffers_mapped.data(), buffer_context.indices.size(), shader_variables
        );

        // evoLog(PrintSeverity::Debug, "drew frame");
    }
}

void cleanup() {
    window::DestroySurface(vulkan_context.instance, &rendering_context.surface);
    vulkanDestroyDebugMessenger(vulkan_context.instance, &vulkan_context.debug_messenger);
    vulkanDestroyInstance(&vulkan_context.instance);

    window::Destroy(&sdl_window);
}

int main() {
    try {
        window::Create(&sdl_window);
        init_vulkan();
        init_imgui();
        main_loop();
        cleanup();
    } catch (const std::exception& e) {
        evoLog(PrintSeverity::Fatal, "{}", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

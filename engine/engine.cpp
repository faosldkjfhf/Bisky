#include "engine.h"

#include "core/compute_pipeline.h"
#include "core/mesh_loader.h"
#include "core/pipeline_builder.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "rendering/renderer.h"
#include "utils/init.h"
#include "utils/utils.h"
#include <slang-com-ptr.h>
#include <vulkan/vulkan_core.h>

namespace bisky {

Engine::Engine() { initialize(); }

Engine::~Engine() { cleanup(); }

void Engine::initialize() {
  _window = std::make_shared<core::Window>(800, 800, "Bisky Engine", this);
  _device = std::make_shared<core::Device>(_window);
  _renderer = std::make_shared<rendering::Renderer>(_window, _device);
  // _computePipeline = std::make_shared<core::ComputePipeline>(_window, _device, _renderer);

  ComputeEffect gradient = {};
  ComputeEffect sky = {};

  VkPushConstantRange pushConstant = {};
  pushConstant.offset = 0;
  pushConstant.size = sizeof(ComputePushConstants);
  pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  VkPipelineLayoutCreateInfo computeLayout = {};
  computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  computeLayout.setLayoutCount = 1;
  computeLayout.pSetLayouts = &_renderer->drawImageLayout();
  computeLayout.pushConstantRangeCount = 1;
  computeLayout.pPushConstantRanges = &pushConstant;

  VK_CHECK(vkCreatePipelineLayout(_device->device(), &computeLayout, nullptr, &gradient.layout));
  VK_CHECK(vkCreatePipelineLayout(_device->device(), &computeLayout, nullptr, &sky.layout));

  initializeSlang();
  slang::IModule *gradientModule = utils::createSlangModule(_session, "../resources/shaders/compute/shader.slang");

  // initialize the compute effects
  VkShaderModule gradientShader;
  if (!utils::loadShaderModule(_session, gradientModule, _device->device(), "computeMain", &gradientShader)) {
    fmt::println("failed to create shader module");
  }

  VkShaderModule skyShader;
  if (!utils::loadShaderModule(_session, gradientModule, _device->device(), "computeMain", &skyShader)) {
    fmt::println("failed to create shader module");
  }

  VkPipelineShaderStageCreateInfo stageInfo = {};
  stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stageInfo.module = gradientShader;
  stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  stageInfo.pName = "main";

  VkComputePipelineCreateInfo computePipelineCreateInfo = {};
  computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  computePipelineCreateInfo.layout = gradient.layout;
  computePipelineCreateInfo.stage = stageInfo;

  gradient.name = "gradient";
  gradient.data = {};
  gradient.data.data1 = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
  gradient.data.data2 = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
  VK_CHECK(vkCreateComputePipelines(_device->device(), VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr,
                                    &gradient.pipeline));

  computePipelineCreateInfo.stage.module = skyShader;
  sky.name = "sky";
  sky.data = {};
  sky.data.data1 = glm::vec4(0.1f, 0.2f, 0.4f, 0.97f);
  sky.data.data2 = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
  VK_CHECK(vkCreateComputePipelines(_device->device(), VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr,
                                    &sky.pipeline));

  _backgroundEffects.push_back(gradient);
  _backgroundEffects.push_back(sky);

  vkDestroyShaderModule(_device->device(), skyShader, nullptr);
  vkDestroyShaderModule(_device->device(), gradientShader, nullptr);

  Slang::ComPtr<slang::ISession> newSession = init::createSession(_globalSession);
  slang::IModule *triangleModule =
      utils::createSlangModule(newSession, "../resources/shaders/render/colored_triangle_mesh.slang");

  VkShaderModule triangleVertShader;
  VkShaderModule triangleFragShader;
  utils::loadShaderModule(newSession, triangleModule, _device->device(), "vertMain", &triangleVertShader);
  utils::loadShaderModule(newSession, triangleModule, _device->device(), "fragMain", &triangleFragShader);

  VkPushConstantRange bufferRange = {};
  bufferRange.offset = 0;
  bufferRange.size = sizeof(GPUPushConstants);
  bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  VkPipelineLayoutCreateInfo pipelineLayoutInfo = init::pipelineLayoutCreateInfo();
  pipelineLayoutInfo.pushConstantRangeCount = 1;
  pipelineLayoutInfo.pPushConstantRanges = &bufferRange;
  pipelineLayoutInfo.setLayoutCount = 1;
  pipelineLayoutInfo.pSetLayouts = &_renderer->singleImageLayout();

  VK_CHECK(vkCreatePipelineLayout(_device->device(), &pipelineLayoutInfo, nullptr, &_meshPipelineLayout));

  // build our graphics pipeline
  core::PipelineBuilder builder;
  builder.layout = _meshPipelineLayout;
  _meshPipeline = builder.setShaders(triangleVertShader, triangleFragShader)
                      .setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                      .setPolygonMode(VK_POLYGON_MODE_FILL)
                      .setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE)
                      .setMultisamplingNone()
                      .disableBlending()
                      // .enableBlendingAdditive()
                      .enableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL)
                      .setColorAttachmentFormat(_renderer->drawImage().format)
                      .setDepthFormat(_renderer->depthImage().format)
                      .build(_device->device());

  // load our meshes
  _testMeshes =
      core::MeshLoader::loadGltfMeshes(_device, _renderer->immediateSubmit(), "../resources/models/basicmesh.glb")
          .value();

  // destroy the shader modules
  vkDestroyShaderModule(_device->device(), triangleVertShader, nullptr);
  vkDestroyShaderModule(_device->device(), triangleFragShader, nullptr);
}

void Engine::initializeSlang() {
  slang::createGlobalSession(_globalSession.writeRef());

  slang::SessionDesc sessionDesc = {};
  slang::TargetDesc targetDesc = {};
  targetDesc.format = SLANG_SPIRV;
  targetDesc.profile = _globalSession->findProfile("spirv_1_5");
  targetDesc.flags = 0;

  sessionDesc.targets = &targetDesc;
  sessionDesc.targetCount = 1;

  std::vector<slang::CompilerOptionEntry> options;
  options.push_back(
      {slang::CompilerOptionName::EmitSpirvDirectly, {slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr}});
  sessionDesc.compilerOptionEntries = options.data();
  sessionDesc.compilerOptionEntryCount = options.size();

  if (!SLANG_SUCCEEDED(_globalSession->createSession(sessionDesc, _session.writeRef()))) {
    throw std::runtime_error("failed to create slang session");
  }
}

void Engine::cleanup() {
  for (auto &asset : _testMeshes) {
    asset->meshBuffers.cleanup(_device->allocator());
  }

  vkDestroyPipelineLayout(_device->device(), _meshPipelineLayout, nullptr);
  vkDestroyPipeline(_device->device(), _meshPipeline, nullptr);

  for (auto &effect : _backgroundEffects) {
    vkDestroyPipelineLayout(_device->device(), effect.layout, nullptr);
    vkDestroyPipeline(_device->device(), effect.pipeline, nullptr);
  }

  _renderer->cleanup();
  _device->cleanup();
  _window->cleanup();
}

void Engine::run() {
  while (!_window->shouldClose()) {
    input();
    update();
    render();
  }

  vkDeviceWaitIdle(_device->device());
}

void Engine::input() { glfwPollEvents(); }

void Engine::update() {}

void Engine::render() {
  // imgui new frame
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();

  ImGui::NewFrame();

  ImGui::Begin("Debug");

  ComputeEffect &selected = _backgroundEffects[_currentBackgroundEffect];

  ImGui::SliderFloat("Render Scale", &_renderer->renderScale(), 0.3f, 1.0f);
  ImGui::Text("Selected Effect: %s", selected.name);
  ImGui::SliderInt("Effect Index", &_currentBackgroundEffect, 0, 1);
  ImGui::InputFloat4("data1", (float *)&selected.data.data1);
  ImGui::InputFloat4("data2", (float *)&selected.data.data2);
  ImGui::InputFloat4("data3", (float *)&selected.data.data3);
  ImGui::InputFloat4("data4", (float *)&selected.data.data4);

  ImGui::End();

  ImGui::Render();

  // reset fences and wait for next fence
  _renderer->waitForFence();

  // reset our descriptor allocators
  _renderer->getCurrentFrame().deletionQueue.flush();
  _renderer->getCurrentFrame().frameDescriptors.clearPools(_device->device());

  // try to acquire the next image
  uint32_t imageIndex;
  if (_renderer->acquireNextImage(&imageIndex)) {
    _renderer->resetFence();
  } else {
    return;
  }

  // begin the render pass
  VkCommandBuffer commandBuffer = _renderer->beginRenderPass();

  // draw the image to the swapchain
  _renderer->draw(commandBuffer, _backgroundEffects[_currentBackgroundEffect], _meshPipelineLayout, _meshPipeline,
                  _testMeshes, imageIndex);

  // end command buffer and render pass
  _renderer->endRenderPass(commandBuffer);

  // submit to present queue
  _renderer->present(imageIndex);
}

void Engine::onKey(int key, int scancode, int action, int mods) {
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
    _window->setShouldClose();
  }
}

void Engine::onResize(int width, int height) { _renderer->setFramebufferResized(true); }

void Engine::onClick(int button, int action, int mods) {}

void Engine::onMouseMove(double xpos, double ypos) {}

} // namespace bisky

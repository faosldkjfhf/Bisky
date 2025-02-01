#pragma once

#include "pch.h"

#include <slang-com-ptr.h>
#include <slang.h>

namespace bisky {

namespace rendering {
class Renderer;
}

namespace core {

const std::vector<Vertex> vertices = {{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
                                      {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
                                      {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
                                      {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},

                                      {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
                                      {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
                                      {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
                                      {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}};

const std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4};

class Window;
class Device;

struct PipelineInfo {
  std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
  VkPipelineDynamicStateCreateInfo dynamicState;
  VkPipelineVertexInputStateCreateInfo vertexInputInfo;
  VkPipelineInputAssemblyStateCreateInfo inputAssembly;
  VkPipelineViewportStateCreateInfo viewportState;
  VkPipelineViewportStateCreateInfo rasterizer;
  VkPipelineMultisampleStateCreateInfo multisampling;
  VkPipelineColorBlendAttachmentState colorBlendAttachment;
  VkPipelineColorBlendStateCreateInfo colorBlending;
  VkPipelineLayoutCreateInfo pipelineLayoutInfo;
};

class Pipeline {
public:
  Pipeline(Window &window, Device &device, rendering::Renderer &renderer);
  ~Pipeline();

  VkPipelineLayout layout() { return _pipelineLayout; }

  void cleanup();
  void bind(VkCommandBuffer commandBuffer, uint32_t imageIndex);

  void setDynamicState(std::vector<VkDynamicState> dynamicStates);
  void setVertexInputState(std::vector<VkVertexInputBindingDescription> bindings,
                           std::vector<VkVertexInputAttributeDescription> attributes);
  void setInputAssemblyState(VkPrimitiveTopology primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                             VkBool32 primitiveRestartEnable = VK_FALSE);
  void setViewportState(uint32_t viewportCount = 1, uint32_t scissorCount = 1);
  void setRasterizer(VkPolygonMode polygonMode, VkCullModeFlagBits cullMode, VkFrontFace frontFace,
                     float lineWidth = 1.0f, VkBool32 depthBiasEnable = VK_FALSE);
  void setPipelineLayout(std::vector<VkDescriptorSetLayout> setLayouts,
                         std::vector<VkPushConstantRange> pushConstantRanges);

  void updateUniformBuffer(uint32_t imageIndex, void *data, size_t size);

private:
  void initialize();
  void createDescriptorSetLayout();
  void createPipelineLayout();
  void createGraphicsPipeline(const char *file, const char *vertEntry, const char *fragEntry);

  // TODO: move to model?
  void createTextureImage();
  void createTextureImageView();
  void createTextureImageSampler();
  void generateMipmaps(VkImage image, VkFormat format, int32_t width, int32_t height, uint32_t mipLevels);

  void createUniformBuffers();
  void createDescriptorPool();
  void createDescriptorSets();

  VkShaderModule createShaderModule(Slang::ComPtr<slang::ISession> session, slang::IModule *module,
                                    const char *entryPoint);
  void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryAllocateFlags properties, VkBuffer &buffer,
                    VmaAllocation &allocation);
  void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
  void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

  Window &_window;
  Device &_device;
  rendering::Renderer &_renderer;

  VkPipelineLayout _pipelineLayout;
  VkPipeline _graphicsPipeline;
  PipelineInfo _pipelineInfo;

  VkDescriptorSetLayout _descriptorSetLayout;
  VkDescriptorPool _descriptorPool;
  std::vector<VkDescriptorSet> _descriptorSets;

  // uniform buffers
  std::vector<VkBuffer> _uniformBuffers;
  std::vector<VmaAllocation> _uniformBufferAllocations;
  std::vector<void *> _uniformBuffersMapped;

  // TODO: Should be moved to the model
  uint32_t _mipLevels;
  VkImage _textureImage;
  VmaAllocation _textureAllocation;
  VkImageView _textureImageView;
  VkSampler _textureSampler;

  // slang global session for compiling
  Slang::ComPtr<slang::IGlobalSession> _globalSession;
};

} // namespace core
} // namespace bisky

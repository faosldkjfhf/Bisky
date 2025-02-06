#pragma once

#include "core/compute_pipeline.h"
#include "core/device.h"
#include "core/window.h"
#include "gpu/gpu_mesh_buffers.h"
#include "icallbacks.h"
#include "pch.h"
#include "rendering/renderer.h"
#include <slang-com-ptr.h>

namespace bisky {

class Engine : public ICallbacks {
public:
  Engine();
  ~Engine();

  void run();

  GPUMeshBuffers uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

private:
  void input();
  void update();
  void render();

  void initialize();
  void initializeSlang();
  void cleanup();

  virtual void onKey(int key, int scancode, int action, int mods) override;
  virtual void onResize(int width, int height) override;
  virtual void onClick(int button, int action, int mods) override;
  virtual void onMouseMove(double xpos, double ypos) override;

  Pointer<core::Window> _window;
  Pointer<core::Device> _device;
  Pointer<rendering::Renderer> _renderer;

  Vector<ComputeEffect> _backgroundEffects;
  int _currentBackgroundEffect = 0;

  VkPipelineLayout _trianglePipelineLayout;
  VkPipeline _trianglePipeline;

  VkPipelineLayout _meshPipelineLayout;
  VkPipeline _meshPipeline;

  GPUMeshBuffers _meshBuffers;
  // Vector<Pointer<MeshAsset>> _testMeshes;

  Slang::ComPtr<slang::IGlobalSession> _globalSession;
  Slang::ComPtr<slang::ISession> _session;
};

} // namespace bisky

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/layer_tree_pixel_resource_test.h"

#include "cc/layers/layer.h"
#include "cc/raster/bitmap_raster_buffer_provider.h"
#include "cc/raster/gpu_raster_buffer_provider.h"
#include "cc/raster/one_copy_raster_buffer_provider.h"
#include "cc/raster/raster_buffer_provider.h"
#include "cc/raster/zero_copy_raster_buffer_provider.h"
#include "cc/resources/resource_pool.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "gpu/GLES2/gl2extchromium.h"

namespace cc {

LayerTreeHostPixelResourceTest::LayerTreeHostPixelResourceTest(
    PixelResourceTestCase test_case,
    Layer::LayerMaskType mask_type)
    : mask_type_(mask_type) {
  InitializeFromTestCase(test_case);
}

LayerTreeHostPixelResourceTest::LayerTreeHostPixelResourceTest() = default;

void LayerTreeHostPixelResourceTest::InitializeFromTestCase(
    PixelResourceTestCase test_case) {
  DCHECK(!initialized_);
  test_case_ = test_case;
  test_type_ = (test_case == SOFTWARE) ? PIXEL_TEST_SOFTWARE : PIXEL_TEST_GL;
  initialized_ = true;
}

std::unique_ptr<RasterBufferProvider>
LayerTreeHostPixelResourceTest::CreateRasterBufferProvider(
    LayerTreeHostImpl* host_impl) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      task_runner_provider()->HasImplThread()
          ? task_runner_provider()->ImplThreadTaskRunner()
          : task_runner_provider()->MainThreadTaskRunner();
  DCHECK(task_runner);
  DCHECK(initialized_);

  LayerTreeFrameSink* layer_tree_frame_sink =
      host_impl->layer_tree_frame_sink();
  viz::ContextProvider* compositor_context_provider =
      layer_tree_frame_sink->context_provider();
  viz::RasterContextProvider* worker_context_provider =
      layer_tree_frame_sink->worker_context_provider();
  LayerTreeResourceProvider* resource_provider = host_impl->resource_provider();
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager =
      layer_tree_frame_sink->gpu_memory_buffer_manager();
  int max_bytes_per_copy_operation = 1024 * 1024;
  int max_staging_buffer_usage_in_bytes = 32 * 1024 * 1024;

  switch (test_case_) {
    case SOFTWARE:
      EXPECT_FALSE(compositor_context_provider);
      EXPECT_EQ(PIXEL_TEST_SOFTWARE, test_type_);

      return std::make_unique<BitmapRasterBufferProvider>(
          host_impl->layer_tree_frame_sink());
    case GPU:
      EXPECT_TRUE(compositor_context_provider);
      EXPECT_TRUE(worker_context_provider);
      EXPECT_EQ(PIXEL_TEST_GL, test_type_);

      return std::make_unique<GpuRasterBufferProvider>(
          compositor_context_provider, worker_context_provider,
          resource_provider, false, 0, viz::PlatformColor::BestTextureFormat(),
          gfx::Size(), false);
    case ZERO_COPY:
      EXPECT_TRUE(compositor_context_provider);
      EXPECT_TRUE(gpu_memory_buffer_manager);
      EXPECT_EQ(PIXEL_TEST_GL, test_type_);

      return std::make_unique<ZeroCopyRasterBufferProvider>(
          resource_provider, gpu_memory_buffer_manager,
          compositor_context_provider, viz::PlatformColor::BestTextureFormat());
    case ONE_COPY:
      EXPECT_TRUE(compositor_context_provider);
      EXPECT_TRUE(worker_context_provider);
      EXPECT_EQ(PIXEL_TEST_GL, test_type_);

      return std::make_unique<OneCopyRasterBufferProvider>(
          task_runner, compositor_context_provider, worker_context_provider,
          resource_provider, max_bytes_per_copy_operation, false, false,
          max_staging_buffer_usage_in_bytes,
          viz::PlatformColor::BestTextureFormat());
  }
  return {};
}

void LayerTreeHostPixelResourceTest::RunPixelResourceTest(
    scoped_refptr<Layer> content_root,
    base::FilePath file_name) {
  RunPixelTest(test_type_, content_root, file_name);
}

ParameterizedPixelResourceTest::ParameterizedPixelResourceTest()
    : LayerTreeHostPixelResourceTest(::testing::get<0>(GetParam()),
                                     ::testing::get<1>(GetParam())) {}

}  // namespace cc

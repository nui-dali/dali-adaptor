#ifndef DALI_GRAPHICS_VULKAN_API_PIPELINE_CACHE_H
#define DALI_GRAPHICS_VULKAN_API_PIPELINE_CACHE_H

/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <dali/graphics-api/graphics-api-pipeline.h>
#include <dali/graphics/vulkan/api/vulkan-api-pipeline-factory.h>
#include <dali/devel-api/threading/thread-pool.h>

#include <memory>
#include <map>

namespace Dali
{
namespace Graphics
{
namespace Vulkan
{
class Graphics;
} // Vulkan

namespace VulkanAPI
{
class Controller;
namespace Internal
{
class Pipeline;
} // Internal

class Controller;
class PipelineFactory;
class Pipeline;

class PipelineCache
{
public:

  PipelineCache();

  ~PipelineCache();

  /**
   * Finds a pipeline based on the factory setup
   * @return pipeline implementation or nullptr if pipeline isn't part of cache
   */
  Internal::Pipeline* GetPipeline( const VulkanAPI::PipelineFactory& factory ) const;

  /**
   * Saves pipeline in the cache
   * @param pipeline
   * @return
   */
  bool SavePipeline( const VulkanAPI::PipelineFactory& factory, std::unique_ptr< Internal::Pipeline > pipeline );

  /**
   * Removes unused pipeline
   */
  bool RemovePipeline( Internal::Pipeline* pipeline );

  /**
   * Compiles all pending pipelines
   */
  Dali::UniqueFutureGroup Compile( bool parallel = true );

private:
  /**
   * Computes pipeline cache size
   * @return
   */
  uint32_t GetCacheSize() const;

public:

  friend class PipelineCacheDebug;

  struct CacheEntry
  {
    std::unique_ptr< Internal::Pipeline > pipelineImpl;
    std::unique_ptr< PipelineFactory::Info > info{}; // to compare if hash collision occurs
    int32_t age{0}; // To increment if only 1 ref left
  };

  std::map< uint32_t, std::vector< CacheEntry>> mCacheMap;
  std::unique_ptr<Dali::ThreadPool> mThreadPool;
};
} // VulkanAPI
} // Graphics
} // Dali

#endif // DALI_GRAPHICS_VULKAN_API_PIPELINE_CACHE_H
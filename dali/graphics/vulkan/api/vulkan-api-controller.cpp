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

#include <dali/graphics/vulkan/api/vulkan-api-controller.h>

#include <dali/graphics/vulkan/vulkan-graphics.h>
#include <dali/graphics/vulkan/internal/vulkan-buffer.h>
#include <dali/graphics/vulkan/internal/vulkan-command-buffer.h>
#include <dali/graphics/vulkan/internal/vulkan-command-pool.h>
#include <dali/graphics/vulkan/internal/vulkan-framebuffer.h>
#include <dali/graphics/vulkan/internal/vulkan-surface.h>
#include <dali/graphics/vulkan/internal/vulkan-sampler.h>
#include <dali/graphics/vulkan/internal/vulkan-image.h>
#include <dali/graphics/vulkan/internal/vulkan-image-view.h>
#include <dali/graphics/vulkan/internal/vulkan-swapchain.h>
#include <dali/graphics/vulkan/internal/vulkan-debug.h>
#include <dali/graphics/vulkan/internal/vulkan-fence.h>

// API
#include <dali/graphics/vulkan/api/vulkan-api-shader.h>
#include <dali/graphics/vulkan/api/vulkan-api-texture.h>
#include <dali/graphics/vulkan/api/vulkan-api-buffer.h>
#include <dali/graphics/vulkan/api/vulkan-api-framebuffer.h>
#include <dali/graphics/vulkan/api/vulkan-api-texture-factory.h>
#include <dali/graphics/vulkan/api/vulkan-api-shader-factory.h>
#include <dali/graphics/vulkan/api/vulkan-api-buffer-factory.h>
#include <dali/graphics/vulkan/api/vulkan-api-pipeline.h>
#include <dali/graphics/vulkan/api/vulkan-api-pipeline-factory.h>
#include <dali/graphics/vulkan/api/vulkan-api-framebuffer-factory.h>
#include <dali/graphics/vulkan/api/vulkan-api-sampler-factory.h>
#include <dali/graphics/vulkan/api/vulkan-api-render-command.h>
#include <dali/graphics/vulkan/api/internal/vulkan-pipeline-cache.h>
#include <dali/graphics/vulkan/api/internal/vulkan-api-descriptor-set-allocator.h>

#include <dali/devel-api/threading/thread-pool.h>

namespace Dali
{
namespace Graphics
{
namespace VulkanAPI
{
namespace DepthStencilFlagBits
{
static constexpr uint32_t DEPTH_BUFFER_BIT   = 1; // depth buffer enabled
static constexpr uint32_t STENCIL_BUFFER_BIT = 2; // stencil buffer enabled
}

// State of the depth-stencil buffer
using DepthStencilFlags = uint32_t;

struct Controller::Impl
{
  struct RenderPassData
  {
    // only move semantics
    RenderPassData( vk::RenderPassBeginInfo _beginInfo,
                    std::vector< vk::ClearValue >&& _clearColorValues,
                    Vulkan::RefCountedFramebuffer _framebuffer )
      : beginInfo( _beginInfo ),
        colorValues( std::move( _clearColorValues ) ),
        framebuffer( std::move( _framebuffer ) ),
        renderCommands()
    {
    }

    // no default constructor!
    RenderPassData() = delete;

    vk::RenderPassBeginInfo beginInfo{};
    std::vector< vk::ClearValue > colorValues{};
    Vulkan::RefCountedFramebuffer framebuffer{};
    std::vector< Dali::Graphics::RenderCommand* > renderCommands;
  };

  Impl( Controller& owner )
  : mGraphics(),
    mOwner( owner ),
    mDepthStencilBufferCurrentState( 0u ),
    mDepthStencilBufferRequestedState( 0u )
  {
  }

  ~Impl() = default;

  // TODO: @todo this function initialises basic buffers, shaders and pipeline
  // for the prototype ONLY
  bool Initialise()
  {
    // Create factories
    mShaderFactory = MakeUnique< VulkanAPI::ShaderFactory >( mGraphics );
    mTextureFactory = MakeUnique< VulkanAPI::TextureFactory >( mOwner );
    mBufferFactory = MakeUnique< VulkanAPI::BufferFactory >( mOwner );
    mFramebufferFactory = MakeUnique< VulkanAPI::FramebufferFactory >( mOwner );
    mPipelineFactory = MakeUnique< VulkanAPI::PipelineFactory >( mOwner );
    mSamplerFactory = MakeUnique< VulkanAPI::SamplerFactory >( mOwner );

    mDefaultPipelineCache = MakeUnique< VulkanAPI::PipelineCache >();
    mDescriptorSetAllocator = MakeUnique< VulkanAPI::Internal::DescriptorSetAllocator>( mOwner );

    return mThreadPool.Initialize();
  }

  void UpdateDepthStencilBuffer()
  {
    // if we enable depth/stencil dynamically we need to block and invalidate pipeline cache
    // enable depth-stencil
    if( mDepthStencilBufferCurrentState != mDepthStencilBufferRequestedState )
    {
      DALI_LOG_INFO( gLogFilter, Debug::Verbose, "UpdateDepthStencilBuffer(): New state: DEPTH: %d, STENCIL: %d\n", int(mDepthStencilBufferRequestedState&1), int((mDepthStencilBufferRequestedState>>1)&1) );

      // Formats
      const std::array<vk::Format, 4> DEPTH_STENCIL_FORMATS = {
        vk::Format::eUndefined,       // no depth nor stencil needed
        vk::Format::eD16Unorm,        // only depth buffer
        vk::Format::eS8Uint,          // only stencil buffer
        vk::Format::eD24UnormS8Uint   // depth and stencil buffers
      };

      mGraphics.DeviceWaitIdle();

      mGraphics.GetSwapchainForFBID(0u)->SetDepthStencil( DEPTH_STENCIL_FORMATS[mDepthStencilBufferRequestedState] );

      // make sure GPU finished any pending work
      mGraphics.DeviceWaitIdle();

      mDepthStencilBufferCurrentState = mDepthStencilBufferRequestedState;
    }
  }

  void AcquireNextFramebuffer()
  {
    // for all swapchains acquire new framebuffer
    auto surface = mGraphics.GetSurface( 0u );

    auto swapchain = mGraphics.GetSwapchainForFBID( 0u );

    if ( mGraphics.IsSurfaceResized() )
    {
      swapchain->Invalidate();
    }

    // We won't run garbage collection in case there are pending resource transfers.
    swapchain->AcquireNextFramebuffer( !mOwner.HasPendingResourceTransfers() );

    if( !swapchain->IsValid() )
    {
      // make sure device doesn't do any work before replacing swapchain
      mGraphics.DeviceWaitIdle();

      // replace swapchain
      swapchain = mGraphics.ReplaceSwapchainForSurface( surface, std::move(swapchain) );

      // get new valid framebuffer
      swapchain->AcquireNextFramebuffer( !mOwner.HasPendingResourceTransfers() );
    }
  }

  void CompilePipelines()
  {
    mPipelineCompileFuture = mDefaultPipelineCache->Compile( true );
  }

  void BeginFrame()
  {
    // Acquire next framebuffer image
    AcquireNextFramebuffer();

    // Compile all pipelines that haven't been initialised yet
    CompilePipelines();

    mRenderPasses.clear();
    mCurrentFramebuffer.Reset();
  }

  void EndFrame()
  {
    // Update descriptor sets if there are any updates
    // swap all swapchains
    auto swapchain = mGraphics.GetSwapchainForFBID( 0u );

    if( !mRenderPasses.empty() )
    {
      // Ensure there are enough command buffers for each render pass,
      swapchain->AllocateCommandBuffers( mRenderPasses.size() );
      std::vector<Vulkan::RefCountedCommandBuffer>& renderPassBuffers = swapchain->GetCommandBuffers();
      uint32_t index = 0;
      for( auto& renderPassData : mRenderPasses )
      {
        ProcessRenderPassData( renderPassBuffers[index], renderPassData );
        ++index;
      }
    }
    else
    {
      auto primaryCommandBuffer = swapchain->GetLastCommandBuffer();
      primaryCommandBuffer->Begin( vk::CommandBufferUsageFlagBits::eOneTimeSubmit, nullptr );
      primaryCommandBuffer->BeginRenderPass( vk::RenderPassBeginInfo{}
        .setFramebuffer( swapchain->GetCurrentFramebuffer()->GetVkHandle() )
        .setRenderPass(swapchain->GetCurrentFramebuffer()->GetRenderPass() )
        .setRenderArea( { {0, 0}, { swapchain->GetCurrentFramebuffer()->GetWidth(), swapchain->GetCurrentFramebuffer()->GetHeight() } } )
        .setPClearValues( swapchain->GetCurrentFramebuffer()->GetClearValues().data() )
        .setClearValueCount( uint32_t(swapchain->GetCurrentFramebuffer()->GetClearValues().size()) ), vk::SubpassContents::eInline );
      primaryCommandBuffer->EndRenderPass();
      primaryCommandBuffer->End();
    }

    for( auto& future : mMemoryTransferFutures )
    {
      future->Wait();
      future.reset();
    }

    mMemoryTransferFutures.clear();

    swapchain->Present();
  }

  Dali::Graphics::TextureFactory& GetTextureFactory() const
  {
    return *( mTextureFactory.get() );
  }

  Dali::Graphics::ShaderFactory& GetShaderFactory() const
  {
    return *( mShaderFactory.get() );
  }

  Dali::Graphics::BufferFactory& GetBufferFactory() const
  {
    return *( mBufferFactory.get() );
  }

  Dali::Graphics::FramebufferFactory& GetFramebufferFactory() const
  {
    mFramebufferFactory->Reset();
    return *( mFramebufferFactory.get() );
  }


  std::unique_ptr< Dali::Graphics::RenderCommand > AllocateRenderCommand()
  {
    return std::make_unique< VulkanAPI::RenderCommand >( mOwner, mGraphics );
  }

  void UpdateRenderPass( const std::vector< Dali::Graphics::RenderCommand*  >& commands, uint32_t startIndex, uint32_t endIndex )
  {
    auto firstCommand = static_cast<VulkanAPI::RenderCommand*>(commands[startIndex]);
    auto renderTargetBinding = firstCommand->GetRenderTargetBinding();

    Vulkan::RefCountedFramebuffer framebuffer{ nullptr };
    if( renderTargetBinding.framebuffer )
    {
      framebuffer = static_cast<const VulkanAPI::Framebuffer&>(*renderTargetBinding.framebuffer).GetFramebufferRef();
    }
    else
    {
      // use first surface/swapchain as render target
      auto surface = mGraphics.GetSurface( 0u );
      auto swapchain = mGraphics.GetSwapchainForFBID( 0u );
      framebuffer = swapchain->GetCurrentFramebuffer();
    }

    if( framebuffer != mCurrentFramebuffer )
    {
      mCurrentFramebuffer = framebuffer;

      // @todo Only do if there is a color attachment and color clear is enabled and there is a clear color
      auto newColors = mCurrentFramebuffer->GetClearValues();
      newColors[0].color.setFloat32( { renderTargetBinding.clearColors[0].r,
                                       renderTargetBinding.clearColors[0].g,
                                       renderTargetBinding.clearColors[0].b,
                                       renderTargetBinding.clearColors[0].a
                                     } );

      mRenderPasses.emplace_back(
              vk::RenderPassBeginInfo{}
                      .setRenderPass( mCurrentFramebuffer->GetRenderPass() )
                      .setFramebuffer( mCurrentFramebuffer->GetVkHandle() )
                      .setRenderArea( vk::Rect2D( { 0, 0 }, { mCurrentFramebuffer->GetWidth(),
                                                              mCurrentFramebuffer->GetHeight() } ) )
                      .setClearValueCount( uint32_t( newColors.size() ) )
                      .setPClearValues( newColors.data() ),
              std::move( newColors ),
              framebuffer );

    }

    auto& vector = mRenderPasses.back().renderCommands;
    vector.insert( vector.end(), commands.begin()+int(startIndex), commands.begin()+int(endIndex) );
  }

  /**
   * Submits number of commands in one go ( simiar to vkCmdExecuteCommands )
   * @param commands
   */
  void SubmitCommands( std::vector< Dali::Graphics::RenderCommand* > commands )
  {
    /*
     * analyze descriptorset needs pers signature
     */
    std::vector<DescriptorSetRequirements> dsRequirements {};
    for( auto& command : commands )
    {
      auto vulkanCommand = static_cast<VulkanAPI::RenderCommand*>( command );
      vulkanCommand->UpdateDescriptorSetAllocationRequirements( dsRequirements, *mDescriptorSetAllocator.get() );
    }

    if( !mDescriptorSetsFreeList.empty() )
    {
      mDescriptorSetAllocator->FreeDescriptorSets( std::move( mDescriptorSetsFreeList ) );
      mDescriptorSetsFreeList.clear();
    }

    /*
     * Update descriptor pools based on the requirements
     */
    if( dsRequirements.size() )
    {
      mDescriptorSetAllocator->UpdateWithRequirements( dsRequirements, 0u );
    }

    /**
     * Allocate descriptor sets for all signatures that requirements forced
     * recreating pools
     */
    for( auto& command : commands )
    {
      auto vulkanCommand = static_cast<VulkanAPI::RenderCommand*>( command );
      vulkanCommand->AllocateDescriptorSets( *mDescriptorSetAllocator );
    }

    mMemoryTransferFutures.emplace_back( mThreadPool.SubmitTask(0, Task([this](uint32_t workerIndex){
      // if there are any scheduled writes
      if( !mBufferTransferRequests.empty() )
      {
        for( auto&& req : mBufferTransferRequests )
        {
          void* dst = req->dstBuffer->GetMemory()->Map();
          memcpy( dst, &*req->srcPtr, req->srcSize );
          req->dstBuffer->GetMemory()->Flush();
          req->dstBuffer->GetMemory()->Unmap();
        }
        mBufferTransferRequests.clear();
      }

      // execute all scheduled resource transfers
      ProcessResourceTransferRequests();
    })) );

    // the list of commands may be empty, but still we may have scheduled memory
    // transfers
    if( commands.empty() )
    {
      return;
    }

    // Update render pass data per framebuffer
    const Dali::Graphics::Framebuffer* currFramebuffer = nullptr;
    uint32_t previousPassBeginIndex = 0u;
    uint32_t index = 0u;
    for( auto& command : commands )
    {
      if( command->mRenderTargetBinding.framebuffer != currFramebuffer )
      {
        if( index )
        {
          UpdateRenderPass( commands, previousPassBeginIndex, index );
        }
        previousPassBeginIndex = index;
        currFramebuffer = command->mRenderTargetBinding.framebuffer;
      }
      ++index;
    }
    UpdateRenderPass( commands, previousPassBeginIndex, index );
  }

  // @todo: possibly optimise
  bool TestCopyRectIntersection( const ResourceTransferRequest* srcRequest, const ResourceTransferRequest* currentRequest )
  {
    auto srcOffset = srcRequest->bufferToImageInfo.copyInfo.imageOffset;
    auto srcExtent = srcRequest->bufferToImageInfo.copyInfo.imageExtent;

    auto curOffset = currentRequest->bufferToImageInfo.copyInfo.imageOffset;
    auto curExtent = currentRequest->bufferToImageInfo.copyInfo.imageExtent;

    auto offsetX0 = std::min( srcOffset.x, curOffset.x );
    auto offsetY0 = std::min( srcOffset.y, curOffset.y );
    auto offsetX1 = std::max( srcOffset.x+int32_t(srcExtent.width), curOffset.x+int32_t(curExtent.width) );
    auto offsetY1 = std::max( srcOffset.y+int32_t(srcExtent.height), curOffset.y+int32_t(curExtent.height) );

    return ( (offsetX1 - offsetX0) < (int32_t(srcExtent.width) + int32_t(curExtent.width)) &&
             (offsetY1 - offsetY0) < (int32_t(srcExtent.height) + int32_t(curExtent.height)) );

  }


  void ProcessResourceTransferRequests( bool immediateOnly = false )
  {
    std::lock_guard<std::recursive_mutex> lock(mResourceTransferMutex);
    if(!mResourceTransferRequests.empty())
    {
      using ResourceTransferRequestList = std::vector<const ResourceTransferRequest*>;

      /**
       * Structure associating unique images and lists of transfer requests for which
       * the key image is a destination. It contains separate lists of requests per image.
       * Each list of requests groups non-intersecting copy operations into smaller batches.
       */
      struct ResourceTransferRequestPair
      {
        ResourceTransferRequestPair( const Vulkan::RefCountedImage& key )
        : image( key ), requestList{{}}
        {
        }

        Vulkan::RefCountedImage                  image;
        std::vector<ResourceTransferRequestList> requestList;
      };

      // Map of all the requests where 'image' is a key.
      std::vector<ResourceTransferRequestPair> requestMap;

      auto highestBatchIndex = 1u;

      // Collect all unique destination images and all transfer requests associated with them
      for( const auto& req : mResourceTransferRequests )
      {
        Vulkan::RefCountedImage image { nullptr };
        if( req.requestType == TransferRequestType::BUFFER_TO_IMAGE )
        {
          image = req.bufferToImageInfo.dstImage;
        }
        else if ( req.requestType == TransferRequestType::IMAGE_TO_IMAGE )
        {
          image = req.imageToImageInfo.dstImage;
        }
        else if ( req.requestType == TransferRequestType::USE_TBM_SURFACE )
        {
          image = req.useTBMSurfaceInfo.srcImage;
        }
        else if ( req.requestType == TransferRequestType::LAYOUT_TRANSITION_ONLY )
        {
          image = req.imageLayoutTransitionInfo.image;
        }
        assert( image );

        auto predicate = [&]( auto& item )->bool {
          return image->GetVkHandle() == item.image->GetVkHandle();
        };
        auto it = std::find_if( requestMap.begin(), requestMap.end(), predicate );

        if( it == requestMap.end() )
        {
          // initialise new array
          requestMap.emplace_back( image );
          it = requestMap.end()-1;
        }

        auto& transfers = it->requestList;

        // Compare with current transfer list whether there are any intersections
        // with current image copy area. If intersection occurs, start new list
        auto& currentList = transfers.back();

        bool intersects( false );
        for( auto& item : currentList )
        {
          // if area intersects create new list
          if( (intersects = TestCopyRectIntersection( item, &req )) )
          {
            transfers.push_back({});
            highestBatchIndex = std::max( highestBatchIndex, uint32_t(transfers.size()) );
            break;
          }
        }

        // push request to the most recently created list
        transfers.back().push_back( &req );
      }

      // For all unique images prepare layout transition barriers as all of them must be
      // in eTransferDstOptimal layout
      std::vector<vk::ImageMemoryBarrier> preLayoutBarriers;
      std::vector<vk::ImageMemoryBarrier> postLayoutBarriers;
      for( auto& item : requestMap )
      {
        auto image = item.image;
        // add barrier
        preLayoutBarriers.push_back( mGraphics.CreateImageMemoryBarrier( image, image->GetImageLayout(), vk::ImageLayout::eTransferDstOptimal ) );
        postLayoutBarriers.push_back( mGraphics.CreateImageMemoryBarrier( image, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal ) );
        image->SetImageLayout( vk::ImageLayout::eShaderReadOnlyOptimal );
      }

      // Build command buffer for each image until reaching next sync point
      auto commandBuffer = mGraphics.CreateCommandBuffer( true );

      // Fence between submissions
      auto fence = mGraphics.CreateFence( {} );

      /**
       * The loop iterates through requests for each unique image. It parallelizes
       * transfers to images until end of data in the batch.
       * After submitting copy commands the loop waits for the fence to be signalled
       * and repeats recording for the next batch of transfer requests.
       */
      for( auto i = 0u; i < highestBatchIndex; ++i )
      {
        commandBuffer->Begin( vk::CommandBufferUsageFlagBits::eOneTimeSubmit, nullptr );

        // change image layouts only once
        if( i == 0 )
        {
          commandBuffer->PipelineBarrier( vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, preLayoutBarriers );
        }

        for( auto& item : requestMap )
        {
          auto& batchItem = item.requestList;
          if( batchItem.size() <= i )
          {
            continue;
          }

          auto& requestList = batchItem[i];

          // record all copy commands for this batch
          for( auto& req : requestList )
          {
            if( req->requestType == TransferRequestType::BUFFER_TO_IMAGE )
            {
              commandBuffer->CopyBufferToImage( req->bufferToImageInfo.srcBuffer,
                                                req->bufferToImageInfo.dstImage, vk::ImageLayout::eTransferDstOptimal,
                                                { req->bufferToImageInfo.copyInfo } );

            }
            else if( req->requestType == TransferRequestType::IMAGE_TO_IMAGE )
            {
              commandBuffer->CopyImage( req->imageToImageInfo.srcImage, vk::ImageLayout::eTransferSrcOptimal,
                                        req->imageToImageInfo.dstImage, vk::ImageLayout::eTransferDstOptimal,
                                        { req->imageToImageInfo.copyInfo });
            }
          }
        }

        // if this is the last batch restore original layouts
        if( i == highestBatchIndex - 1 )
        {
          commandBuffer->PipelineBarrier( vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, postLayoutBarriers );
        }
        commandBuffer->End();

        // submit to the queue
        mGraphics.Submit( mGraphics.GetTransferQueue(0u), { Vulkan::SubmissionData{ {}, {}, { commandBuffer }, {} } }, fence );
        mGraphics.WaitForFence( fence );
        mGraphics.ResetFence( fence );
      }

      // Destroy staging resources immediately
      for( auto& request : mResourceTransferRequests )
      {
        if( request.requestType == TransferRequestType::BUFFER_TO_IMAGE )
        {
          auto& buffer = request.bufferToImageInfo.srcBuffer;
          // Do not destroy
          if( buffer != mTextureStagingBuffer->GetBufferRef() )
          {
            buffer->DestroyNow();
          }
        }
        else if( request.requestType == TransferRequestType::IMAGE_TO_IMAGE )
        {
          auto& image = request.imageToImageInfo.srcImage;
          if( image->GetVkHandle() )
          {
            image->DestroyNow();
          }
        }
      }

      // Clear transfer queue
      mResourceTransferRequests.clear();
    }
  }

  void InvalidateSwapchain()
  {
    auto swapchain = mGraphics.GetSwapchainForFBID( 0u );
    swapchain->Invalidate();
  }

  void ProcessRenderPassData( Vulkan::RefCountedCommandBuffer commandBuffer, const RenderPassData& renderPassData )
  {
    if( mPipelineCompileFuture )
    {
      mPipelineCompileFuture->Wait();
      mPipelineCompileFuture.reset( nullptr );
    }

    commandBuffer->Begin( vk::CommandBufferUsageFlagBits::eOneTimeSubmit, nullptr );
    commandBuffer->BeginRenderPass( renderPassData.beginInfo, vk::SubpassContents::eInline );

    // update descriptor sets
    for( auto&& command : renderPassData.renderCommands )
    {
      auto apiCommand = static_cast<VulkanAPI::RenderCommand*>(command);
      apiCommand->PrepareResources();
    }

    if( mDescriptorWrites.size() )
    {
      mGraphics.GetDevice().updateDescriptorSets( uint32_t( mDescriptorWrites.size() ), mDescriptorWrites.data(), 0, nullptr );
      mDescriptorWrites.clear();
      mDescriptorInfoStack.clear();
    }

    for( auto&& command : renderPassData.renderCommands )
    {
#if defined(DEBUG_ENABLED)
      if( getenv( "LOG_VULKAN_API" ) )
      {
        DALI_LOG_STREAM( gVulkanFilter, Debug::General, *command );
      }
#endif

      auto apiCommand = static_cast<VulkanAPI::RenderCommand*>(command);

      // skip if there's no valid pipeline
      if( !apiCommand->GetVulkanPipeline() )
      {
        continue;
      }

      apiCommand->BindPipeline( commandBuffer );

      //@todo add assert to check the pipeline render pass nad the inherited render pass are the same

      // set dynamic state
      if( apiCommand->mDrawCommand.scissorTestEnable )
      {
        vk::Rect2D scissorRect( { apiCommand->mDrawCommand.scissor.x,
                                  apiCommand->mDrawCommand.scissor.y },
                                { apiCommand->mDrawCommand.scissor.width,
                                  apiCommand->mDrawCommand.scissor.height } );

        commandBuffer->SetScissor( 0, 1, &scissorRect );
      }

      // dynamic state: viewport
      auto vulkanApiPipeline = static_cast<const VulkanAPI::Pipeline*>(apiCommand->GetPipeline());

      auto dynamicStateMask = vulkanApiPipeline->GetDynamicStateMask();
      if( (dynamicStateMask & Dali::Graphics::PipelineDynamicStateBits::VIEWPORT_BIT) && apiCommand->mDrawCommand.viewportEnable )
      {
        auto viewportRect = apiCommand->mDrawCommand.viewport;

        vk::Viewport viewport( viewportRect.x,
                               viewportRect.y,
                               viewportRect.width,
                               viewportRect.height,
                               viewportRect.minDepth,
                               viewportRect.maxDepth );

        commandBuffer->SetViewport( 0, 1, &viewport );
      }

      // bind vertex buffers
      auto binding = 0u;
      for( auto&& vb : apiCommand->GetVertexBufferBindings() )
      {
        commandBuffer->BindVertexBuffer( binding++, static_cast<const VulkanAPI::Buffer&>( *vb ).GetBufferRef(), 0 );
      }

      // note: starting set = 0
      const auto& descriptorSets = apiCommand->GetDescriptorSets();
      commandBuffer->BindDescriptorSets( descriptorSets, vulkanApiPipeline->GetVkPipelineLayout(), 0, uint32_t( descriptorSets.size() ) );

      // draw
      const auto& drawCommand = apiCommand->GetDrawCommand();

      const auto& indexBinding = apiCommand->GetIndexBufferBinding();
      if( indexBinding.buffer )
      {
        commandBuffer->BindIndexBuffer( static_cast<const VulkanAPI::Buffer&>(*indexBinding.buffer).GetBufferRef(),
                                 0, vk::IndexType::eUint16 );
        commandBuffer->DrawIndexed( drawCommand.indicesCount,
                             drawCommand.instanceCount,
                             drawCommand.firstIndex,
                             0,
                             drawCommand.firstInstance );
      }
      else
      {
        commandBuffer->Draw( drawCommand.vertexCount,
                      drawCommand.instanceCount,
                      drawCommand.firstVertex,
                      drawCommand.firstInstance );
      }
    }

    commandBuffer->EndRenderPass();
    commandBuffer->End();
  }

  bool EnableDepthStencilBuffer( bool enableDepth, bool enableStencil )
  {
    mDepthStencilBufferRequestedState = ( enableDepth ? DepthStencilFlagBits::DEPTH_BUFFER_BIT : 0u ) |
                                        ( enableStencil ? DepthStencilFlagBits::STENCIL_BUFFER_BIT : 0u );

    auto retval = mDepthStencilBufferRequestedState != mDepthStencilBufferCurrentState;

    UpdateDepthStencilBuffer();

    return retval;
  }

  void RunGarbageCollector( size_t numberOfDiscardedRenderers )
  {
    // @todo Decide what GC's to run.
  }

  void DiscardUnusedResources()
  {
    mGraphics.GetGraphicsQueue(0).GetVkHandle().waitIdle();

    mGraphics.GetSwapchainForFBID(0)->ResetAllCommandBuffers();

    mGraphics.CollectGarbage();

    mDescriptorSetAllocator->InvalidateAllDescriptorSets();
  }

  bool IsDiscardQueueEmpty()
  {
    return mGraphics.GetDiscardQueue(0u).empty() && mGraphics.GetDiscardQueue(1u).empty();
  }

  void SwapBuffers()
  {
    mGraphics.SwapBuffers();
  }

  uint32_t GetSwapchainBufferCount()
  {
    return mGraphics.GetSwapchainForFBID(0u)->GetImageCount();
  }

  Vulkan::Graphics mGraphics;
  Controller& mOwner;

  std::unique_ptr< PipelineCache > mDefaultPipelineCache;
  std::unique_ptr< VulkanAPI::Internal::DescriptorSetAllocator > mDescriptorSetAllocator;
  std::unique_ptr< VulkanAPI::TextureFactory > mTextureFactory;
  std::unique_ptr< VulkanAPI::ShaderFactory > mShaderFactory;
  std::unique_ptr< VulkanAPI::BufferFactory > mBufferFactory;
  std::unique_ptr< VulkanAPI::PipelineFactory > mPipelineFactory;
  std::unique_ptr< VulkanAPI::FramebufferFactory > mFramebufferFactory;
  std::unique_ptr< VulkanAPI::SamplerFactory > mSamplerFactory;

  // used for UBOs
  std::vector< std::unique_ptr< VulkanAPI::BufferMemoryTransfer > > mBufferTransferRequests;

  // used for texture<->buffer<->memory transfers
  std::vector< ResourceTransferRequest > mResourceTransferRequests;

  Vulkan::RefCountedFramebuffer mCurrentFramebuffer;
  std::vector< RenderPassData > mRenderPasses;

  ThreadPool mThreadPool;

  std::vector< std::shared_ptr< Future<void> > > mMemoryTransferFutures;

  std::vector<vk::WriteDescriptorSet> mDescriptorWrites;

  struct DescriptorInfo
  {
    vk::DescriptorImageInfo imageInfo;
    vk::DescriptorBufferInfo bufferInfo;
  };

  std::deque<DescriptorInfo> mDescriptorInfoStack;

  std::mutex mDescriptorWriteMutex{};
  std::recursive_mutex mResourceTransferMutex{};

  DepthStencilFlags mDepthStencilBufferCurrentState { 0u };
  DepthStencilFlags mDepthStencilBufferRequestedState { 0u };

  std::vector<DescriptorSetList> mDescriptorSetsFreeList;

  std::unique_ptr<VulkanAPI::Buffer> mTextureStagingBuffer{};
  Dali::SharedFuture                 mTextureStagingBufferFuture{};
  void*                              mTextureStagingBufferMappedPtr{ nullptr };

  bool mDrawOnResume{ false };

  Dali::UniqueFutureGroup            mPipelineCompileFuture{};
};

// TODO: @todo temporarily ignore missing return type, will be fixed later

std::unique_ptr< Dali::Graphics::Shader > Controller::CreateShader( const Dali::Graphics::BaseFactory< Dali::Graphics::Shader >& factory )
{
  return factory.Create();
}

std::unique_ptr< Dali::Graphics::Texture > Controller::CreateTexture( const Dali::Graphics::BaseFactory< Dali::Graphics::Texture >& factory )
{
  return factory.Create();
}

std::unique_ptr< Dali::Graphics::Buffer > Controller::CreateBuffer( const Dali::Graphics::BaseFactory< Dali::Graphics::Buffer >& factory )
{
  return factory.Create();
}

std::unique_ptr< Dali::Graphics::Sampler > Controller::CreateSampler( const Dali::Graphics::BaseFactory< Dali::Graphics::Sampler >& factory )
{
  return factory.Create();
}

std::unique_ptr< Dali::Graphics::Pipeline > Controller::CreatePipeline( const Dali::Graphics::PipelineFactory& factory )
{
  auto& pipelineFactory = const_cast<PipelineFactory&>(dynamic_cast<const PipelineFactory&>( factory ));

  // if no custom cache, use default one
  if( !pipelineFactory.mPipelineCache )
  {
    pipelineFactory.mPipelineCache = mImpl->mDefaultPipelineCache.get();
  }

  return mImpl->mPipelineFactory->Create();
}

std::unique_ptr< Dali::Graphics::Framebuffer > Controller::CreateFramebuffer( const Dali::Graphics::BaseFactory< Dali::Graphics::Framebuffer >& factory )
{
  return factory.Create();
}


Controller::Controller()
: mImpl( std::make_unique< Impl >( *this ) )
{
}

void Controller::Initialise()
{
  mImpl->mGraphics.Create();

  // create device
  mImpl->mGraphics.CreateDevice();

  mImpl->Initialise();

  uint32_t stagingSize = 0;
  auto v = getenv("DALI_DEFAULT_STAGING_BUFFER_SIZE_MB");
  if( v )
  {
    stagingSize = uint32_t(atoi(v)) * 1024 * 1024;
  }
  // Initialise large staging buffer and keep the future. The future must be checked
  // before next initialisation
  if( stagingSize )
  {
    mImpl->mTextureStagingBufferFuture = InitializeTextureStagingBuffer( stagingSize, false );
  }
}

Controller::~Controller() = default;

Controller& Controller::operator=( Controller&& ) noexcept = default;

void Controller::BeginFrame()
{
  mImpl->mDrawOnResume = false;
  mStats.samplerTextureBindings = 0;
  mStats.uniformBufferBindings = 0;

  //@ todo, not multithreaded yet
  mImpl->mDescriptorWrites.clear();

  mStats.frame++;
  mImpl->BeginFrame();
}

void Controller::PushDescriptorWrite( const vk::WriteDescriptorSet& write )
{
  vk::DescriptorImageInfo* pImageInfo { nullptr };
  vk::DescriptorBufferInfo* pBufferInfo { nullptr };
  std::lock_guard<std::mutex> lock( mImpl->mDescriptorWriteMutex );
  if( write.pImageInfo )
  {
    mImpl->mDescriptorInfoStack.emplace_back();
    mImpl->mDescriptorInfoStack.back().imageInfo = *write.pImageInfo;
    pImageInfo = &mImpl->mDescriptorInfoStack.back().imageInfo;
  }
  else if( write.pBufferInfo )
  {
    mImpl->mDescriptorInfoStack.emplace_back();
    mImpl->mDescriptorInfoStack.back().bufferInfo = *write.pBufferInfo;
    pBufferInfo = &mImpl->mDescriptorInfoStack.back().bufferInfo;
  }
  mImpl->mDescriptorWrites.emplace_back( write );
  mImpl->mDescriptorWrites.back().pBufferInfo = pBufferInfo;
  mImpl->mDescriptorWrites.back().pImageInfo = pImageInfo;
}

void Controller::FreeDescriptorSets( DescriptorSetList&& descriptorSetList )
{
  if( descriptorSetList.descriptorSets.empty() )
  {
    return;
  }
  mImpl->mDescriptorSetsFreeList.emplace_back( std::move( descriptorSetList ) );
}

bool Controller::TestDescriptorSetsValid( VulkanAPI::DescriptorSetList& descriptorSetList, std::vector<bool>& results ) const
{
  return mImpl->mDescriptorSetAllocator->TestIfValid( descriptorSetList, results );
}

void Controller::EndFrame()
{
  mImpl->EndFrame();

#if(DEBUG_ENABLED)
  // print stats
  PrintStats();
#endif
}

void Controller::PrintStats()
{
  DALI_LOG_INFO( gLogFilter, Debug::Verbose, "Frame: %d\n", int(mStats.frame));
  DALI_LOG_INFO( gLogFilter, Debug::Verbose, "  UBO bindings: %d\n", int(mStats.uniformBufferBindings));
  DALI_LOG_INFO( gLogFilter, Debug::Verbose, "  Tex bindings: %d\n", int(mStats.samplerTextureBindings));
}

void Controller::Pause()
{
}

void Controller::Resume()
{
  // Ensure we re-draw at least once:
  mImpl->mDrawOnResume = true;
}

Dali::Graphics::TextureFactory& Controller::GetTextureFactory() const
{
  return mImpl->GetTextureFactory();
}

Dali::Graphics::ShaderFactory& Controller::GetShaderFactory() const
{
  return mImpl->GetShaderFactory();
}

Dali::Graphics::BufferFactory& Controller::GetBufferFactory() const
{
  return mImpl->GetBufferFactory();
}

Dali::Graphics::FramebufferFactory& Controller::GetFramebufferFactory() const
{
  return mImpl->GetFramebufferFactory();
}

Dali::Graphics::PipelineFactory& Controller::GetPipelineFactory()
{
  mImpl->mPipelineFactory->Reset();
  return *mImpl->mPipelineFactory;
}

Dali::Graphics::SamplerFactory& Controller::GetSamplerFactory()
{
  return mImpl->mSamplerFactory->Reset();
}

Vulkan::Graphics& Controller::GetGraphics() const
{
  return mImpl->mGraphics;
}

void Controller::ScheduleBufferMemoryTransfer( std::unique_ptr< VulkanAPI::BufferMemoryTransfer > transferRequest )
{
  mImpl->mBufferTransferRequests.emplace_back( std::move( transferRequest ) );
}

void Controller::ScheduleResourceTransfer( VulkanAPI::ResourceTransferRequest&& transferRequest )
{
  std::lock_guard<std::recursive_mutex> lock(mImpl->mResourceTransferMutex);
  mImpl->mResourceTransferRequests.emplace_back( std::move(transferRequest) );

  // if we requested immediate upload then request will be processed instantly with skipping
  // all the deferred update requests
  if( !mImpl->mResourceTransferRequests.back().deferredTransferMode )
  {
    mImpl->ProcessResourceTransferRequests( true );
  }
}

bool Controller::HasPendingResourceTransfers() const
{
  return !mImpl->mResourceTransferRequests.empty();
}

void Controller::SubmitCommands( std::vector< Dali::Graphics::RenderCommand* > commands )
{
  mImpl->SubmitCommands( std::move( commands ) );
}

std::unique_ptr< Dali::Graphics::RenderCommand > Controller::AllocateRenderCommand()
{
  return mImpl->AllocateRenderCommand();
}

bool Controller::EnableDepthStencilBuffer( bool enableDepth, bool enableStencil )
{
  return mImpl->EnableDepthStencilBuffer( enableDepth, enableStencil );
}

void Controller::RunGarbageCollector( size_t numberOfDiscardedRenderers )
{
  mImpl->RunGarbageCollector( numberOfDiscardedRenderers );
}

void Controller::DiscardUnusedResources()
{
  mImpl->DiscardUnusedResources();
}

bool Controller::IsDiscardQueueEmpty()
{
  return mImpl->IsDiscardQueueEmpty();
}

bool Controller::IsDrawOnResumeRequired()
{
  return mImpl->mDrawOnResume;
}

void Controller::WaitIdle()
{
  mImpl->mGraphics.GetGraphicsQueue(0u).GetVkHandle().waitIdle();
}

void Controller::SwapBuffers()
{
  mImpl->SwapBuffers();
}

uint32_t Controller::GetSwapchainBufferCount()
{
  return mImpl->GetSwapchainBufferCount();
}

uint32_t Controller::GetCurrentBufferIndex()
{
  return mImpl->mGraphics.GetCurrentBufferIndex();
}

Dali::SharedFuture Controller::InitializeTextureStagingBuffer( uint32_t size, bool useWorkerThread )
{
  // Check if we can reuse existing staging buffer for that frame
  if( !mImpl->mTextureStagingBuffer ||
      mImpl->mTextureStagingBuffer->GetBufferRef()->GetSize() < size )
  {
    auto workerFunc = [&, size](auto workerIndex){
      mImpl->mTextureStagingBuffer.reset(static_cast<VulkanAPI::Buffer*>(this->CreateBuffer(
        this->GetBufferFactory()
            .SetSize( size )
            .SetUsageFlags( 0u | Dali::Graphics::BufferUsage::TRANSFER_SRC )).release()));
      mImpl->mTextureStagingBuffer->Map();
    };

    if( useWorkerThread )
    {
      return mImpl->mThreadPool.SubmitTask( 0u, workerFunc );
    }
    else
    {
      workerFunc(0);
    }
  }
  return {};
}

void Controller::UpdateTextures( const std::vector<Dali::Graphics::TextureUpdateInfo>& updateInfoList, const std::vector<Dali::Graphics::TextureUpdateSourceInfo>& sourceList )
{
  using MemoryUpdateAndOffset = std::pair<const Dali::Graphics::TextureUpdateInfo*, uint32_t>;
  std::vector<MemoryUpdateAndOffset> relevantUpdates{};

  std::vector<Task> copyTasks{};

  relevantUpdates.reserve( updateInfoList.size() );
  copyTasks.reserve( updateInfoList.size() );

  uint32_t totalStagingBufferSize{0u};

  void* stagingBufferMappedPtr = nullptr;

  /**
   * If a texture appears more than once we need to process it preserving the order
   * of updates. It's necessary to make sure that all updates will run on
   * the same thread.
   */
  struct TextureTask
  {
    TextureTask( const Dali::Graphics::TextureUpdateInfo* i, const Dali::Task& t )
      : pInfo( i ), copyTask( t ) {}
    const Dali::Graphics::TextureUpdateInfo* pInfo;
    Dali::Task                copyTask;
  };

  std::map<Dali::Graphics::Texture*, std::vector<TextureTask>> updateMap;
  for( auto& info : updateInfoList )
  {
    updateMap[info.dstTexture].emplace_back( &info, nullptr );
  }

  // make a copy of update info lists by storing additional information
  for( auto& aTextureInfo : updateMap )
  {
    auto texture = aTextureInfo.first;
    for( auto& textureTask : aTextureInfo.second )
    {
      auto& info = *textureTask.pInfo;
      const auto& source = sourceList[ info.srcReference ];
      if( source.sourceType == Dali::Graphics::TextureUpdateSourceInfo::Type::Memory )
      {
        auto sourcePtr = reinterpret_cast<char*>(source.memorySource.pMemory);
        auto sourceInfoPtr = &source;
        auto pInfo = textureTask.pInfo;

        // If the texture supports direct write access, then we can
        // schedule direct copy task and skip the GPU upload. The update
        // should be fully complete.
        if( info.dstTexture->GetProperties().directWriteAccessEnabled )
        {
          auto taskLambda = [ pInfo, sourcePtr, sourceInfoPtr, texture ]( auto workerIndex )
          {
            auto vulkanTexture = static_cast<VulkanAPI::Texture*>( texture );
            const auto& properties = vulkanTexture->GetProperties();

            if( properties.emulated )
            {
              std::vector<char> data;
              data.resize( vulkanTexture->GetMemoryRequirements().size );
              vulkanTexture->TryConvertPixelData( sourcePtr, pInfo->srcSize, pInfo->srcExtent2D.width, pInfo->srcExtent2D.height, &data[0] );

              // substitute temporary source
              Dali::Graphics::TextureUpdateSourceInfo newSource{};
              newSource.memorySource.pMemory = data.data();
              vulkanTexture->CopyMemoryDirect( *pInfo, newSource, false );
            }
            else
            {
              vulkanTexture->CopyMemoryDirect( *pInfo, *sourceInfoPtr, false );
            }
          };
          textureTask.copyTask = taskLambda;
        }
        else
        {
          const auto size = info.dstTexture->GetMemoryRequirements().size;
          auto currentOffset = totalStagingBufferSize;

          relevantUpdates.emplace_back( &info, currentOffset );
          totalStagingBufferSize += uint32_t(size);
          auto ppStagingMemory = &stagingBufferMappedPtr; // this pointer will be set later!

          // The staging buffer is not allocated yet. The task knows pointer to the pointer which will point
          // at staging buffer right before executing tasks. The function will either perform direct copy
          // or will do suitable conversion if source format isn't supported and emulation is available.
          auto taskLambda = [ppStagingMemory, currentOffset, pInfo, sourcePtr, texture ]( auto workerThread ){

            char* pStagingMemory = reinterpret_cast<char*>(*ppStagingMemory);

            auto vulkanTexture = static_cast<VulkanAPI::Texture*>( texture );

            // Try to initialise` texture resources explicitly if they are not yet initialised
            vulkanTexture->InitialiseResources();

            // If texture is 'emulated' convert pixel data otherwise do direct copy
            const auto& properties = vulkanTexture->GetProperties();

            if( properties.emulated )
            {
              vulkanTexture->TryConvertPixelData( sourcePtr, pInfo->srcSize, pInfo->srcExtent2D.width, pInfo->srcExtent2D.height, &pStagingMemory[currentOffset] );
            }
            else
            {
              std::copy( sourcePtr, sourcePtr + pInfo->srcSize, &pStagingMemory[currentOffset] );
            }
          };

          // Add task
          textureTask.copyTask = taskLambda;
          relevantUpdates.emplace_back( &info, currentOffset );
        }
      }
      else
      {
        // for other source types offset within staging buffer doesn't matter
        relevantUpdates.emplace_back( &info, 1u );
      }
    }
  }

  // Prepare one task per each texture to make sure sequential order of updates
  // for the same texture.
  // @todo: this step probably can be avoid in case of using optimal tiling!
  for( auto& item : updateMap )
  {
    auto pUpdates = &item.second;
    auto task = [ pUpdates ]( auto workerIndex )
    {
      for( auto& update : *pUpdates )
      {
        update.copyTask( workerIndex );
      }
    };
    copyTasks.emplace_back( task );
  }

  // Allocate staging buffer for all updates using CPU memory
  // as source. The staging buffer exists only for a time of 1 frame.
  auto& threadPool = mImpl->mThreadPool;

  // Make sure the Initialise() function is not busy with creating first staging buffer
  if( mImpl->mTextureStagingBufferFuture )
  {
    mImpl->mTextureStagingBufferFuture->Wait();
    mImpl->mTextureStagingBufferFuture.reset();
  }

  // Check whether we need staging buffer and if we can reuse existing staging buffer for that frame.
  if( totalStagingBufferSize )
  {
    if( !mImpl->mTextureStagingBuffer ||
        mImpl->mTextureStagingBuffer->GetBufferRef()->GetSize() < totalStagingBufferSize )
    {
      // Initialise new staging buffer. Since caller function is parallelized, initialisation
      // stays on the caller thread.
      InitializeTextureStagingBuffer( totalStagingBufferSize, false );
    }

    // Write into memory in parallel
    stagingBufferMappedPtr = mImpl->mTextureStagingBuffer->Map();
  }

  // Submit tasks
  auto futures = threadPool.SubmitTasks( copyTasks, 100u );
  futures->Wait();

  // Unmap memory
  // mImpl->mTextureStagingBuffer->Unmap();

  // Nothing to update on the GPU, then exit now
  if( relevantUpdates.empty() )
  {
    return;
  }

  for( auto& pair : relevantUpdates )
  {
    auto& info = *pair.first;
    const auto& source = sourceList[info.srcReference];
    switch( source.sourceType )
    {
      // directly copy buffer
      case Dali::Graphics::TextureUpdateSourceInfo::Type::Buffer:
      {
        info.dstTexture->CopyBuffer( *source.bufferSource.buffer,
                                     info.srcOffset,
                                     info.srcExtent2D,
                                     info.dstOffset2D,
                                     info.layer, // layer
                                     info.level, // mipmap
                                     {} ); // update mode, deprecated
        break;
      }
      // for memory, use staging buffer
      case Dali::Graphics::TextureUpdateSourceInfo::Type::Memory:
      {
        auto memoryBufferOffset = pair.second;
        info.dstTexture->CopyBuffer( *mImpl->mTextureStagingBuffer,
                                     memoryBufferOffset,
                                     info.srcExtent2D,
                                     info.dstOffset2D,
                                     info.layer, // layer
                                     info.level, // mipmap
                                     {} ); // update mode, deprecated
        break;
      }

      case Dali::Graphics::TextureUpdateSourceInfo::Type::Texture:
        // Unsupported
        break;
    }
  }
}

} // namespace VulkanAPI
} // namespace Graphics
} // namespace Dali
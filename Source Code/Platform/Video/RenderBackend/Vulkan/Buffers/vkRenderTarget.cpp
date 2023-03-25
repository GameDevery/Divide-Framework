#include "stdafx.h"

#include "Headers/vkRenderTarget.h"

#include "Platform/Video/RenderBackend/Vulkan/Headers/VKWrapper.h"
#include "Platform/Video/RenderBackend/Vulkan/Textures/Headers/vkTexture.h"

namespace Divide
{
    vkRenderTarget::vkRenderTarget( GFXDevice& context, const RenderTargetDescriptor& descriptor )
        : RenderTarget( context, descriptor )
    {
        _renderingInfo = {};
        _renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        
        for ( auto& info : _colourAttachmentInfo )
        {
            info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            info.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        }

        auto& info = _depthAttachmentInfo;
        info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        info.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
    }

    bool vkRenderTarget::create()
    {
        if ( RenderTarget::create() )
        {
            _renderingInfo.renderArea.offset.x = 0;
            _renderingInfo.renderArea.offset.y = 0;
            _renderingInfo.renderArea.extent.width = getWidth();
            _renderingInfo.renderArea.extent.height = getHeight();
            _renderingInfo.layerCount = 1u;
            return true;
        }

        return false;
    }

    void vkRenderTarget::readData( [[maybe_unused]] const vec4<U16> rect, [[maybe_unused]] GFXImageFormat imageFormat, [[maybe_unused]] GFXDataFormat dataType, [[maybe_unused]] std::pair<bufferPtr, size_t> outData ) const noexcept
    {

    }

    void vkRenderTarget::blitFrom( VkCommandBuffer cmdBuffer, vkRenderTarget* source, const RTBlitParams& params ) noexcept
    {
        PROFILE_SCOPE_AUTO( Profiler::Category::Graphics );

        if ( source == nullptr || !IsValid(params) )
        {
            return;
        }

        enum class TransitionType : U8
        {
            SHADER_READ_TO_BLIT_READ,
            SHADER_READ_TO_BLIT_WRITE,
            BLIT_READ_TO_SHADER_READ,
            BLIT_WRITE_TO_SHADER_READ,
            COUNT
        };

        const auto transitionImage = [&cmdBuffer]( vkTexture* tex, VkImage image, VkImageMemoryBarrier2& memBarrierOut, TransitionType transitionType )
        {
            ImageSubRange subRange = tex->getView()._subRange;

            if ( tex->descriptor().texType() == TextureType::TEXTURE_CUBE_ARRAY )
            {
                subRange._layerRange.offset *= 6u;
                if ( subRange._layerRange.count != U16_MAX )
                {
                    subRange._layerRange.count *= 6u;
                }
            }

            memBarrierOut = vk::imageMemoryBarrier2();

            memBarrierOut.subresourceRange.aspectMask = vkTexture::GetAspectFlags( tex->descriptor() );
            memBarrierOut.subresourceRange.baseMipLevel = subRange._mipLevels.offset;
            memBarrierOut.subresourceRange.levelCount = subRange._mipLevels.count == U16_MAX ? VK_REMAINING_MIP_LEVELS : subRange._mipLevels.count;
            memBarrierOut.subresourceRange.baseArrayLayer = subRange._layerRange.offset;
            memBarrierOut.subresourceRange.layerCount = subRange._layerRange.count == U16_MAX ? VK_REMAINING_ARRAY_LAYERS : subRange._layerRange.count;
            memBarrierOut.image = image;

            const VkImageLayout targetLayout = IsDepthTexture( tex->descriptor().baseFormat() ) ? VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            switch ( transitionType )
            {
                case TransitionType::SHADER_READ_TO_BLIT_READ :
                {
                    memBarrierOut.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
                    memBarrierOut.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
                    memBarrierOut.oldLayout = targetLayout;
                    memBarrierOut.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
                    memBarrierOut.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT | VK_PIPELINE_STAGE_2_BLIT_BIT;
                    memBarrierOut.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                } break;
                case TransitionType::SHADER_READ_TO_BLIT_WRITE :
                {
                    memBarrierOut.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
                    memBarrierOut.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
                    memBarrierOut.oldLayout = targetLayout;
                    memBarrierOut.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
                    memBarrierOut.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT | VK_PIPELINE_STAGE_2_BLIT_BIT;
                    memBarrierOut.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

                } break;
                case TransitionType::BLIT_READ_TO_SHADER_READ :
                {
                    memBarrierOut.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
                    memBarrierOut.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT | VK_PIPELINE_STAGE_2_BLIT_BIT;
                    memBarrierOut.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                    memBarrierOut.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
                    memBarrierOut.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
                    memBarrierOut.newLayout = targetLayout;
                } break;
                case TransitionType::BLIT_WRITE_TO_SHADER_READ :
                {
                    memBarrierOut.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
                    memBarrierOut.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT | VK_PIPELINE_STAGE_2_BLIT_BIT;
                    memBarrierOut.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                    memBarrierOut.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
                    memBarrierOut.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
                    memBarrierOut.newLayout = targetLayout;
                } break;

                default: DIVIDE_UNEXPECTED_CALL(); break;
            };
        };

        VK_API::PushDebugMessage(cmdBuffer, "vkRrenderTarget::blitFrom");

        vkRenderTarget* input = source;
        vkRenderTarget* output = this;
        const vec2<U16> inputDim = input->_descriptor._resolution;
        const vec2<U16> outputDim = output->_descriptor._resolution;

        VkDependencyInfo dependencyInfo = vk::dependencyInfo();
        for ( const RTBlitEntry entry : params )
        {
            if ( entry._input._index == INVALID_INDEX ||
                 entry._output._index == INVALID_INDEX ||
                 !input->_attachmentsUsed[entry._input._index] ||
                 !output->_attachmentsUsed[entry._output._index] )
            {
                continue;
            }

            std::array<VkImageMemoryBarrier2, 2> imageBarriers{};
            U8 imageBarrierCount = 0u;

            const RTAttachment_uptr& inAtt = input->_attachments[entry._input._index];
            const RTAttachment_uptr& outAtt = output->_attachments[entry._output._index];

            vkTexture* vkTexIn = static_cast<vkTexture*>(inAtt->texture().get());
            vkTexture* vkTexOut = static_cast<vkTexture*>(outAtt->texture().get());

            const bool needsResolve = vkTexIn->sampleFlagBits() != vkTexOut->sampleFlagBits() && vkTexIn->sampleFlagBits() != VK_SAMPLE_COUNT_1_BIT;

            transitionImage( vkTexIn, needsResolve ? vkTexIn->resolvedImage()->_image : vkTexIn->image()->_image, imageBarriers[imageBarrierCount++], TransitionType::SHADER_READ_TO_BLIT_READ);
            transitionImage( vkTexOut, vkTexOut->image()->_image, imageBarriers[imageBarrierCount++], TransitionType::SHADER_READ_TO_BLIT_WRITE );

            if ( imageBarrierCount > 0u )
            {
                dependencyInfo.imageMemoryBarrierCount = imageBarrierCount;
                dependencyInfo.pImageMemoryBarriers = imageBarriers.data();

                vkCmdPipelineBarrier2( cmdBuffer, &dependencyInfo );
                imageBarrierCount = 0u;
            }

            VkImageBlit2 blitRegions[MAX_BLIT_ENTRIES] = {};
            U8 blitRegionCount = 0u;

            const U8 layerCount = entry._layerCount * IsCubeTexture( vkTexIn->descriptor().texType() ) ? 6u : 1u;

            for ( U8 mip = 0u; mip < entry._mipCount; ++mip )
            {
                const VkImageSubresourceLayers srcSubResource = {
                    .aspectMask = VkImageAspectFlags( entry._input._index == RT_DEPTH_ATTACHMENT_IDX ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT ),
                    .mipLevel = to_U32( entry._input._mipOffset + mip ),
                    .baseArrayLayer = entry._input._layerOffset,
                    .layerCount = layerCount,
                };

                const VkImageSubresourceLayers dstSubResource = {
                    .aspectMask = VkImageAspectFlags( entry._output._index == RT_DEPTH_ATTACHMENT_IDX ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT ),
                    .mipLevel = to_U32( entry._output._mipOffset + mip ),
                    .baseArrayLayer = entry._output._layerOffset,
                    .layerCount = layerCount,
                };

                VkImageBlit2& blitRegion = blitRegions[blitRegionCount++];

                blitRegion = {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
                    .srcSubresource = srcSubResource,
                    .dstSubresource = dstSubResource,
                };

                blitRegion.srcOffsets[0] = blitRegion.dstOffsets[0] = {
                    .x = 0,
                    .y = 0,
                    .z = 0
                };

                blitRegion.srcOffsets[1] = {
                    .x = vkTexIn->width(),
                    .y = vkTexIn->height(),
                    .z = vkTexIn->depth()
                };

                blitRegion.dstOffsets[1] = {
                    .x = vkTexOut->width(),
                    .y = vkTexOut->height(),
                    .z = vkTexOut->depth()
                };

            }

            if ( blitRegionCount > 0u )
            {
                const bool msSource = vkTexIn->sampleFlagBits() != VK_SAMPLE_COUNT_1_BIT;

                const VkBlitImageInfo2 blitInfo = {
                    .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
                    .srcImage = msSource ? vkTexIn->resolvedImage()->_image : vkTexIn->image()->_image,
                    .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    .dstImage = vkTexOut->image()->_image,
                    .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    .regionCount = blitRegionCount,
                    .pRegions = blitRegions,
                    .filter = VK_FILTER_NEAREST,
                };

                vkCmdBlitImage2( cmdBuffer, &blitInfo );
            }

            transitionImage( vkTexIn, needsResolve ? vkTexIn->resolvedImage()->_image : vkTexIn->image()->_image, imageBarriers[imageBarrierCount++],  TransitionType::BLIT_READ_TO_SHADER_READ );
            transitionImage( vkTexOut, vkTexOut->image()->_image, imageBarriers[imageBarrierCount++], TransitionType::BLIT_WRITE_TO_SHADER_READ );
            if ( imageBarrierCount > 0u )
            {
                dependencyInfo.imageMemoryBarrierCount = imageBarrierCount;
                dependencyInfo.pImageMemoryBarriers = imageBarriers.data();

                vkCmdPipelineBarrier2( cmdBuffer, &dependencyInfo );
                imageBarrierCount = 0u;
            }
        }

        VK_API::PopDebugMessage( cmdBuffer );
    }

    void vkRenderTarget::transitionAttachments( VkCommandBuffer cmdBuffer, const RTDrawDescriptor& descriptor, const bool toWrite )
    {
        // Double the number of barriers needed in case we have an MSAA RenderTarget (we need to transition the resolve targets as well)
        static std::array<VkImageMemoryBarrier2, (to_base( RTColourAttachmentSlot::COUNT ) + 1) * 2> memBarriers{};
        U8 memBarrierCount = 0u;

        const U16 srcDepthLayer = descriptor._writeLayers[RT_DEPTH_ATTACHMENT_IDX];
        U16 targetDepthLayer = 0u;

        bool needLayeredDepth = (srcDepthLayer != INVALID_INDEX && srcDepthLayer > 0u);

        for ( U8 i = 0u; i < to_base( RTColourAttachmentSlot::COUNT ); ++i )
        {
            if ( !_attachmentsUsed[i] )
            {
                continue;
            }

            if ( descriptor._writeLayers[i] != INVALID_INDEX && descriptor._writeLayers[i] > 0u )
            {
                targetDepthLayer = descriptor._writeLayers[i];
                needLayeredDepth = true;
                break;
            }
        }

        DIVIDE_ASSERT( descriptor._mipWriteLevel != INVALID_INDEX );

        const auto transitionTexture = [toWrite](const VkImageSubresourceRange& subresourceRange, const VkImage image, const bool isDepth, const bool hasStencil, const bool isResolve, VkImageMemoryBarrier2& memBarrier)
        {
            memBarrier = vk::imageMemoryBarrier2();
            memBarrier.image = image;
            memBarrier.subresourceRange = subresourceRange;

            if ( toWrite )
            {
                memBarrier.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
                memBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
                memBarrier.oldLayout = isDepth ? VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                if (isDepth )
                {
                    memBarrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                    memBarrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_RESOLVE_BIT;
                    memBarrier.newLayout = hasStencil ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
                }
                else
                {
                    memBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
                    memBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_2_RESOLVE_BIT;
                    memBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                }
            }
            else
            {
                memBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
                memBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
                memBarrier.newLayout = isDepth ? VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                if (isDepth )
                {

                    memBarrier.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                    memBarrier.srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_RESOLVE_BIT;
                    memBarrier.oldLayout = hasStencil ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
                }
                else
                {
                    memBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
                    memBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_2_RESOLVE_BIT;
                    memBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                }
            }
        };

        VkImageSubresourceRange subresourceRange{};
        for ( U8 i = 0u; i < to_base( RTColourAttachmentSlot::COUNT ); ++i )
        {
            if ( _attachmentsUsed[i] && descriptor._drawMask[i] )
            {
                vkTexture* vkTex = static_cast<vkTexture*>(_attachments[i]->texture().get());

                ImageView targetView = vkTex->getView();
                if ( needLayeredDepth || (descriptor._writeLayers[i] != INVALID_INDEX && descriptor._writeLayers[i] > 0) )
                {
                    const U16 targetColourLayer = descriptor._writeLayers[i] == INVALID_INDEX ? targetDepthLayer : descriptor._writeLayers[i];
                    targetView._subRange._layerRange = { targetColourLayer, 1u };
                }
                else if ( descriptor._mipWriteLevel > 0u )
                {
                    targetView._subRange._mipLevels =  { descriptor._mipWriteLevel, 1u };
                }

                subresourceRange.aspectMask = vkTexture::GetAspectFlags( vkTex->descriptor() );
                subresourceRange.baseMipLevel = targetView._subRange._mipLevels.offset;
                subresourceRange.levelCount = targetView._subRange._mipLevels.count == U16_MAX ? VK_REMAINING_MIP_LEVELS : targetView._subRange._mipLevels.count;
                subresourceRange.baseArrayLayer = targetView._subRange._layerRange.offset;
                subresourceRange.layerCount = targetView._subRange._layerRange.count == U16_MAX ? VK_REMAINING_ARRAY_LAYERS : targetView._subRange._layerRange.count;

                transitionTexture( subresourceRange, vkTex->image()->_image, false, false, false, memBarriers[memBarrierCount++] );

                if ( vkTex->sampleFlagBits() != VK_SAMPLE_COUNT_1_BIT )
                {
                    subresourceRange.aspectMask = vkTexture::GetAspectFlags( vkTex->descriptor() );
                    subresourceRange.baseMipLevel = 0u;
                    subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
                    subresourceRange.baseArrayLayer = 0u;
                    subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
                    transitionTexture( subresourceRange, vkTex->resolvedImage()->_image, false, false, true, memBarriers[memBarrierCount++] );
                }
            }
        }

        if ( _attachmentsUsed[RT_DEPTH_ATTACHMENT_IDX] )
        {

            const auto& att = _attachments[RT_DEPTH_ATTACHMENT_IDX];
            vkTexture* vkTex = static_cast<vkTexture*>(att->texture().get());

            ImageView targetView = vkTex->getView();
            if ( needLayeredDepth )
            {
                targetView._subRange._layerRange = { srcDepthLayer == INVALID_INDEX ? targetDepthLayer : srcDepthLayer, 1u };
            }
            else if ( descriptor._mipWriteLevel > 0u )
            {
                targetView._subRange._mipLevels = { descriptor._mipWriteLevel, 1u };
            }
            
            const bool hasStencil = att->descriptor()._type == RTAttachmentType::DEPTH_STENCIL;

            subresourceRange.aspectMask = vkTexture::GetAspectFlags( vkTex->descriptor() );
            subresourceRange.baseMipLevel = targetView._subRange._mipLevels.offset;
            subresourceRange.levelCount = targetView._subRange._mipLevels.count == U16_MAX ? VK_REMAINING_MIP_LEVELS : targetView._subRange._mipLevels.count;
            subresourceRange.baseArrayLayer = targetView._subRange._layerRange.offset;
            subresourceRange.layerCount = targetView._subRange._layerRange.count == U16_MAX ? VK_REMAINING_ARRAY_LAYERS : targetView._subRange._layerRange.count;

            transitionTexture( subresourceRange, vkTex->image()->_image, true, hasStencil, false, memBarriers[memBarrierCount++]);
            
            if ( vkTex->sampleFlagBits() != VK_SAMPLE_COUNT_1_BIT )
            {
                subresourceRange.aspectMask = vkTexture::GetAspectFlags( vkTex->descriptor() );
                subresourceRange.baseMipLevel = 0u;
                subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
                subresourceRange.baseArrayLayer = 0u;
                subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
                transitionTexture( subresourceRange, vkTex->resolvedImage()->_image, true, hasStencil, true, memBarriers[memBarrierCount++] );
            }
        }

        if ( memBarrierCount > 0u )
        {
            VkDependencyInfo dependencyInfo = vk::dependencyInfo();
            dependencyInfo.imageMemoryBarrierCount = memBarrierCount;
            dependencyInfo.pImageMemoryBarriers = memBarriers.data();

            vkCmdPipelineBarrier2( cmdBuffer, &dependencyInfo );
        }
    }


    void vkRenderTarget::begin( VkCommandBuffer cmdBuffer, const RTDrawDescriptor& descriptor, const RTClearDescriptor& clearPolicy, VkPipelineRenderingCreateInfo& pipelineRenderingCreateInfoOut )
    {
        _previousPolicy = descriptor;

        assert(pipelineRenderingCreateInfoOut.sType == VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO);

        const bool needLayeredColour = descriptor._writeLayers[RT_DEPTH_ATTACHMENT_IDX] != INVALID_INDEX;
        U16 targetColourLayer = needLayeredColour ? descriptor._writeLayers[RT_DEPTH_ATTACHMENT_IDX] : 0u;

        bool needLayeredDepth = false;
        U16 targetDepthLayer = 0u;
        for ( U8 i = 0u; i < to_base( RTColourAttachmentSlot::COUNT ); ++i )
        {
            if ( !_attachmentsUsed[i] )
            {
                continue;
            }

            if ( descriptor._writeLayers[i] != INVALID_INDEX )
            {
                needLayeredDepth = true;
                targetDepthLayer = descriptor._writeLayers[i];
                break;
            }
        }

        vkTexture::CachedImageView::Descriptor imageViewDescriptor{};
        imageViewDescriptor._subRange = {};

        U8 stagingIndex = 0u;
        VkImageMemoryBarrier2 memBarrier{};
        for ( U8 i = 0u; i < to_base( RTColourAttachmentSlot::COUNT ); ++i )
        {
            if ( _attachmentsUsed[i] && descriptor._drawMask[i] )
            {
                vkTexture* vkTex = static_cast<vkTexture*>(_attachments[i]->texture().get());
                imageViewDescriptor._subRange = vkTex->getView()._subRange;
                if ( descriptor._writeLayers[i] != INVALID_INDEX || needLayeredColour )
                {
                    targetColourLayer = descriptor._writeLayers[i] == INVALID_INDEX ? targetColourLayer : descriptor._writeLayers[i];
                    imageViewDescriptor._subRange._layerRange = { targetColourLayer, 1u };
                }
                else if ( descriptor._mipWriteLevel > 0u )
                {
                    imageViewDescriptor._subRange._mipLevels = { descriptor._mipWriteLevel, 1u };
                }

                imageViewDescriptor._format = vkTex->vkFormat();
                imageViewDescriptor._type = imageViewDescriptor._subRange._layerRange.count > 1u ? TextureType::TEXTURE_2D_ARRAY : TextureType::TEXTURE_2D;
                imageViewDescriptor._usage = ImageUsage::RT_COLOUR_ATTACHMENT;

                VkRenderingAttachmentInfo& info = _colourAttachmentInfo[i];
                info.imageView = vkTex->getImageView( imageViewDescriptor );
                _colourAttachmentFormats[stagingIndex] = vkTex->vkFormat();
                if ( clearPolicy[i]._enabled || !_attachmentsUsed[i] )
                {
                    info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                    info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                    info.clearValue.color = {
                        clearPolicy[i]._colour.r,
                        clearPolicy[i]._colour.g,
                        clearPolicy[i]._colour.b,
                        clearPolicy[i]._colour.a
                    };
                }
                else
                {
                    info.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
                    info.storeOp = VK_ATTACHMENT_STORE_OP_NONE;
                    info.clearValue = {};
                }

                if ( vkTex->sampleFlagBits() != VK_SAMPLE_COUNT_1_BIT )
                {
                    imageViewDescriptor._resolveTarget = true;
                    info.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
                    info.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                    info.resolveImageView = vkTex->getImageView( imageViewDescriptor );
                    imageViewDescriptor._resolveTarget = false;
                }
                _stagingColourAttachmentInfo[stagingIndex] = info;
            }
            else
            {
                _colourAttachmentFormats[stagingIndex] = VK_FORMAT_UNDEFINED;
                _stagingColourAttachmentInfo[stagingIndex] = {};
                _stagingColourAttachmentInfo[stagingIndex].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            }

            ++stagingIndex;
        }

        pipelineRenderingCreateInfoOut.colorAttachmentCount = stagingIndex;
        pipelineRenderingCreateInfoOut.pColorAttachmentFormats = _colourAttachmentFormats.data();

        _renderingInfo.colorAttachmentCount = stagingIndex;
        _renderingInfo.pColorAttachments = _stagingColourAttachmentInfo.data();

        if ( _attachmentsUsed[RT_DEPTH_ATTACHMENT_IDX] )
        {
            const auto& att = _attachments[RT_DEPTH_ATTACHMENT_IDX];
            const bool hasStencil = att->descriptor()._type == RTAttachmentType::DEPTH_STENCIL;

            vkTexture* vkTex = static_cast<vkTexture*>(att->texture().get());
            imageViewDescriptor._subRange = vkTex->getView()._subRange;
            if ( descriptor._writeLayers[RT_DEPTH_ATTACHMENT_IDX] != INVALID_INDEX || needLayeredDepth )
            {
                targetDepthLayer = descriptor._writeLayers[RT_DEPTH_ATTACHMENT_IDX] == INVALID_INDEX ? targetDepthLayer : descriptor._writeLayers[RT_DEPTH_ATTACHMENT_IDX];
                imageViewDescriptor._subRange._layerRange = { targetDepthLayer, 1u };
            }
            else if ( descriptor._mipWriteLevel != U16_MAX )
            {
                imageViewDescriptor._subRange._mipLevels = { descriptor._mipWriteLevel, 1u };
            }


            imageViewDescriptor._format = vkTex->vkFormat();
            imageViewDescriptor._type = imageViewDescriptor._subRange._layerRange.count > 1u ? TextureType::TEXTURE_2D_ARRAY : TextureType::TEXTURE_2D;
            imageViewDescriptor._usage = hasStencil ? ImageUsage::RT_DEPTH_STENCIL_ATTACHMENT : ImageUsage::RT_DEPTH_ATTACHMENT;

            _depthAttachmentInfo.imageView = vkTex->getImageView( imageViewDescriptor );

            if ( clearPolicy[RT_DEPTH_ATTACHMENT_IDX]._enabled )
            {
                _depthAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                _depthAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                _depthAttachmentInfo.clearValue.depthStencil.depth = clearPolicy[RT_DEPTH_ATTACHMENT_IDX]._colour.r;
                _depthAttachmentInfo.clearValue.depthStencil.stencil = to_U32(clearPolicy[RT_DEPTH_ATTACHMENT_IDX]._colour.g);
            }
            else
            {
                _depthAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
                _depthAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_NONE;
                _depthAttachmentInfo.clearValue.depthStencil.depth = 1.f;
            }

            if (vkTex->sampleFlagBits() != VK_SAMPLE_COUNT_1_BIT )
            {
                imageViewDescriptor._resolveTarget = true;
                _depthAttachmentInfo.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
                _depthAttachmentInfo.resolveImageLayout = hasStencil ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
                _depthAttachmentInfo.resolveImageView = vkTex->getImageView( imageViewDescriptor );
                imageViewDescriptor._resolveTarget = false;
            }
            pipelineRenderingCreateInfoOut.depthAttachmentFormat = vkTex->vkFormat();
            _renderingInfo.pDepthAttachment = &_depthAttachmentInfo;
        }
        else
        {
            pipelineRenderingCreateInfoOut.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
            _renderingInfo.pDepthAttachment = nullptr;
        }

        transitionAttachments( cmdBuffer, descriptor, true );
    }

    void vkRenderTarget::end( VkCommandBuffer cmdBuffer )
    {
        transitionAttachments( cmdBuffer, _previousPolicy, false );
    }

}; //namespace Divide
#include "dk_renderer.hpp"

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <switch.h>
#include "debug.hpp"

// GLM headers
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES // Enforces GLSL std140/std430 alignment rules for glm types
#define GLM_FORCE_INTRINSICS               // Enables usage of SIMD CPU instructions (requiring the above as well)
#include <glm/vec2.hpp>

namespace {

    constexpr std::array VertexBufferState =
    {
        DkVtxBufferState{sizeof(NVGvertex), 0},
    };

    constexpr std::array VertexAttribState =
    {
        DkVtxAttribState{0, 0, offsetof(NVGvertex, x), DkVtxAttribSize_2x32, DkVtxAttribType_Float, 0},
        DkVtxAttribState{0, 0, offsetof(NVGvertex, u), DkVtxAttribSize_2x32, DkVtxAttribType_Float, 0},
    };

    struct View
    {
        glm::vec2 size;
    };

    void UpdateImage(dk::Image &image, CMemPool &scratchPool, dk::Device device, dk::Queue transferQueue, int type, int x, int y, int w, int h, const unsigned char *data)
    {
        // Do not proceed if no data is provided upfront
        if (data == nullptr)
        {
            return;
        }

        // Allocate memory from the pool for the image
        size_t imageSize = type == NVG_TEXTURE_RGBA ? w * h * 4 : w * h;
        CMemPool::Handle tempimgmem = scratchPool.allocate(imageSize, DK_IMAGE_LINEAR_STRIDE_ALIGNMENT);
        memcpy(tempimgmem.getCpuAddr(), data, imageSize);

        dk::UniqueCmdBuf tempcmdbuf = dk::CmdBufMaker{device}.create();
        CMemPool::Handle tempcmdmem = scratchPool.allocate(DK_MEMBLOCK_ALIGNMENT);
        tempcmdbuf.addMemory(tempcmdmem.getMemBlock(), tempcmdmem.getOffset(), tempcmdmem.getSize());

        dk::ImageView imageView{image};
        tempcmdbuf.copyBufferToImage({ tempimgmem.getGpuAddr() }, imageView, { static_cast<uint32_t>(x), static_cast<uint32_t>(y), 0, static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1 });

        transferQueue.submitCommands(tempcmdbuf.finishList());
        transferQueue.waitIdle();

        // Destroy temp mem
        tempcmdmem.destroy();
        tempimgmem.destroy();
    }

}

Texture::Texture(int id) : id(id)
{
}

Texture::~Texture()
{
    this->imageMem.destroy();
}

void Texture::Initialize(CMemPool &imagePool, CMemPool &scratchPool, dk::Device device, dk::Queue transferQueue, int type, int w, int h, int imageFlags, const unsigned char *data)
{
    this->textureDescriptor = {
        .width = w,
        .height = h,
        .type = type,
        .flags = imageFlags,
    };

    // Create an image layout
    dk::ImageLayout layout;
    auto layoutMaker = dk::ImageLayoutMaker{device}.setFlags(0).setDimensions(w, h);
    if (type == NVG_TEXTURE_RGBA)
    {
        layoutMaker.setFormat(DkImageFormat_RGBA8_Unorm);
    }
    else
    {
        layoutMaker.setFormat(DkImageFormat_R8_Unorm);
    }
    layoutMaker.initialize(layout);

    // Initialize image
    this->imageMem = imagePool.allocate(layout.getSize(), layout.getAlignment());
    this->image.initialize(layout, this->imageMem.getMemBlock(), this->imageMem.getOffset());
    this->imageDescriptor.initialize(this->image);

    // Only update the image if the data isn't null
    if (data != nullptr)
    {
        UpdateImage(this->image, scratchPool, device, transferQueue, type, 0, 0, w, h, data);
    }
}

int Texture::GetId()
{
    return this->id;
}

const DKNVGtextureDescriptor *Texture::GetDescriptor()
{
    return &this->textureDescriptor;
}

dk::Image &Texture::GetImage()
{
    return this->image;
}

dk::ImageDescriptor &Texture::GetImageDescriptor()
{
    return this->imageDescriptor;
}

DkRenderer::DkRenderer(unsigned int viewWidth, unsigned int viewHeight, dk::Device device, dk::Queue queue, CMemPool &imageMemPool, CMemPool &codeMemPool, CMemPool &dataMemPool) :
    viewWidth(viewWidth), viewHeight(viewHeight), device(device), queue(queue), imageMemPool(imageMemPool), codeMemPool(codeMemPool), dataMemPool(dataMemPool), imageDescriptorMappings({0})
{
    // Create a dynamic command buffer and allocate memory for it.
    this->dynCmdBuf = dk::CmdBufMaker{this->device}.create();
    this->dynCmdMem.allocate(this->dataMemPool, DynamicCmdSize);

    this->imageDescriptorSet.allocate(this->dataMemPool);
    this->samplerDescriptorSet.allocate(this->dataMemPool);

    this->viewUniformBuffer = this->dataMemPool.allocate(sizeof(View), DK_UNIFORM_BUF_ALIGNMENT);
    this->fragUniformBuffer = this->dataMemPool.allocate(sizeof(FragmentUniformSize), DK_UNIFORM_BUF_ALIGNMENT);

    // Create and bind preset samplers
    dk::UniqueCmdBuf initCmdBuf = dk::CmdBufMaker{this->device}.create();
    CMemPool::Handle initCmdMem = this->dataMemPool.allocate(DK_MEMBLOCK_ALIGNMENT);
    initCmdBuf.addMemory(initCmdMem.getMemBlock(), initCmdMem.getOffset(), initCmdMem.getSize());

    for (uint8_t i = 0; i < SamplerType_Total; i++)
    {
        const DkFilter filter = (i & SamplerType_Nearest) ? DkFilter_Nearest : DkFilter_Linear;
        const DkMipFilter mipFilter = (i & SamplerType_Nearest) ? DkMipFilter_Nearest : DkMipFilter_Linear;
        const DkWrapMode uWrapMode = (i & SamplerType_RepeatX) ? DkWrapMode_Repeat : DkWrapMode_ClampToEdge;
        const DkWrapMode vWrapMode = (i & SamplerType_RepeatY) ? DkWrapMode_Repeat : DkWrapMode_ClampToEdge;

        auto sampler = dk::Sampler{};
        auto samplerDescriptor = dk::SamplerDescriptor{};
        sampler.setFilter(filter, filter, (i & SamplerType_MipFilter) ? mipFilter : DkMipFilter_None);
        sampler.setWrapMode(uWrapMode, vWrapMode);
        samplerDescriptor.initialize(sampler);
        this->samplerDescriptorSet.update(initCmdBuf, i, samplerDescriptor);
    }

    // Flush the descriptor cache
    initCmdBuf.barrier(DkBarrier_None, DkInvalidateFlags_Descriptors);

    this->samplerDescriptorSet.bindForSamplers(initCmdBuf);
    this->imageDescriptorSet.bindForImages(initCmdBuf);

    this->queue.submitCommands(initCmdBuf.finishList());
    this->queue.waitIdle();

    initCmdMem.destroy();
    initCmdBuf.destroy();
}

DkRenderer::~DkRenderer()
{
    if (this->vertexBuffer)
    {
        this->vertexBuffer->destroy();
    }

    this->viewUniformBuffer.destroy();
    this->fragUniformBuffer.destroy();
    this->textures.clear();
}

int DkRenderer::AcquireImageDescriptor(std::shared_ptr<Texture> texture, int image)
{
    int freeImageDescriptor = this->lastImageDescriptor + 1;
    int mapping = 0;

    for (int desc = 0; desc <= this->lastImageDescriptor; desc++)
    {
        mapping = this->imageDescriptorMappings[desc];

        // We've found the image descriptor requested
        if (mapping == image)
        {
            return desc;
        }

        // Update the free image descriptor
        if (mapping == 0 && freeImageDescriptor == this->lastImageDescriptor + 1)
        {
            freeImageDescriptor = desc;
        }
    }

    // No descriptors are free
    if (freeImageDescriptor >= static_cast<int>(MaxImages))
    {
        return -1;
    }

    // Update descriptor sets
    this->imageDescriptorSet.update(this->dynCmdBuf, freeImageDescriptor, texture->GetImageDescriptor());

    // Flush the descriptor cache
    this->dynCmdBuf.barrier(DkBarrier_None, DkInvalidateFlags_Descriptors);

    // Update the map
    this->imageDescriptorMappings[freeImageDescriptor] = image;
    this->lastImageDescriptor = freeImageDescriptor;
    return freeImageDescriptor;
}

void DkRenderer::FreeImageDescriptor(int image)
{
    for (int desc = 0; desc <= this->lastImageDescriptor; desc++)
    {
        if (this->imageDescriptorMappings[desc] == image)
        {
            OutputDebugString("Freed image descriptor %d for image %d\n", desc, image);
            this->imageDescriptorMappings[desc] = 0;
        }
    }
}

CMemPool::Handle DkRenderer::CreateDataBuffer(const void *data, size_t size)
{
    auto buffer = this->dataMemPool.allocate(size);
    memcpy(buffer.getCpuAddr(), data, size);
    return buffer;
}

void DkRenderer::UpdateVertexBuffer(const void *data, size_t size)
{
    if (this->vertexBuffer)
    {
        this->vertexBuffer->destroy();
        this->vertexBuffer.reset();
    }

    this->vertexBuffer = CreateDataBuffer(data, size);
}

void DkRenderer::SetUniforms(DKNVGcontext *ctx, int offset, int image)
{
    this->dynCmdBuf.pushConstants(this->fragUniformBuffer.getGpuAddr(), this->fragUniformBuffer.getSize(), 0, ctx->fragSize, ctx->uniforms + offset);
    this->dynCmdBuf.bindUniformBuffer(DkStage_Fragment, 0, this->fragUniformBuffer.getGpuAddr(), this->fragUniformBuffer.getSize());

    auto texture = this->FindTexture(image);

    // Could not find a texture
    if (texture == nullptr)
    {
        return;
    }

    const int imageDescId = this->AcquireImageDescriptor(texture, image);

    // Failed to find a suitable image descriptor
    if (imageDescId == -1)
    {
        return;
    }

    const int imageFlags = texture->GetDescriptor()->flags;
    uint32_t samplerId = 0;

    if (imageFlags & NVG_IMAGE_GENERATE_MIPMAPS) samplerId |= SamplerType_MipFilter;
    if (imageFlags & NVG_IMAGE_NEAREST)          samplerId |= SamplerType_Nearest;
    if (imageFlags & NVG_IMAGE_REPEATX)          samplerId |= SamplerType_RepeatX;
    if (imageFlags & NVG_IMAGE_REPEATY)          samplerId |= SamplerType_RepeatY;

    this->dynCmdBuf.bindTextures(DkStage_Fragment, 0, dkMakeTextureHandle(imageDescId, samplerId));
}

void DkRenderer::DrawFill(DKNVGcontext *ctx, DKNVGcall *call)
{
    DKNVGpath *paths = &ctx->paths[call->pathOffset];
    int npaths = call->pathCount;

    // Set the stencils to be used
    this->dynCmdBuf.setStencil(DkFace_FrontAndBack, 0xFF, 0x0, 0xFF);

    // Set the depth stencil state
    auto depthStencilState = dk::DepthStencilState{}
        .setStencilTestEnable(true)
        .setStencilFrontCompareOp(DkCompareOp_Always)
        .setStencilFrontFailOp(DkStencilOp_Keep)
        .setStencilFrontDepthFailOp(DkStencilOp_Keep)
        .setStencilFrontPassOp(DkStencilOp_IncrWrap)
        .setStencilBackCompareOp(DkCompareOp_Always)
        .setStencilBackFailOp(DkStencilOp_Keep)
        .setStencilBackDepthFailOp(DkStencilOp_Keep)
        .setStencilBackPassOp(DkStencilOp_DecrWrap);
    this->dynCmdBuf.bindDepthStencilState(depthStencilState);

    // Configure for shape drawing
    this->dynCmdBuf.bindColorWriteState(dk::ColorWriteState{}.setMask(0, 0));
    this->SetUniforms(ctx, call->uniformOffset, 0);
    this->dynCmdBuf.bindRasterizerState(dk::RasterizerState{}.setCullMode(DkFace_None));

    // Draw vertices
    for (int i = 0; i < npaths; i++)
    {
        this->dynCmdBuf.draw(DkPrimitive_TriangleFan, paths[i].fillCount, 1, paths[i].fillOffset, 0);
    }

    this->dynCmdBuf.bindColorWriteState(dk::ColorWriteState{});
    this->SetUniforms(ctx, call->uniformOffset + ctx->fragSize, call->image);
    this->dynCmdBuf.bindRasterizerState(dk::RasterizerState{});

    if (ctx->flags & NVG_ANTIALIAS)
    {
        // Configure stencil anti-aliasing
        depthStencilState
            .setStencilFrontCompareOp(DkCompareOp_Equal)
            .setStencilFrontFailOp(DkStencilOp_Keep)
            .setStencilFrontDepthFailOp(DkStencilOp_Keep)
            .setStencilFrontPassOp(DkStencilOp_Keep)
            .setStencilBackCompareOp(DkCompareOp_Equal)
            .setStencilBackFailOp(DkStencilOp_Keep)
            .setStencilBackDepthFailOp(DkStencilOp_Keep)
            .setStencilBackPassOp(DkStencilOp_Keep);
        this->dynCmdBuf.bindDepthStencilState(depthStencilState);

        // Draw fringes
        for (int i = 0; i < npaths; i++)
        {
            this->dynCmdBuf.draw(DkPrimitive_TriangleStrip, paths[i].strokeCount, 1, paths[i].strokeOffset, 0);
        }
    }

    // Configure and draw fill
    depthStencilState
        .setStencilFrontCompareOp(DkCompareOp_NotEqual)
        .setStencilFrontFailOp(DkStencilOp_Zero)
        .setStencilFrontDepthFailOp(DkStencilOp_Zero)
        .setStencilFrontPassOp(DkStencilOp_Zero)
        .setStencilBackCompareOp(DkCompareOp_NotEqual)
        .setStencilBackFailOp(DkStencilOp_Zero)
        .setStencilBackDepthFailOp(DkStencilOp_Zero)
        .setStencilBackPassOp(DkStencilOp_Zero);
    this->dynCmdBuf.bindDepthStencilState(depthStencilState);

    this->dynCmdBuf.draw(DkPrimitive_TriangleStrip, call->triangleCount, 1, call->triangleOffset, 0);

    // Reset the depth stencil state to default
    this->dynCmdBuf.bindDepthStencilState(dk::DepthStencilState{});
}

void DkRenderer::DrawConvexFill(DKNVGcontext *ctx, DKNVGcall *call)
{
    DKNVGpath *paths = &ctx->paths[call->pathOffset];
    int npaths = call->pathCount;

    this->SetUniforms(ctx, call->uniformOffset, call->image);

    for (int i = 0; i < npaths; i++)
    {
        this->dynCmdBuf.draw(DkPrimitive_TriangleFan, paths[i].fillCount, 1, paths[i].fillOffset, 0);

        // Draw fringes
        if (paths[i].strokeCount > 0)
        {
            this->dynCmdBuf.draw(DkPrimitive_TriangleStrip, paths[i].strokeCount, 1, paths[i].strokeOffset, 0);
        }
    }
}

void DkRenderer::DrawStroke(DKNVGcontext *ctx, DKNVGcall *call)
{
    DKNVGpath* paths = &ctx->paths[call->pathOffset];
    int npaths = call->pathCount;

    if (ctx->flags & NVG_STENCIL_STROKES)
    {
        // Set the stencil to be used
        this->dynCmdBuf.setStencil(DkFace_Front, 0xFF, 0x0, 0xFF);

        // Configure for filling the stroke base without overlap
        auto depthStencilState = dk::DepthStencilState{}
            .setStencilTestEnable(true)
            .setStencilFrontCompareOp(DkCompareOp_Equal)
            .setStencilFrontFailOp(DkStencilOp_Keep)
            .setStencilFrontDepthFailOp(DkStencilOp_Keep)
            .setStencilFrontPassOp(DkStencilOp_Incr);
        this->dynCmdBuf.bindDepthStencilState(depthStencilState);
        this->SetUniforms(ctx, call->uniformOffset + ctx->fragSize, call->image);

        // Draw vertices
        for (int i = 0; i < npaths; i++)
        {
            this->dynCmdBuf.draw(DkPrimitive_TriangleStrip, paths[i].strokeCount, 1, paths[i].strokeOffset, 0);
        }

        // Configure for drawing anti-aliased pixels
        depthStencilState.setStencilFrontPassOp(DkStencilOp_Keep);
        this->dynCmdBuf.bindDepthStencilState(depthStencilState);
        this->SetUniforms(ctx, call->uniformOffset, call->image);

        // Draw vertices
        for (int i = 0; i < npaths; i++)
        {
            this->dynCmdBuf.draw(DkPrimitive_TriangleStrip, paths[i].strokeCount, 1, paths[i].strokeOffset, 0);
        }

        // Configure for clearing the stencil buffer
        depthStencilState
            .setStencilTestEnable(true)
            .setStencilFrontCompareOp(DkCompareOp_Always)
            .setStencilFrontFailOp(DkStencilOp_Zero)
            .setStencilFrontDepthFailOp(DkStencilOp_Zero)
            .setStencilFrontPassOp(DkStencilOp_Zero);

        // Draw vertices
        for (int i = 0; i < npaths; i++)
        {
            this->dynCmdBuf.draw(DkPrimitive_TriangleStrip, paths[i].strokeCount, 1, paths[i].strokeOffset, 0);
        }

        // Reset the depth stencil state to default
        this->dynCmdBuf.bindDepthStencilState(dk::DepthStencilState{});
    }
    else
    {
        this->SetUniforms(ctx, call->uniformOffset, call->image);

        // Draw vertices
        for (int i = 0; i < npaths; i++)
        {
            this->dynCmdBuf.draw(DkPrimitive_TriangleStrip, paths[i].strokeCount, 1, paths[i].strokeOffset, 0);
        }
    }
}

void DkRenderer::DrawTriangles(DKNVGcontext *ctx, DKNVGcall *call)
{
    this->SetUniforms(ctx, call->uniformOffset, call->image);
    this->dynCmdBuf.draw(DkPrimitive_Triangles, call->triangleCount, 1, call->triangleOffset, 0);
}

int DkRenderer::Create(DKNVGcontext *ctx)
{
    this->vertexShader.load(codeMemPool, "romfs:/shaders/fill_vsh.dksh");

    // Load the appropriate fragment shader depending on whether AA is enabled
    if (ctx->flags & NVG_ANTIALIAS)
    {
        this->fragmentShader.load(codeMemPool, "romfs:/shaders/fill_aa_fsh.dksh");
    }
    else
    {
        this->fragmentShader.load(codeMemPool, "romfs:/shaders/fill_fsh.dksh");
    }

    // Set the size of fragment uniforms
    ctx->fragSize = FragmentUniformSize;
    return 1;
}

std::shared_ptr<Texture> DkRenderer::FindTexture(int id)
{
    for (auto it = this->textures.begin(); it != this->textures.end(); it++)
    {
        if ((*it)->GetId() == id)
        {
            return *it;
        }
    }

    return nullptr;
}

int DkRenderer::CreateTexture(DKNVGcontext *ctx, int type, int w, int h, int imageFlags, const unsigned char* data)
{
    const auto textureId = this->nextTextureId++;
    auto texture = std::make_shared<Texture>(textureId);
    texture->Initialize(this->imageMemPool, this->dataMemPool, this->device, this->queue, type, w, h, imageFlags, data);
    this->textures.push_back(texture);
    return texture->GetId();
}

int DkRenderer::DeleteTexture(DKNVGcontext *ctx, int image)
{
    bool found = false;

    for (auto it = this->textures.begin(); it != this->textures.end();)
    {
        // Remove textures with the given id
        if ((*it)->GetId() == image)
        {
            it = this->textures.erase(it);
            found = true;
        }
        else
        {
            ++it;
        }
    }

    // Free any used image descriptors
    this->FreeImageDescriptor(image);
    return found;
}

int DkRenderer::UpdateTexture(DKNVGcontext *ctx, int image, int x, int y, int w, int h, const unsigned char *data)
{
    std::shared_ptr<Texture> texture = this->FindTexture(image);

    // Could not find a texture
    if (texture == nullptr)
    {
        return 0;
    }

    const DKNVGtextureDescriptor *texDesc = texture->GetDescriptor();

    if (texDesc->type == NVG_TEXTURE_RGBA)
    {
        data += y * texDesc->width*4;
    }
    else
    {
        data += y * texDesc->width;
    }
    x = 0;
    w = texDesc->width;

    UpdateImage(texture->GetImage(), this->dataMemPool, this->device, this->queue, texDesc->type, x, y, w, h, data);
    return 1;
}

int DkRenderer::GetTextureSize(DKNVGcontext *ctx, int image, int *w, int *h)
{
    auto descriptor = this->GetTextureDescriptor(ctx, image);

    if (descriptor == nullptr)
    {
        return 0;
    }

    *w = descriptor->width;
    *h = descriptor->height;
    return 1;
}

const DKNVGtextureDescriptor *DkRenderer::GetTextureDescriptor(DKNVGcontext* ctx, int id)
{
    for (auto it = this->textures.begin(); it != this->textures.end(); it++)
    {
        if ((*it)->GetId() == id)
        {
           return (*it)->GetDescriptor();
        }
    }

    return nullptr;
}

void DkRenderer::Flush(DKNVGcontext *ctx)
{
    int i;

    if (ctx->ncalls > 0)
    {
        // Prepare dynamic command buffer
        this->dynCmdMem.begin(this->dynCmdBuf);

        // Update buffers with data
        this->UpdateVertexBuffer(ctx->verts, ctx->nverts * sizeof(NVGvertex));

        // Enable blending
        this->dynCmdBuf.bindColorState(dk::ColorState{}.setBlendEnable(0, true));

        // Setup
        this->dynCmdBuf.bindShaders(DkStageFlag_GraphicsMask, { vertexShader, fragmentShader });
        this->dynCmdBuf.bindVtxAttribState(VertexAttribState);
        this->dynCmdBuf.bindVtxBufferState(VertexBufferState);
        this->dynCmdBuf.bindVtxBuffer(0, this->vertexBuffer->getGpuAddr(), this->vertexBuffer->getSize());

        // Push the view size to the uniform buffer and bind it
        const auto view = View{glm::vec2{this->viewWidth, this->viewHeight}};
        this->dynCmdBuf.pushConstants(this->viewUniformBuffer.getGpuAddr(), this->viewUniformBuffer.getSize(), 0, sizeof(view), &view);
        this->dynCmdBuf.bindUniformBuffer(DkStage_Vertex, 0, this->viewUniformBuffer.getGpuAddr(), this->viewUniformBuffer.getSize());

        // Iterate over calls
        for (i = 0; i < ctx->ncalls; i++)
        {
            DKNVGcall *call = &ctx->calls[i];

            // Perform blending
            this->dynCmdBuf.bindBlendStates(0, { dk::BlendState{}.setFactors(static_cast<DkBlendFactor>(call->blendFunc.srcRGB), static_cast<DkBlendFactor>(call->blendFunc.dstRGB), static_cast<DkBlendFactor>(call->blendFunc.srcAlpha), static_cast<DkBlendFactor>(call->blendFunc.dstRGB)) });

            if (call->type == DKNVG_FILL)
            {
                this->DrawFill(ctx, call);
            }
            else if (call->type == DKNVG_CONVEXFILL)
            {
                this->DrawConvexFill(ctx, call);
            }
            else if (call->type == DKNVG_STROKE)
            {
                this->DrawStroke(ctx, call);
            }
            else if (call->type == DKNVG_TRIANGLES)
            {
                this->DrawTriangles(ctx, call);
            }

            this->queue.submitCommands(this->dynCmdMem.end(this->dynCmdBuf));
            this->queue.waitIdle();
        }
    }

    // Reset calls
    ctx->nverts = 0;
    ctx->npaths = 0;
    ctx->ncalls = 0;
    ctx->nuniforms = 0;
}
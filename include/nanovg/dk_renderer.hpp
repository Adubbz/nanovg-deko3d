#pragma once

#include <deko3d.hpp>
#include <map>
#include <memory>
#include <vector>
#include "framework/CDescriptorSet.h"
#include "framework/CMemPool.h"
#include "framework/CShader.h"
#include "framework/CCmdMemRing.h"
#include "nanovg.h"

// Create flags
enum NVGcreateFlags
{
    // Flag indicating if geometry based anti-aliasing is used (may not be needed when using MSAA).
    NVG_ANTIALIAS 		= 1<<0,
    // Flag indicating if strokes should be drawn using stencil buffer. The rendering will be a little
    // slower, but path overlaps (i.e. self-intersecting or sharp turns) will be drawn just once.
    NVG_STENCIL_STROKES	= 1<<1,
    // Flag indicating that additional debug checks are done.
    NVG_DEBUG 			= 1<<2,
};

enum DKNVGuniformLoc
{
    DKNVG_LOC_VIEWSIZE,
    DKNVG_LOC_TEX,
    DKNVG_LOC_FRAG,
    DKNVG_MAX_LOCS
};

enum VKNVGshaderType
{
  NSVG_SHADER_FILLGRAD,
  NSVG_SHADER_FILLIMG,
  NSVG_SHADER_SIMPLE,
  NSVG_SHADER_IMG
};

struct DKNVGtextureDescriptor
{
    int width, height;
    int type;
    int flags;
};

struct DKNVGblend
{
    int srcRGB;
    int dstRGB;
    int srcAlpha;
    int dstAlpha;
};

enum DKNVGcallType
{
    DKNVG_NONE = 0,
    DKNVG_FILL,
    DKNVG_CONVEXFILL,
    DKNVG_STROKE,
    DKNVG_TRIANGLES,
};

struct DKNVGcall
{
    int type;
    int image;
    int pathOffset;
    int pathCount;
    int triangleOffset;
    int triangleCount;
    int uniformOffset;
    DKNVGblend blendFunc;
};

struct DKNVGpath
{
    int fillOffset;
    int fillCount;
    int strokeOffset;
    int strokeCount;
};

struct DKNVGfragUniforms
{
    float scissorMat[12]; // matrices are actually 3 vec4s
    float paintMat[12];
    struct NVGcolor innerCol;
    struct NVGcolor outerCol;
    float scissorExt[2];
    float scissorScale[2];
    float extent[2];
    float radius;
    float feather;
    float strokeMult;
    float strokeThr;
    int texType;
    int type;
};

class DkRenderer;

struct DKNVGcontext
{
    DkRenderer *renderer;
    float view[2];
    int fragSize;
    int flags;
    // Per frame buffers
    DKNVGcall* calls;
    int ccalls;
    int ncalls;
    DKNVGpath* paths;
    int cpaths;
    int npaths;
    struct NVGvertex* verts;
    int cverts;
    int nverts;
    unsigned char* uniforms;
    int cuniforms;
    int nuniforms;
};

class Texture
{
    private:
        int id;
        dk::Image image;
        dk::ImageDescriptor imageDescriptor;
        CMemPool::Handle imageMem;
        DKNVGtextureDescriptor textureDescriptor;
    public:
        Texture(int id);
        ~Texture();

        void Initialize(CMemPool &imagePool, CMemPool &scratchPool, dk::Device device, dk::Queue transferQueue, int type, int w, int h, int imageFlags, const unsigned char *data);
        void Update(CMemPool &imagePool, CMemPool &scratchPool, dk::Device device, dk::Queue transferQueue, int type, int w, int h, int imageFlags, const unsigned char *data);

        int GetId();
        const DKNVGtextureDescriptor *GetDescriptor();

        dk::Image &GetImage();
        dk::ImageDescriptor &GetImageDescriptor();
};

class DkRenderer
{
    private:
        enum SamplerType : uint8_t
        {
            SamplerType_MipFilter = 1 << 0,
            SamplerType_Nearest   = 1 << 1,
            SamplerType_RepeatX   = 1 << 2,
            SamplerType_RepeatY   = 1 << 3,
            SamplerType_Total     = 0x10,
        };

    private:
        static constexpr unsigned int NumFramebuffers = 2;
        static constexpr unsigned int DynamicCmdSize = 0x20000;
        static constexpr unsigned int FragmentUniformSize = sizeof(DKNVGfragUniforms) + 4 - sizeof(DKNVGfragUniforms) % 4;
        static constexpr unsigned int MaxImages = 0x1000;

        // From the application
        const unsigned int viewWidth;
        const unsigned int viewHeight;

        dk::Device device;
        dk::Queue queue;

        CMemPool &imageMemPool;
        CMemPool &codeMemPool;
        CMemPool &dataMemPool;

        // State
        dk::UniqueCmdBuf dynCmdBuf;
        CCmdMemRing<NumFramebuffers> dynCmdMem;

        std::optional<CMemPool::Handle> vertexBuffer;

        CShader vertexShader;
        CShader fragmentShader;
        CMemPool::Handle viewUniformBuffer;
        CMemPool::Handle fragUniformBuffer;

        int nextTextureId = 1;
        std::vector<std::shared_ptr<Texture>> textures;
        CDescriptorSet<MaxImages> imageDescriptorSet;
        CDescriptorSet<SamplerType_Total> samplerDescriptorSet;

        std::array<int, MaxImages> imageDescriptorMappings;
        int lastImageDescriptor = 0;

        int AcquireImageDescriptor(std::shared_ptr<Texture> texture, int image);
        void FreeImageDescriptor(int image);

        CMemPool::Handle CreateDataBuffer(const void *data, size_t size);
        void UpdateVertexBuffer(const void *data, size_t size);

        void SetUniforms(DKNVGcontext *ctx, int offset, int image);

        void DrawFill(DKNVGcontext *ctx, DKNVGcall *call);
        void DrawConvexFill(DKNVGcontext *ctx, DKNVGcall *call);
        void DrawStroke(DKNVGcontext *ctx, DKNVGcall *call);
        void DrawTriangles(DKNVGcontext *ctx, DKNVGcall *call);

        std::shared_ptr<Texture> FindTexture(int id);
    public:
        DkRenderer(unsigned int viewWidth, unsigned int viewHeight, dk::Device device, dk::Queue queue, CMemPool &imageMemPool, CMemPool &codeMemPool, CMemPool &dataMemPool);
        ~DkRenderer();

        int Create(DKNVGcontext *ctx);
        int CreateTexture(DKNVGcontext *ctx, int type, int w, int h, int imageFlags, const unsigned char *data);
        int DeleteTexture(DKNVGcontext *ctx, int image);
        int UpdateTexture(DKNVGcontext *ctx, int image, int x, int y, int w, int h, const unsigned char *data);
        int GetTextureSize(DKNVGcontext *ctx, int image, int *w, int *h);
        const DKNVGtextureDescriptor *GetTextureDescriptor(DKNVGcontext *ctx, int id);

        void Flush(DKNVGcontext *ctx);
};
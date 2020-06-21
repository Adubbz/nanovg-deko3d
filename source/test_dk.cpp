#include "framework/CApplication.h"
#include "framework/CMemPool.h"

// C++ standard library headers
#include <array>
#include <optional>

#include "nanovg.h"
#define NANOVG_DK_IMPLEMENTATION
#include "nanovg_dk.h"
#include "twili.h"

#include "debug.hpp"

#ifndef USE_OPENGL
namespace {

    void drawWidths(NVGcontext* vg, float x, float y, float width)
    {
        int i;

        nvgSave(vg);

        nvgStrokeColor(vg, nvgRGBA(0,0,0,255));

        for (i = 0; i < 20; i++) {
            float w = (i+0.5f)*0.1f;
            nvgStrokeWidth(vg, w);
            nvgBeginPath(vg);
            nvgMoveTo(vg, x,y);
            nvgLineTo(vg, x+width,y+width*0.3f);
            nvgStroke(vg);
            y += 10;
        }

        nvgRestore(vg);
    }

    void drawEditBoxBase(NVGcontext* vg, float x, float y, float w, float h)
    {
        NVGpaint bg;
        // Edit
        bg = nvgBoxGradient(vg, x+1,y+1+1.5f, w-2,h-2, 3,4, nvgRGBA(255,255,255,32), nvgRGBA(32,32,32,32));
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x+1,y+1, w-2,h-2, 4-1);
        nvgFillPaint(vg, bg);
        nvgFill(vg);

        nvgBeginPath(vg);
        nvgRoundedRect(vg, x+0.5f,y+0.5f, w-1,h-1, 4-0.5f);
        nvgStrokeColor(vg, nvgRGBA(0,0,0,48));
        nvgStroke(vg);
    }

    void drawColorwheel(NVGcontext* vg, float x, float y, float w, float h, float t)
    {
        int i;
        float r0, r1, ax,ay, bx,by, cx,cy, aeps, r;
        float hue = sinf(t * 0.12f);
        NVGpaint paint;

        nvgSave(vg);

    /*	nvgBeginPath(vg);
        nvgRect(vg, x,y,w,h);
        nvgFillColor(vg, nvgRGBA(255,0,0,128));
        nvgFill(vg);*/

        cx = x + w*0.5f;
        cy = y + h*0.5f;
        r1 = (w < h ? w : h) * 0.5f - 5.0f;
        r0 = r1 - 20.0f;
        aeps = 0.5f / r1;	// half a pixel arc length in radians (2pi cancels out).

        for (i = 0; i < 6; i++) {
            float a0 = (float)i / 6.0f * NVG_PI * 2.0f - aeps;
            float a1 = (float)(i+1.0f) / 6.0f * NVG_PI * 2.0f + aeps;
            nvgBeginPath(vg);
            nvgArc(vg, cx,cy, r0, a0, a1, NVG_CW);
            nvgArc(vg, cx,cy, r1, a1, a0, NVG_CCW);
            nvgClosePath(vg);
            ax = cx + cosf(a0) * (r0+r1)*0.5f;
            ay = cy + sinf(a0) * (r0+r1)*0.5f;
            bx = cx + cosf(a1) * (r0+r1)*0.5f;
            by = cy + sinf(a1) * (r0+r1)*0.5f;
            paint = nvgLinearGradient(vg, ax,ay, bx,by, nvgHSLA(a0/(NVG_PI*2),1.0f,0.55f,255), nvgHSLA(a1/(NVG_PI*2),1.0f,0.55f,255));
            nvgFillPaint(vg, paint);
            nvgFill(vg);
        }

        nvgBeginPath(vg);
        nvgCircle(vg, cx,cy, r0-0.5f);
        nvgCircle(vg, cx,cy, r1+0.5f);
        nvgStrokeColor(vg, nvgRGBA(0,0,0,64));
        nvgStrokeWidth(vg, 1.0f);
        nvgStroke(vg);

        // Selector
        nvgSave(vg);
        nvgTranslate(vg, cx,cy);
        nvgRotate(vg, hue*NVG_PI*2);

        // Marker on
        nvgStrokeWidth(vg, 2.0f);
        nvgBeginPath(vg);
        nvgRect(vg, r0-1,-3,r1-r0+2,6);
        nvgStrokeColor(vg, nvgRGBA(255,255,255,192));
        nvgStroke(vg);

        paint = nvgBoxGradient(vg, r0-3,-5,r1-r0+6,10, 2,4, nvgRGBA(0,0,0,128), nvgRGBA(0,0,0,0));
        nvgBeginPath(vg);
        nvgRect(vg, r0-2-10,-4-10,r1-r0+4+20,8+20);
        nvgRect(vg, r0-2,-4,r1-r0+4,8);
        nvgPathWinding(vg, NVG_HOLE);
        nvgFillPaint(vg, paint);
        nvgFill(vg);

        // Center triangle
        r = r0 - 6;
        ax = cosf(120.0f/180.0f*NVG_PI) * r;
        ay = sinf(120.0f/180.0f*NVG_PI) * r;
        bx = cosf(-120.0f/180.0f*NVG_PI) * r;
        by = sinf(-120.0f/180.0f*NVG_PI) * r;
        nvgBeginPath(vg);
        nvgMoveTo(vg, r,0);
        nvgLineTo(vg, ax,ay);
        nvgLineTo(vg, bx,by);
        nvgClosePath(vg);
        paint = nvgLinearGradient(vg, r,0, ax,ay, nvgHSLA(hue,1.0f,0.5f,255), nvgRGBA(255,255,255,255));
        nvgFillPaint(vg, paint);
        nvgFill(vg);
        paint = nvgLinearGradient(vg, (r+ax)*0.5f,(0+ay)*0.5f, bx,by, nvgRGBA(0,0,0,0), nvgRGBA(0,0,0,255));
        nvgFillPaint(vg, paint);
        nvgFill(vg);
        nvgStrokeColor(vg, nvgRGBA(0,0,0,64));
        nvgStroke(vg);

        // Select circle on triangle
        ax = cosf(120.0f/180.0f*NVG_PI) * r*0.3f;
        ay = sinf(120.0f/180.0f*NVG_PI) * r*0.4f;
        nvgStrokeWidth(vg, 2.0f);
        nvgBeginPath(vg);
        nvgCircle(vg, ax,ay,5);
        nvgStrokeColor(vg, nvgRGBA(255,255,255,192));
        nvgStroke(vg);

        paint = nvgRadialGradient(vg, ax,ay, 7,9, nvgRGBA(0,0,0,64), nvgRGBA(0,0,0,0));
        nvgBeginPath(vg);
        nvgRect(vg, ax-20,ay-20,40,40);
        nvgCircle(vg, ax,ay,7);
        nvgPathWinding(vg, NVG_HOLE);
        nvgFillPaint(vg, paint);
        nvgFill(vg);

        nvgRestore(vg);

        nvgRestore(vg);
    }

    struct DemoData {
        int fontNormal, fontBold, fontIcons, fontEmoji;
        int images[12];
    };
    typedef struct DemoData DemoData;

    int loadDemoData(NVGcontext* vg, DemoData* data)
    {
        int i;

        if (vg == NULL)
            return -1;

        for (i = 0; i < 12; i++) {
            char file[128];
            snprintf(file, 128, "romfs:/images/image%d.jpg", i+1);
            data->images[i] = nvgCreateImage(vg, file, 0);
            if (data->images[i] == 0) {
                printf("Could not load %s.\n", file);
                return -1;
            }
        }

        data->fontIcons = nvgCreateFont(vg, "icons", "romfs:/fonts/entypo.ttf");
        if (data->fontIcons == -1) {
            printf("Could not add font icons.\n");
            return -1;
        }
        data->fontNormal = nvgCreateFont(vg, "sans", "romfs:/fonts/Roboto-Regular.ttf");
        if (data->fontNormal == -1) {
            printf("Could not add font italic.\n");
            return -1;
        }
        data->fontBold = nvgCreateFont(vg, "sans-bold", "romfs:/fonts/Roboto-Bold.ttf");
        if (data->fontBold == -1) {
            printf("Could not add font bold.\n");
            return -1;
        }
        data->fontEmoji = nvgCreateFont(vg, "emoji", "romfs:/fonts/NotoEmoji-Regular.ttf");
        if (data->fontEmoji == -1) {
            printf("Could not add font emoji.\n");
            return -1;
        }
        nvgAddFallbackFontId(vg, data->fontNormal, data->fontEmoji);
        nvgAddFallbackFontId(vg, data->fontBold, data->fontEmoji);

        return 0;
    }

    void freeDemoData(NVGcontext* vg, DemoData* data)
    {
        int i;

        if (vg == NULL)
            return;

        for (i = 0; i < 12; i++)
            nvgDeleteImage(vg, data->images[i]);
    }

    static float clampf(float a, float mn, float mx) { return a < mn ? mn : (a > mx ? mx : a); }

    void drawParagraph(NVGcontext* vg, float x, float y, float width, float height, float mx, float my)
    {
        NVGtextRow rows[3];
        NVGglyphPosition glyphs[100];
        const char* text = "This is longer chunk of text.\n  \n  Would have used lorem ipsum but she    was busy jumping over the lazy dog with the fox and all the men who came to the aid of the party.🎉";
        const char* start;
        const char* end;
        int nrows, i, nglyphs, j, lnum = 0;
        float lineh;
        float caretx, px;
        float bounds[4];
        float a;
        const char* hoverText = "Hover your mouse over the text to see calculated caret position.";
        float gx,gy;
        int gutter = 0;
        const char* boxText = "Testing\nsome multiline\ntext.";
        NVG_NOTUSED(height);

        nvgSave(vg);

        nvgFontSize(vg, 15.0f);
        nvgFontFace(vg, "sans");
        nvgTextAlign(vg, NVG_ALIGN_LEFT|NVG_ALIGN_TOP);
        nvgTextMetrics(vg, NULL, NULL, &lineh);

        // The text break API can be used to fill a large buffer of rows,
        // or to iterate over the text just few lines (or just one) at a time.
        // The "next" variable of the last returned item tells where to continue.
        start = text;
        end = text + strlen(text);
        while ((nrows = nvgTextBreakLines(vg, start, end, width, rows, 3))) {
            for (i = 0; i < nrows; i++) {
                NVGtextRow* row = &rows[i];
                int hit = mx > x && mx < (x+width) && my >= y && my < (y+lineh);

                nvgBeginPath(vg);
                nvgFillColor(vg, nvgRGBA(255,255,255,hit?64:16));
                nvgRect(vg, x + row->minx, y, row->maxx - row->minx, lineh);
                nvgFill(vg);

                nvgFillColor(vg, nvgRGBA(255,255,255,255));
                nvgText(vg, x, y, row->start, row->end);

                if (hit) {
                    caretx = (mx < x+row->width/2) ? x : x+row->width;
                    px = x;
                    nglyphs = nvgTextGlyphPositions(vg, x, y, row->start, row->end, glyphs, 100);
                    for (j = 0; j < nglyphs; j++) {
                        float x0 = glyphs[j].x;
                        float x1 = (j+1 < nglyphs) ? glyphs[j+1].x : x+row->width;
                        float gx = x0 * 0.3f + x1 * 0.7f;
                        if (mx >= px && mx < gx)
                            caretx = glyphs[j].x;
                        px = gx;
                    }
                    nvgBeginPath(vg);
                    nvgFillColor(vg, nvgRGBA(255,192,0,255));
                    nvgRect(vg, caretx, y, 1, lineh);
                    nvgFill(vg);

                    gutter = lnum+1;
                    gx = x - 10;
                    gy = y + lineh/2;
                }
                lnum++;
                y += lineh;
            }
            // Keep going...
            start = rows[nrows-1].next;
        }

        if (gutter) {
            char txt[16];
            snprintf(txt, sizeof(txt), "%d", gutter);
            nvgFontSize(vg, 12.0f);
            nvgTextAlign(vg, NVG_ALIGN_RIGHT|NVG_ALIGN_MIDDLE);

            nvgTextBounds(vg, gx,gy, txt, NULL, bounds);

            nvgBeginPath(vg);
            nvgFillColor(vg, nvgRGBA(255,192,0,255));
            nvgRoundedRect(vg, (int)bounds[0]-4,(int)bounds[1]-2, (int)(bounds[2]-bounds[0])+8, (int)(bounds[3]-bounds[1])+4, ((int)(bounds[3]-bounds[1])+4)/2-1);
            nvgFill(vg);

            nvgFillColor(vg, nvgRGBA(32,32,32,255));
            nvgText(vg, gx,gy, txt, NULL);
        }

        y += 20.0f;

        nvgFontSize(vg, 11.0f);
        nvgTextAlign(vg, NVG_ALIGN_LEFT|NVG_ALIGN_TOP);
        nvgTextLineHeight(vg, 1.2f);

        nvgTextBoxBounds(vg, x,y, 150, hoverText, NULL, bounds);

        // Fade the tooltip out when close to it.
        gx = clampf(mx, bounds[0], bounds[2]) - mx;
        gy = clampf(my, bounds[1], bounds[3]) - my;
        a = sqrtf(gx*gx + gy*gy) / 30.0f;
        a = clampf(a, 0, 1);
        nvgGlobalAlpha(vg, a);

        nvgBeginPath(vg);
        nvgFillColor(vg, nvgRGBA(220,220,220,255));
        nvgRoundedRect(vg, bounds[0]-2,bounds[1]-2, (int)(bounds[2]-bounds[0])+4, (int)(bounds[3]-bounds[1])+4, 3);
        px = (int)((bounds[2]+bounds[0])/2);
        nvgMoveTo(vg, px,bounds[1] - 10);
        nvgLineTo(vg, px+7,bounds[1]+1);
        nvgLineTo(vg, px-7,bounds[1]+1);
        nvgFill(vg);

        nvgFillColor(vg, nvgRGBA(0,0,0,220));
        nvgTextBox(vg, x,y, 150, hoverText, NULL);

        nvgRestore(vg);
    }

    void drawThumbnails(NVGcontext* vg, float x, float y, float w, float h, const int* images, int nimages, float t)
    {
        float cornerRadius = 3.0f;
        NVGpaint shadowPaint, imgPaint, fadePaint;
        float ix,iy,iw,ih;
        float thumb = 60.0f;
        float arry = 30.5f;
        int imgw, imgh;
        float stackh = (nimages/2) * (thumb+10) + 10;
        int i;
        float u = (1+cosf(t*0.5f))*0.5f;
        float u2 = (1-cosf(t*0.2f))*0.5f;
        float scrollh, dv;

        nvgSave(vg);
    //	nvgClearState(vg);

        // Drop shadow
        shadowPaint = nvgBoxGradient(vg, x,y+4, w,h, cornerRadius*2, 20, nvgRGBA(0,0,0,128), nvgRGBA(0,0,0,0));
        nvgBeginPath(vg);
        nvgRect(vg, x-10,y-10, w+20,h+30);
        nvgRoundedRect(vg, x,y, w,h, cornerRadius);
        nvgPathWinding(vg, NVG_HOLE);
        nvgFillPaint(vg, shadowPaint);
        nvgFill(vg);

        // Window
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x,y, w,h, cornerRadius);
        nvgMoveTo(vg, x-10,y+arry);
        nvgLineTo(vg, x+1,y+arry-11);
        nvgLineTo(vg, x+1,y+arry+11);
        nvgFillColor(vg, nvgRGBA(200,200,200,255));
        nvgFill(vg);

        nvgSave(vg);
        nvgScissor(vg, x,y,w,h);
        nvgTranslate(vg, 0, -(stackh - h)*u);

        dv = 1.0f / (float)(nimages-1);

        for (i = 0; i < nimages; i++) {
            float tx, ty, v, a;
            tx = x+10;
            ty = y+10;
            tx += (i%2) * (thumb+10);
            ty += (i/2) * (thumb+10);
            nvgImageSize(vg, images[i], &imgw, &imgh);
            if (imgw < imgh) {
                iw = thumb;
                ih = iw * (float)imgh/(float)imgw;
                ix = 0;
                iy = -(ih-thumb)*0.5f;
            } else {
                ih = thumb;
                iw = ih * (float)imgw/(float)imgh;
                ix = -(iw-thumb)*0.5f;
                iy = 0;
            }

            v = i * dv;
            a = 1.0;

            imgPaint = nvgImagePattern(vg, tx+ix, ty+iy, iw,ih, 0.0f/180.0f*NVG_PI, images[i], a);
            nvgBeginPath(vg);
            nvgRoundedRect(vg, tx,ty, thumb,thumb, 5);
            nvgFillPaint(vg, imgPaint);
            nvgFill(vg);

            shadowPaint = nvgBoxGradient(vg, tx-1,ty, thumb+2,thumb+2, 5, 3, nvgRGBA(0,0,0,128), nvgRGBA(0,0,0,0));
            nvgBeginPath(vg);
            nvgRect(vg, tx-5,ty-5, thumb+10,thumb+10);
            nvgRoundedRect(vg, tx,ty, thumb,thumb, 6);
            nvgPathWinding(vg, NVG_HOLE);
            nvgFillPaint(vg, shadowPaint);
            nvgFill(vg);

            nvgBeginPath(vg);
            nvgRoundedRect(vg, tx+0.5f,ty+0.5f, thumb-1,thumb-1, 4-0.5f);
            nvgStrokeWidth(vg,1.0f);
            nvgStrokeColor(vg, nvgRGBA(255,255,255,192));
            nvgStroke(vg);
        }
        nvgRestore(vg);

        // Hide fades
        fadePaint = nvgLinearGradient(vg, x,y,x,y+6, nvgRGBA(200,200,200,255), nvgRGBA(200,200,200,0));
        nvgBeginPath(vg);
        nvgRect(vg, x+4,y,w-8,6);
        nvgFillPaint(vg, fadePaint);
        nvgFill(vg);

        fadePaint = nvgLinearGradient(vg, x,y+h,x,y+h-6, nvgRGBA(200,200,200,255), nvgRGBA(200,200,200,0));
        nvgBeginPath(vg);
        nvgRect(vg, x+4,y+h-6,w-8,6);
        nvgFillPaint(vg, fadePaint);
        nvgFill(vg);

        // Scroll bar
        shadowPaint = nvgBoxGradient(vg, x+w-12+1,y+4+1, 8,h-8, 3,4, nvgRGBA(0,0,0,32), nvgRGBA(0,0,0,92));
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x+w-12,y+4, 8,h-8, 3);
        nvgFillPaint(vg, shadowPaint);
    //	nvgFillColor(vg, nvgRGBA(255,0,0,128));
        nvgFill(vg);

        scrollh = (h/stackh) * (h-8);
        shadowPaint = nvgBoxGradient(vg, x+w-12-1,y+4+(h-8-scrollh)*u-1, 8,scrollh, 3,4, nvgRGBA(220,220,220,255), nvgRGBA(128,128,128,255));
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x+w-12+1,y+4+1 + (h-8-scrollh)*u, 8-2,scrollh-2, 2);
        nvgFillPaint(vg, shadowPaint);
    //	nvgFillColor(vg, nvgRGBA(0,0,0,128));
        nvgFill(vg);

        nvgRestore(vg);
    }

}

void OutputDkDebug(void* userData, const char* context, DkResult result, const char* message) 
{
    OutputDebugString("Context: %s\nResult: %d\nMessage: %s\n", context, result, message);
    
    if (result != DkResult_Success) {
        while (true) {
            ;
        }
    }
}

class DkTest final : public CApplication
{
    static constexpr unsigned NumFramebuffers = 2;
    static constexpr uint32_t FramebufferWidth = 1280;
    static constexpr uint32_t FramebufferHeight = 720;
    static constexpr unsigned StaticCmdSize = 0x1000;

    dk::UniqueDevice device;
    dk::UniqueQueue queue;

    std::optional<CMemPool> pool_images;
    std::optional<CMemPool> pool_code;
    std::optional<CMemPool> pool_data;

    dk::UniqueCmdBuf cmdbuf;

    CMemPool::Handle depthBuffer_mem;
    CMemPool::Handle framebuffers_mem[NumFramebuffers];

    dk::Image depthBuffer;
    dk::Image framebuffers[NumFramebuffers];
    DkCmdList framebuffer_cmdlists[NumFramebuffers];
    dk::UniqueSwapchain swapchain;

    DkCmdList render_cmdlist;

    std::optional<DkRenderer> renderer;
    NVGcontext* vg;

	DemoData data;

public:
    DkTest()
    {
        // Create the deko3d device
        device = dk::DeviceMaker{}.setCbDebug(OutputDkDebug).create();

        // Create the main queue
        queue = dk::QueueMaker{device}.setFlags(DkQueueFlags_Graphics).create();

        // Create the memory pools
        pool_images.emplace(device, DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image, 16*1024*1024);
        pool_code.emplace(device, DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Code, 128*1024);
        pool_data.emplace(device, DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached, 1*1024*1024);

        // Create the static command buffer and feed it freshly allocated memory
        cmdbuf = dk::CmdBufMaker{device}.create();
        CMemPool::Handle cmdmem = pool_data->allocate(StaticCmdSize);
        cmdbuf.addMemory(cmdmem.getMemBlock(), cmdmem.getOffset(), cmdmem.getSize());

        // Create the framebuffer resources
        createFramebufferResources();

        this->renderer.emplace(FramebufferWidth, FramebufferHeight, this->device, this->queue, *this->pool_images, *this->pool_code, *this->pool_data);
	    this->vg = nvgCreateDk(&*this->renderer, NVG_ANTIALIAS | NVG_STENCIL_STROKES);

		if (loadDemoData(vg, &this->data) == -1) {
			OutputDebugString("Failed to load demo data!\n");
		}
    }

    ~DkTest()
    {
        // Cleanup vg. This needs to be done first as it relies on the renderer.
        nvgDeleteDk(vg);

        // Destroy the renderer
        this->renderer.reset();

		freeDemoData(vg, &this->data);

        // Destroy the framebuffer resources
        destroyFramebufferResources();
    }

    void createFramebufferResources()
    {
      // Create layout for the depth buffer
        dk::ImageLayout layout_depthbuffer;
        dk::ImageLayoutMaker{device}
            .setFlags(DkImageFlags_UsageRender | DkImageFlags_HwCompression)
            .setFormat(DkImageFormat_S8)
            .setDimensions(FramebufferWidth, FramebufferHeight)
            .initialize(layout_depthbuffer);

        // Create the depth buffer
        depthBuffer_mem = pool_images->allocate(layout_depthbuffer.getSize(), layout_depthbuffer.getAlignment());
        depthBuffer.initialize(layout_depthbuffer, depthBuffer_mem.getMemBlock(), depthBuffer_mem.getOffset());

        // Create layout for the framebuffers
        dk::ImageLayout layout_framebuffer;
        dk::ImageLayoutMaker{device}
            .setFlags(DkImageFlags_UsageRender | DkImageFlags_UsagePresent | DkImageFlags_HwCompression)
            .setFormat(DkImageFormat_RGBA8_Unorm)
            .setDimensions(FramebufferWidth, FramebufferHeight)
            .initialize(layout_framebuffer);

        // Create the framebuffers
        std::array<DkImage const*, NumFramebuffers> fb_array;
        uint64_t fb_size  = layout_framebuffer.getSize();
        uint32_t fb_align = layout_framebuffer.getAlignment();
        for (unsigned i = 0; i < NumFramebuffers; i ++)
        {
            // Allocate a framebuffer
            framebuffers_mem[i] = pool_images->allocate(fb_size, fb_align);
            framebuffers[i].initialize(layout_framebuffer, framebuffers_mem[i].getMemBlock(), framebuffers_mem[i].getOffset());

            // Generate a command list that binds it
            dk::ImageView colorTarget{ framebuffers[i] }, depthTarget{ depthBuffer };
            cmdbuf.bindRenderTargets(&colorTarget, &depthTarget);
            framebuffer_cmdlists[i] = cmdbuf.finishList();

            // Fill in the array for use later by the swapchain creation code
            fb_array[i] = &framebuffers[i];
        }

        // Create the swapchain using the framebuffers
        swapchain = dk::SwapchainMaker{device, nwindowGetDefault(), fb_array}.create();

        // Generate the main rendering cmdlist
        recordStaticCommands();
    }

    void destroyFramebufferResources()
    {
        // Return early if we have nothing to destroy
        if (!swapchain) return;

        // Make sure the queue is idle before destroying anything
        queue.waitIdle();

        // Clear the static cmdbuf, destroying the static cmdlists in the process
        cmdbuf.clear();

        // Destroy the swapchain
        swapchain.destroy();

        // Destroy the framebuffers
        for (unsigned i = 0; i < NumFramebuffers; i ++)
            framebuffers_mem[i].destroy();

        // Destroy the depth buffer
        depthBuffer_mem.destroy();
    }

    void recordStaticCommands()
    {
        // Initialize state structs with deko3d defaults
        dk::RasterizerState rasterizerState;
        dk::ColorState colorState;
        dk::ColorWriteState colorWriteState;
        dk::BlendState blendState;

        // Calculate several measurements for the scene
        unsigned HalfWidth = FramebufferWidth/2, HalfHeight = FramebufferHeight/2;
        unsigned BoxSize = 400;
        unsigned BoxX = HalfWidth - BoxSize/2, BoxY = HalfHeight - BoxSize/2;
        unsigned TileWidth = BoxSize/5, TileHeight = BoxSize/4;

        // Configure the viewport and scissor
        cmdbuf.setViewports(0, { { 0.0f, 0.0f, FramebufferWidth, FramebufferHeight, 0.0f, 1.0f } });
        cmdbuf.setScissors(0, { { 0, 0, FramebufferWidth, FramebufferHeight } });

        // Clear the color and depth buffers
        cmdbuf.clearColor(0, DkColorMask_RGBA, 0.2f, 0.3f, 0.3f, 1.0f);
        cmdbuf.clearDepthStencil(true, 1.0f, 0xFF, 0);

        // Bind required state
        cmdbuf.bindRasterizerState(rasterizerState);
        cmdbuf.bindColorState(colorState);
        cmdbuf.bindColorWriteState(colorWriteState);

        render_cmdlist = cmdbuf.finishList();
    }

    void render()
    {
        // Acquire a framebuffer from the swapchain (and wait for it to be available)
        int slot = queue.acquireImage(swapchain);

        // Run the command list that attaches said framebuffer to the queue
        queue.submitCommands(framebuffer_cmdlists[slot]);

        // Run the main rendering command list
        queue.submitCommands(render_cmdlist);

		nvgBeginFrame(vg, FramebufferWidth, FramebufferHeight, 1.0f);
        {
            // Render stuff!
			drawWidths(vg, 50, 50, 100);
			drawEditBoxBase(vg, 100, 100, 200, 200);
            drawColorwheel(vg, 300, 300, 250.0f, 250.0f, 0.0f);
            drawParagraph(vg, 600, 50, 150, 100, 0, 0);
            drawThumbnails(vg, 800, 300, 160, 300, data.images, 12, 0);
        }
		nvgEndFrame(vg);

        // Now that we are done rendering, present it to the screen
        queue.presentImage(swapchain, slot);
    }

    bool onFrame(u64 ns) override
    {
        hidScanInput();
        u64 kDown = hidKeysDown(CONTROLLER_P1_AUTO);
        if (kDown & KEY_PLUS)
            return false;

        render();
        return true;
    }
};

// Main entrypoint
int main(int argc, char* argv[])
{
    if (R_FAILED(twiliInitialize())) {
        return 0;
    }
    if (R_FAILED(twiliBindStdio())) {
        return 0;
    }

	printf("Nanovg Deko3D test\n");

    DkTest app;
    app.run();

    twiliExit();
    return 0;
}
#endif
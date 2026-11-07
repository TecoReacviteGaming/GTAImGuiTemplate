#include "imgui_impl_renderware.h"

#include <aml-psdk/game_vc/plugin.h>
#include <aml-psdk/renderware/RwBase.h>
#include <aml-psdk/renderware/RwImage.h>
#include <aml-psdk/renderware/RwRaster.h>
#include <aml-psdk/renderware/RwRender.h>
#include <aml-psdk/gta_base/DrawVertices.h>
#include <aml-psdk/game_vc/engine/Sprite2d.h>

#include <new>
#include <cstring>
#include <algorithm>
#include <vector>

static_assert(sizeof(ImDrawIdx) == sizeof(RwImVertexIndex), "ImDrawIdx must be 16-bit to match RwImVertexIndex");
RwRaster*     g_FontRaster       = nullptr;
RwIm2DVertex* g_pVB              = nullptr;
unsigned int  g_VertexBufferSize = 0;
static std::vector<RwIm2DVertex>    g_Verts;
static std::vector<RwImVertexIndex> g_Idx;
struct ClipVert { float f[8]; };

// helper
static inline ClipVert MakeClipVert(const ImDrawVert& v)
{
    ClipVert c;
    c.f[0] = v.pos.x; c.f[1] = v.pos.y;
    c.f[2] = v.uv.x;  c.f[3] = v.uv.y;
    c.f[4] = (float)((v.col >>  0) & 0xFF);
    c.f[5] = (float)((v.col >>  8) & 0xFF);
    c.f[6] = (float)((v.col >> 16) & 0xFF);
    c.f[7] = (float)((v.col >> 24) & 0xFF);
    return c;
}
static inline unsigned int To8(float f)
{
    return (unsigned int)(f < 0.f ? 0.f : (f > 255.f ? 255.f : f + 0.5f));
}
static inline void PushVert(const ClipVert& c, float screenZ, float recipZ)
{
    RwIm2DVertex v;
    memset(&v, 0, sizeof(v));
    RwIm2DVertexSetScreenX(&v, c.f[0]);
    RwIm2DVertexSetScreenY(&v, c.f[1]);
    RwIm2DVertexSetScreenZ(&v, screenZ);
    RwIm2DVertexSetRecipCameraZ(&v, recipZ);
    RwIm2DVertexSetU(&v, c.f[2], recipZ);
    RwIm2DVertexSetV(&v, c.f[3], recipZ);
    v.emissiveColor = To8(c.f[4]) | (To8(c.f[5]) << 8) | (To8(c.f[6]) << 16) | (To8(c.f[7]) << 24);
    g_Idx.push_back((RwImVertexIndex)g_Verts.size());
    g_Verts.push_back(v);
}
static int ClipPoly(const ClipVert* in, int n, ClipVert* out, int axis, float bound, bool keepGreater)
{
    int m = 0;
    for (int i = 0; i < n; i++)
    {
        const ClipVert& a = in[i];
        const ClipVert& b = in[(i + 1) % n];
        float da = keepGreater ? a.f[axis] - bound : bound - a.f[axis];
        float db = keepGreater ? b.f[axis] - bound : bound - b.f[axis];
        bool aIn = da >= 0.f, bIn = db >= 0.f;
        if (aIn) out[m++] = a;
        if (aIn != bIn)
        {
            float t = da / (da - db);
            ClipVert c;
            for (int k = 0; k < 8; k++) c.f[k] = a.f[k] + (b.f[k] - a.f[k]) * t;
            out[m++] = c;
        }
    }
    return m;
}
static void FlushBatch()
{
    if (g_Verts.empty()) return;
    RwIm2DRenderIndexedPrimitive(rwPRIMTYPETRILIST,
        g_Verts.data(), (RwInt32)g_Verts.size(),
        g_Idx.data(),   (RwInt32)g_Idx.size());
    g_Verts.clear();
    g_Idx.clear();
}

// exposed funcs
bool ImGui_ImplRenderWare_Init()
{
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)RsGlobal->maximumWidth, (float)RsGlobal->maximumHeight);
    return true;
}
void ImGui_ImplRenderWare_ShutDown()
{
    ImGuiIO& io = ImGui::GetIO();
    if (g_FontRaster) RwRasterDestroy(g_FontRaster);
    g_FontRaster = nullptr;
    io.Fonts->TexID = nullptr;

    std::vector<RwIm2DVertex>().swap(g_Verts);
    std::vector<RwImVertexIndex>().swap(g_Idx);
}
void ImGui_ImplRenderWare_NewFrame()
{
    if (!g_FontRaster)
        ImGui_ImplRenderWare_CreateDeviceObjects();

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)RsGlobal->maximumWidth, (float)RsGlobal->maximumHeight);
}
void ImGui_ImplRenderWare_RenderDrawData(ImDrawData* draw_data)
{
    if (!draw_data || draw_data->CmdListsCount == 0 || draw_data->TotalVtxCount <= 0) return;

    const float screenZ = CSprite2d::NearScreenZ;
    const float recipZ  = CSprite2d::RecipNearClip;

    void* boundTex = (void*)-1;

    auto ResetState = [&]()
    {
        RwRenderStateSet(rwRENDERSTATEZTESTENABLE,            (void*)0);
        RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,           (void*)0);
        RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE,      (void*)1);
        RwRenderStateSet(rwRENDERSTATESRCBLEND,               (void*)rwBLENDSRCALPHA);
        RwRenderStateSet(rwRENDERSTATEDESTBLEND,              (void*)rwBLENDINVSRCALPHA);
        RwRenderStateSet(rwRENDERSTATEFOGENABLE,              (void*)0);
        RwRenderStateSet(rwRENDERSTATECULLMODE,               (void*)rwCULLMODECULLNONE);
        RwRenderStateSet(rwRENDERSTATEBORDERCOLOR,            (void*)0);
        RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION,      (void*)rwALPHATESTFUNCTIONGREATER);
        RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTIONREF,   (void*)2);
        RwRenderStateSet(rwRENDERSTATETEXTUREFILTER,          (void*)rwFILTERLINEAR);
        RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS,         (void*)rwTEXTUREADDRESSCLAMP);
        boundTex = (void*)-1;
    };
    ResetState();

    g_Verts.clear();
    g_Idx.clear();

    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];

        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];

            if (pcmd->UserCallback)
            {
                FlushBatch();
                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                    ResetState();
                else
                {
                    pcmd->UserCallback(cmd_list, pcmd);
                    ResetState();
                }
                continue;
            }

            const float cx0 = pcmd->ClipRect.x, cy0 = pcmd->ClipRect.y;
            const float cx1 = pcmd->ClipRect.z, cy1 = pcmd->ClipRect.w;
            if (pcmd->ElemCount == 0 || cx1 <= cx0 || cy1 <= cy0)
                continue;

            void* tex = (void*)pcmd->GetTexID();
            if (tex != boundTex)
            {
                FlushBatch();
                RwRenderStateSet(rwRENDERSTATETEXTURERASTER, tex);
                boundTex = tex;
            }

            const ImDrawVert* vtx = cmd_list->VtxBuffer.Data + pcmd->VtxOffset;
            const ImDrawIdx*  idx = cmd_list->IdxBuffer.Data + pcmd->IdxOffset;

            for (unsigned int i = 0; i + 2 < pcmd->ElemCount; i += 3)
            {
                if (g_Verts.size() > 65000) FlushBatch();

                const ImDrawVert& a = vtx[idx[i + 0]];
                const ImDrawVert& b = vtx[idx[i + 1]];
                const ImDrawVert& c = vtx[idx[i + 2]];

                float minX = std::min(a.pos.x, std::min(b.pos.x, c.pos.x));
                float maxX = std::max(a.pos.x, std::max(b.pos.x, c.pos.x));
                float minY = std::min(a.pos.y, std::min(b.pos.y, c.pos.y));
                float maxY = std::max(a.pos.y, std::max(b.pos.y, c.pos.y));
                if (maxX <= cx0 || minX >= cx1 || maxY <= cy0 || minY >= cy1) continue;

                ClipVert t0 = MakeClipVert(a), t1 = MakeClipVert(b), t2 = MakeClipVert(c);
                if (minX >= cx0 && maxX <= cx1 && minY >= cy0 && maxY <= cy1)
                {
                    PushVert(t0, screenZ, recipZ);
                    PushVert(t1, screenZ, recipZ);
                    PushVert(t2, screenZ, recipZ);
                    continue;
                }

                ClipVert bufA[16], bufB[16];
                bufA[0] = t0; bufA[1] = t1; bufA[2] = t2;
                int m = 3;
                m = ClipPoly(bufA, m, bufB, 0, cx0, true);  if (m < 3) continue;
                m = ClipPoly(bufB, m, bufA, 0, cx1, false); if (m < 3) continue;
                m = ClipPoly(bufA, m, bufB, 1, cy0, true);  if (m < 3) continue;
                m = ClipPoly(bufB, m, bufA, 1, cy1, false); if (m < 3) continue;

                for (int k = 1; k + 1 < m; k++)
                {
                    PushVert(bufA[0],     screenZ, recipZ);
                    PushVert(bufA[k],     screenZ, recipZ);
                    PushVert(bufA[k + 1], screenZ, recipZ);
                }
            }
        }
    }

    FlushBatch();
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, (void*)0);
}
bool ImGui_ImplRenderWare_CreateDeviceObjects()
{
    if (g_FontRaster) return true;

    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pxs;
    int width, height, bpp;
    io.Fonts->GetTexDataAsRGBA32(&pxs, &width, &height, &bpp);

    if (!pxs || width <= 0 || height <= 0 || bpp != 4) return false;
    RwImage* img = RwImageCreate(width, height, bpp * 8);
    if (!img) return false;

    if (!RwImageAllocatePixels(img) || !img->cpPixels)
    {
        RwImageDestroy(img);
        return false;
    }

    const size_t srcStride = (size_t)width * bpp;
    if ((size_t)img->stride == srcStride)
    {
        memcpy(img->cpPixels, pxs, srcStride * height);
    }
    else
    {
        const size_t rowBytes = std::min(srcStride, (size_t)img->stride);
        RwUInt8* dst = img->cpPixels;
        for (int y = 0; y < height; y++, dst += img->stride)
            memcpy(dst, pxs + srcStride * y, rowBytes);
    }

    RwInt32 w, h, d, flags;
    if (!RwImageFindRasterFormat(img, 4, &w, &h, &d, &flags))
    {
        RwImageDestroy(img);
        return false;
    }
    RwRaster* raster = RwRasterCreate(w, h, d, flags);
    if (!raster) { RwImageDestroy(img); return false; }

    RwRaster* result = RwRasterSetFromImage(raster, img);
    RwImageDestroy(img);
    if (!result) { RwRasterDestroy(raster); return false; }

    g_FontRaster = result;
    io.Fonts->TexID = (ImTextureID)g_FontRaster;
    return true;
}

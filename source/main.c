/* 
   TINY3D sample / (c) 2010 Hermes  <www.elotrolado.net>

*/

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <math.h>

#include <io/pad.h>

#include <psl1ght/lv2.h>
#include <psl1ght/lv2/spu.h>
#include <lv2/spu.h>
#include <sysmodule/sysmodule.h>
#include <pngdec/loadpng.h>

#include <tiny3d.h>

#include "spu_soundmodule.bin.h" // load SPU Module
#include "spu_soundlib.h"
#include "audioplayer.h"

#include "geohotlightsitup_mp3.bin.h"

typedef struct
{
    int w, h;
    int stride;
    u32 rsx_offset;
    u8 * bitmap;
    int color_format;
} Surface;

Surface surface1;

extern unsigned *video_text;

// draw one background color in virtual 2D coordinates

void DrawBackground2D(u32 rgba)
{
    tiny3d_SetPolygon(TINY3D_QUADS);

    tiny3d_VertexPos(0  , 0  , 65535);
    tiny3d_VertexColor(rgba);

    tiny3d_VertexPos(847, 0  , 65535);

    tiny3d_VertexPos(847, 511, 65535);

    tiny3d_VertexPos(0  , 511, 65535);
    tiny3d_End();
}


#define SURFACE_RGBA(r, g, b, a) (((a)<<24) | ((r)<<16) | ((g)<<8) | (b))

u8 * CreateSurface(Surface *surface, u8 *texture, int w, int h, text_format color_format)
{
    texture = (u8 *) ((((long) texture) + 15) & ~15);
    surface->rsx_offset   = tiny3d_TextureOffset(texture);
    surface->bitmap       = texture;
    surface->color_format = color_format;
    surface->w            = w;
    surface->h            = h;

    switch(color_format) {
    
        case TINY3D_TEX_FORMAT_L8: // this format is unsupported
        case TINY3D_TEX_FORMAT_A8R8G8B8:
            surface->color_format = TINY3D_TEX_FORMAT_A8R8G8B8; // force color
            surface->stride = w * 4;
            texture += surface->stride * surface->h;
            texture = (u8 *) ((((long) texture) + 15) & ~15);
            break;
       default:
            surface->color_format = TINY3D_TEX_FORMAT_A1R5G5B5; // force color
            surface->stride = w * 2;
            texture += surface->stride * surface->h;
            texture = (u8 *) ((((long) texture) + 15) & ~15);
            break;
    }

    return texture;
}

void DrawSurface(Surface *surface, float x, float y, float z, float w, float h, int smooth)
{
     // Load surface texture
    tiny3d_SetTextureWrap(0, surface->rsx_offset, surface->w, surface->h, surface->stride, surface->color_format, TEXTWRAP_CLAMP, TEXTWRAP_CLAMP, smooth);

    tiny3d_SetPolygon(TINY3D_QUADS);

    tiny3d_VertexPos(x    , y    , z);
    tiny3d_VertexColor(0xffffffff);
    tiny3d_VertexTexture(0.0f, 0.0f);

    tiny3d_VertexPos(x + w, y    , z);
    tiny3d_VertexTexture(0.999f, 0.0f);

    tiny3d_VertexPos(x + w, y + h, z);
    tiny3d_VertexTexture(0.999f, 0.999f);

    tiny3d_VertexPos(x    , y + h, z);
    tiny3d_VertexTexture(0.0f, 0.999f);

    tiny3d_End();
}

void drawScene()
{
    tiny3d_Clear(0xff000000, TINY3D_CLEAR_ALL);

    tiny3d_Project2D(); // change to 2D context (remember you it works with 848 x 512 as virtual coordinates)

    // fix Perspective Projection Matrix

    DrawBackground2D(0x00102fff) ; // light blue 

    DrawSurface(&surface1, (Video_aspect != 1) ? (848-682)/2 : 0, 0.0f, 1.0f, (Video_aspect != 1) ? 682 : 848, 512.0f, 1);

    /* DRAWING FINISH HERE */

    tiny3d_Flip();
    
        
}



void LoadTexture()
{

    u32 * texture_mem = tiny3d_AllocTexture(64*1024*1024); // alloc 64MB of space for textures (this pointer can be global)
    
    u32 * texture_pointer; // use to asign texture space without changes texture_mem

    if(!texture_mem) return; // fail!

    texture_pointer = texture_mem;

    // copy texture datas from PNG to the RSX memory allocated for textures

    // creates one surface of 320 x 200 and 32 bits
    texture_pointer = (u32 *) CreateSurface(&surface1, (u8 *) texture_pointer, 256, 192, TINY3D_TEX_FORMAT_A8R8G8B8);

    // clear surface 1
    memset (surface1.bitmap, 0, surface1.stride * surface1.h);
}

int inited = 0;

#define INITED_SPU          2
#define INITED_SOUNDLIB     4

// SPU
u32 spu = 0;
sysSpuImage spu_image;

#define SPU_SIZE(x) (((x)+127) & ~127)

void exiting()
{

    StopAudio();

    if(inited & INITED_SOUNDLIB)
        SND_End();

    if(inited & INITED_SPU) {
        sleep(1);
        lv2SpuRawDestroy(spu);
        sysSpuImageClose(&spu_image);
    }

    inited=0;
  
}

extern int pintor();

extern unsigned hiscore;

char hiscore_path[MAXPATHLEN];

s32 main(s32 argc, const char* argv[])
{
    u32 entry = 0;
    u32 segmentcount = 0;
    sysSpuSegment * segments;
	
	tiny3d_Init(1024*1024);

	ioPadInit(7);

    if(argc>0 && argv) {
    
            int n;

            strcpy(hiscore_path, argv[0]);

            n= 0;
            while(hiscore_path[n] != 0) n++;

            while(hiscore_path[n] != '/' && n > 1) n--;

            if(hiscore_path[n] == '/') {
                sprintf(&hiscore_path[n], "%s", "/hiscore.bin");
            } else strcpy(hiscore_path, "/dev_hdd0/temp/hiscore.bin");
    }
    
   // SysLoadModule(SYSMODULE_PNGDEC);

    atexit(exiting); // Tiny3D register the event 3 and do exit() call when you exit  to the menu

	// Load texture

    LoadTexture();

    
    lv2SpuInitialize(6, 5);

    lv2SpuRawCreate(&spu, NULL);

    sysSpuElfGetInformation(spu_soundmodule_bin, &entry, &segmentcount);

    size_t segmentsize = sizeof(sysSpuSegment) * segmentcount;
    segments = (sysSpuSegment*)memalign(128, SPU_SIZE(segmentsize)); // must be aligned to 128 or it break malloc() allocations
    memset(segments, 0, segmentsize);

    sysSpuElfGetSegments(spu_soundmodule_bin, segments, segmentcount);
    
    sysSpuImageImport(&spu_image, spu_soundmodule_bin, 0);
    
    sysSpuRawImageLoad(spu, &spu_image);

    inited |= INITED_SPU;

    if(SND_Init(spu)==0) inited |= INITED_SOUNDLIB;

    
    FILE *fp = fopen(hiscore_path, "r");
   
    if(fp) {
        fread(&hiscore, 1, 4, fp);
        fclose(fp);
    }

    SND_Pause(0);

    fp= (FILE *) mem_open( (char *) geohotlightsitup_mp3_bin, sizeof(geohotlightsitup_mp3_bin));

    PlayAudiofd(fp, 0, AUDIO_INFINITE_TIME);

    SetVolumeAudio(255);
    
    video_text = (unsigned *) surface1.bitmap;

    pintor();

    fp = fopen(hiscore_path, "w");
   
    if(fp) {
        fwrite(&hiscore, 1, 4, fp);
        fclose(fp);
    }
  	
	return 0;
}


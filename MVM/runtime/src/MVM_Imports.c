/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  MVM_Imports.c
 *           Module:  MVM_Runtime
 *           Target:  Portable C
 *      Description:  Built-in VM import binding layer with one static handler per imported symbol.
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  INCLUDES
 *********************************************************************************************************************/

#include "MVM_Internal.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/**********************************************************************************************************************
 *  LOCAL MACROS
 *********************************************************************************************************************/

#define MVM_IMPORT_PROTO(name) static bool name(VMGPContext *ctx)
#define MVM_IMPORT_IMPL(name)  MVM_IMPORT_PROTO(name)
#define MVM_BIND_IMPORT(symbol) { .name = #symbol, .handler = symbol }

#define HEAP_BLOCK_HEADER_SIZE (8u)
#define HEAP_BLOCK_FLAG_USED   (1u)
#define STREAM_WRITE_FLAG  0x0200u
#define STREAM_CREATE_FLAG 0x0800u
#define STREAM_TRUNC_FLAG  0x1000u
#define STREAM_DELETE_FLAG 0x4000u
#define MVM_KEY_UP_MASK          0x00000001u
#define MVM_KEY_DOWN_MASK        0x00000002u
#define MVM_KEY_LEFT_MASK        0x00000004u
#define MVM_KEY_RIGHT_MASK       0x00000008u
#define MVM_KEY_FIRE_MASK        0x00000010u
#define MVM_KEY_SELECT_MASK      0x00000020u
#define MVM_POINTER_DOWN_MASK    0x00000040u
#define MVM_POINTER_ALTDOWN_MASK 0x00000080u
#define MVM_KEY_FIRE2_MASK       0x00000100u
#define MVM_SE_OPTION_ASCII      217u

/**********************************************************************************************************************
 *  LOCAL DATA TYPES AND STRUCTURES
 *********************************************************************************************************************/

/**
 * @brief Defines one local import handler signature.
 */
typedef bool (*MVM_ImportHandler_t)(VMGPContext *ctx);

/**
 * @brief Binds one import name to one local handler.
 */
typedef struct MVM_ImportBinding_t
{
  const char *name;              /**< Imported symbol name visible to the guest. */
  MVM_ImportHandler_t handler;   /**< Built-in handler for the symbol. */
} MVM_ImportBinding_t;

/**
 * @brief Tracks bit-wise reads from a packed LZ stream.
 */
typedef struct LZBitStream
{
  const uint8_t *data;     /**< Optional memory-backed packed bit-stream buffer. */
  const VMGPContext *ctx;  /**< Optional source-backed VM context. */
  size_t file_offset;      /**< File offset of the packed bit stream. */
  uint32_t size;           /**< Buffer size in bytes. */
  uint32_t bit_pos;        /**< Current read position in bits. */
  uint32_t cached_index;   /**< Cached source byte index. */
  uint8_t cached_value;    /**< Cached source byte value. */
  bool cache_valid;        /**< Indicates whether one source byte is cached. */
} LZBitStream;

/**********************************************************************************************************************
 *  LOCAL FUNCTION PROTOTYPES
 *********************************************************************************************************************/

/**
 * @brief Looks up one built-in handler binding.
 */
static const MVM_ImportBinding_t *MVM_lFindImportBinding(const char *name);

/**
 * @brief Returns one active resource stream by handle.
 */
static VMGPStream *MVM_lFindStream(VMGPContext *ctx, uint32_t handle);

/**
 * @brief Allocates one free resource stream slot.
 */
static VMGPStream *MVM_lAllocStream(VMGPContext *ctx);

/**
 * @brief Reads one byte range from one open resource stream.
 */
static bool MVM_lReadStreamBytes(const VMGPContext *ctx, const VMGPStream *stream, uint32_t pos, void *dst, uint32_t size);

/**
 * @brief Logs one default zero-result platform stub.
 */
static bool MVM_lReadLzHeader(const uint8_t *p,
                              size_t remain,
                              uint8_t *extended_offset_bits,
                              uint8_t *max_offset_bits,
                              uint32_t *uncompressed_size,
                              uint32_t *compressed_size);

/**
 * @brief Checks whether one LZ bit stream still has readable data.
 */
static bool MVM_lLzBitsValid(const LZBitStream *bit_stream);

/**
 * @brief Reads one byte from one LZ bit stream.
 */
static bool MVM_lReadLzByte(LZBitStream *bit_stream, uint32_t byte_index, uint8_t *value);

/**
 * @brief Reads bits from one LZ bit stream.
 */
static uint32_t MVM_lReadLzBits(LZBitStream *bit_stream, uint32_t count);

/**
 * @brief Expands one packed LZ payload into guest memory.
 */
static uint32_t MVM_lDecompressLzContent(LZBitStream *bit_stream,
                                         uint8_t *dst,
                                         uint32_t dst_size,
                                         uint8_t extended_offset_bits,
                                         uint8_t max_offset_bits);
static uint32_t MVM_lUnicodeStrLen(const uint8_t *src, size_t max_bytes);
static int32_t MVM_lUnicodeStrCmp(const uint8_t *left, size_t left_bytes, const uint8_t *right, size_t right_bytes);
static int32_t MVM_lFixedFromDouble(double value);
static double MVM_lFixedToDouble(int32_t value);
static bool MVM_lReadSpriteHeader(const VMGPContext *ctx,
                                  uint32_t sprite_addr,
                                  uint16_t *width,
                                  uint16_t *height);
static bool MVM_lRectanglesOverlap(int32_t ax,
                                   int32_t ay,
                                   uint32_t aw,
                                   uint32_t ah,
                                   int32_t bx,
                                   int32_t by,
                                   uint32_t bw,
                                   uint32_t bh);
static bool MVM_lReadMapHeader(const VMGPContext *ctx, uint32_t header_addr, VMGPMapState *map_state);
static uint32_t MVM_lMapCellStride(const VMGPMapState *map_state);
static uint8_t *MVM_lMapCellPtr(const VMGPContext *ctx, const VMGPMapState *map_state, uint32_t x, uint32_t y);
static bool MVM_lHeapReadBlock(const VMGPContext *ctx, uint32_t block_addr, uint32_t *size, bool *used);
static void MVM_lHeapWriteBlock(VMGPContext *ctx, uint32_t block_addr, uint32_t size, bool used);
static void MVM_lHeapCompact(VMGPContext *ctx);
static bool MVM_lHeapFindBlockByPayload(const VMGPContext *ctx,
                                        uint32_t payload_addr,
                                        uint32_t *block_addr,
                                        uint32_t *size,
                                        bool *used);
static bool MVM_lHeapAlloc(VMGPContext *ctx, uint32_t size, uint32_t *payload_addr);
static bool MVM_lHeapFree(VMGPContext *ctx, uint32_t payload_addr);
static uint32_t MVM_lHeapMaxFreeBlock(VMGPContext *ctx);
static MVM_DrawCommand_t *MVM_lAllocDrawCommand(VMGPContext *ctx, MVM_DrawCommandType_t type);

/* Import handlers use SDK-visible names by design. */
MVM_IMPORT_PROTO(DbgPrintf);               /* Stub */
MVM_IMPORT_PROTO(vPrint);                  /* Stub */
MVM_IMPORT_PROTO(vClearScreen);            /* Partially implemented */
MVM_IMPORT_PROTO(vDrawLine);               /* Partially implemented */
MVM_IMPORT_PROTO(vDrawObject);             /* Partially implemented */
MVM_IMPORT_PROTO(vFillRect);               /* Partially implemented */
MVM_IMPORT_PROTO(vFlipScreen);             /* Partially implemented */
MVM_IMPORT_PROTO(vGetButtonData);          /* Partially implemented */
MVM_IMPORT_PROTO(vGetCaps);                /* Fully implemented */
MVM_IMPORT_PROTO(vGetPaletteEntry);        /* Partially implemented */
MVM_IMPORT_PROTO(vGetRandom);              /* Fully implemented */
MVM_IMPORT_PROTO(vGetTickCount);           /* Fully implemented */
MVM_IMPORT_PROTO(vMapDispose);             /* Partially implemented */
MVM_IMPORT_PROTO(vMapGetAttribute);        /* Partially implemented */
MVM_IMPORT_PROTO(vMapInit);                /* Partially implemented */
MVM_IMPORT_PROTO(vMapSetTile);             /* Partially implemented */
MVM_IMPORT_PROTO(vMapSetXY);               /* Partially implemented */
MVM_IMPORT_PROTO(vMemFree);                /* Fully implemented */
MVM_IMPORT_PROTO(vMsgBox);                 /* Partially implemented */
MVM_IMPORT_PROTO(vNewPtr);                 /* Fully implemented */
MVM_IMPORT_PROTO(vPlayResource);           /* Partially implemented */
MVM_IMPORT_PROTO(vSetActiveFont);          /* Partially implemented */
MVM_IMPORT_PROTO(vSetBackColor);           /* Partially implemented */
MVM_IMPORT_PROTO(vSetClipWindow);          /* Partially implemented */
MVM_IMPORT_PROTO(vSetForeColor);           /* Partially implemented */
MVM_IMPORT_PROTO(vSetPalette);             /* Partially implemented */
MVM_IMPORT_PROTO(vSetPaletteEntry);        /* Partially implemented */
MVM_IMPORT_PROTO(vSetRandom);              /* Fully implemented */
MVM_IMPORT_PROTO(vSetTransferMode);        /* Partially implemented */
MVM_IMPORT_PROTO(vSpriteBoxCollision);     /* Partially implemented */
MVM_IMPORT_PROTO(vSpriteCollision);        /* Partially implemented */
MVM_IMPORT_PROTO(vSpriteDispose);          /* Partially implemented */
MVM_IMPORT_PROTO(vSpriteInit);             /* Partially implemented */
MVM_IMPORT_PROTO(vSpriteSet);              /* Partially implemented */
MVM_IMPORT_PROTO(vStrCpy);                 /* Fully implemented */
MVM_IMPORT_PROTO(vStrLen);                 /* Fully implemented */
MVM_IMPORT_PROTO(vStreamClose);            /* Fully implemented */
MVM_IMPORT_PROTO(vStreamOpen);             /* Fully implemented */
MVM_IMPORT_PROTO(vStreamRead);             /* Fully implemented */
MVM_IMPORT_PROTO(vStreamSeek);             /* Fully implemented */
MVM_IMPORT_PROTO(vStreamWrite);            /* Partially implemented */
MVM_IMPORT_PROTO(vSysCtl);                 /* Partially implemented */
MVM_IMPORT_PROTO(vTerminateVMGP);          /* Fully implemented */
MVM_IMPORT_PROTO(vTestKey);                /* Partially implemented */
MVM_IMPORT_PROTO(vUpdateMap);              /* Partially implemented */
MVM_IMPORT_PROTO(vUpdateSprite);           /* Partially implemented */
MVM_IMPORT_PROTO(vitoa);                   /* Fully implemented */
MVM_IMPORT_PROTO(vDecompHdr);              /* Fully implemented */
MVM_IMPORT_PROTO(vDecompress);             /* Fully implemented */
MVM_IMPORT_PROTO(vDisposePtr);             /* Fully implemented */
MVM_IMPORT_PROTO(vBeep);                   /* Stub */
MVM_IMPORT_PROTO(vBoxInViewFrustum);       /* Stub */
MVM_IMPORT_PROTO(vCharExtent);             /* Stub */
MVM_IMPORT_PROTO(vCharExtentU);            /* Stub */
MVM_IMPORT_PROTO(vCheckDataCert);          /* Stub */
MVM_IMPORT_PROTO(vCheckDataCertFile);      /* Stub */
MVM_IMPORT_PROTO(vCheckIMEI);              /* Stub */
MVM_IMPORT_PROTO(vCheckNetwork);           /* Stub */
MVM_IMPORT_PROTO(vCollisionBoxBox);        /* Stub */
MVM_IMPORT_PROTO(vCollisionPointBox);      /* Stub */
MVM_IMPORT_PROTO(vCollisionVectorPlane);   /* Stub */
MVM_IMPORT_PROTO(vCollisionVectorPoly);    /* Stub */
MVM_IMPORT_PROTO(vCopyRect);               /* Stub */
MVM_IMPORT_PROTO(vCos);                    /* Fully implemented */
MVM_IMPORT_PROTO(vCreateGrayValue);        /* Stub */
MVM_IMPORT_PROTO(vCreatePlaneFromPoly);    /* Stub */
MVM_IMPORT_PROTO(vCreateTask);             /* Stub */
MVM_IMPORT_PROTO(vCreateTexture);          /* Stub */
MVM_IMPORT_PROTO(vCrossProduct);           /* Stub */
MVM_IMPORT_PROTO(vDeleteAllTextures);      /* Stub */
MVM_IMPORT_PROTO(vDeleteTexture);          /* Stub */
MVM_IMPORT_PROTO(vDisposeTask);            /* Stub */
MVM_IMPORT_PROTO(vDiv);                    /* Fully implemented */
MVM_IMPORT_PROTO(vDotProduct);             /* Stub */
MVM_IMPORT_PROTO(vDrawBillboard);          /* Stub */
MVM_IMPORT_PROTO(vDrawFlatPolygon);        /* Stub */
MVM_IMPORT_PROTO(vDrawPolygon);            /* Stub */
MVM_IMPORT_PROTO(vDrawTile);               /* Stub */
MVM_IMPORT_PROTO(vFileClose);              /* Fully implemented */
MVM_IMPORT_PROTO(vFileCreate);             /* Fully implemented */
MVM_IMPORT_PROTO(vFileDelete);             /* Fully implemented */
MVM_IMPORT_PROTO(vFileOpen);               /* Fully implemented */
MVM_IMPORT_PROTO(vFileRead);               /* Fully implemented */
MVM_IMPORT_PROTO(vFileSeek);               /* Fully implemented */
MVM_IMPORT_PROTO(vFileWrite);              /* Fully implemented */
MVM_IMPORT_PROTO(vFindRGBIndex);           /* Stub */
MVM_IMPORT_PROTO(vFrameTickCount);         /* Fully implemented */
MVM_IMPORT_PROTO(vFreeTexture);            /* Stub */
MVM_IMPORT_PROTO(vGetInteger);             /* Stub */
MVM_IMPORT_PROTO(vGetPixel);               /* Stub */
MVM_IMPORT_PROTO(vGetPointerPos);          /* Stub */
MVM_IMPORT_PROTO(vGetTime);                /* Stub */
MVM_IMPORT_PROTO(vGetTimeDate);            /* Fully implemented */
MVM_IMPORT_PROTO(vGetTimeDateUTC);         /* Fully implemented */
MVM_IMPORT_PROTO(vGetVMGPInfo);            /* Fully implemented */
MVM_IMPORT_PROTO(vGetZBufferValue);        /* Stub */
MVM_IMPORT_PROTO(vInit3D);                 /* Stub */
MVM_IMPORT_PROTO(vKillTask);               /* Stub */
MVM_IMPORT_PROTO(vLightPoint);             /* Stub */
MVM_IMPORT_PROTO(vMapGetTile);             /* Stub */
MVM_IMPORT_PROTO(vMapHeaderUpdate);        /* Stub */
MVM_IMPORT_PROTO(vMapSetAttribute);        /* Stub */
MVM_IMPORT_PROTO(vMatrixGetCurrent);       /* Stub */
MVM_IMPORT_PROTO(vMatrixIdentity);         /* Stub */
MVM_IMPORT_PROTO(vMatrixInvert);           /* Stub */
MVM_IMPORT_PROTO(vMatrixLookAt);           /* Stub */
MVM_IMPORT_PROTO(vMatrixMultiply);         /* Stub */
MVM_IMPORT_PROTO(vMatrixMultiply3x3);      /* Stub */
MVM_IMPORT_PROTO(vMatrixOrtho);            /* Stub */
MVM_IMPORT_PROTO(vMatrixPerspective);      /* Stub */
MVM_IMPORT_PROTO(vMatrixRotateVector);     /* Stub */
MVM_IMPORT_PROTO(vMatrixRotateX);          /* Stub */
MVM_IMPORT_PROTO(vMatrixRotateY);          /* Stub */
MVM_IMPORT_PROTO(vMatrixRotateZ);          /* Stub */
MVM_IMPORT_PROTO(vMatrixScale);            /* Stub */
MVM_IMPORT_PROTO(vMatrixSetCurrent);       /* Stub */
MVM_IMPORT_PROTO(vMatrixSetLight);         /* Stub */
MVM_IMPORT_PROTO(vMatrixSetProjection);    /* Stub */
MVM_IMPORT_PROTO(vMatrixTranslate);        /* Stub */
MVM_IMPORT_PROTO(vMatrixTranspose);        /* Stub */
MVM_IMPORT_PROTO(vMaxFreeBlock);           /* Fully implemented */
MVM_IMPORT_PROTO(vMsgBoxU);                /* Stub */
MVM_IMPORT_PROTO(vMul);                    /* Fully implemented */
MVM_IMPORT_PROTO(vNewPtrDbg);              /* Fully implemented */
MVM_IMPORT_PROTO(vPlot);                   /* Stub */
MVM_IMPORT_PROTO(vPow);                    /* Fully implemented */
MVM_IMPORT_PROTO(vReceive);                /* Stub */
MVM_IMPORT_PROTO(vReceiveAny);             /* Stub */
MVM_IMPORT_PROTO(vRenderPrimitive);        /* Stub */
MVM_IMPORT_PROTO(vRenderPrimitiveIndexed); /* Stub */
MVM_IMPORT_PROTO(vResClose);               /* Fully implemented */
MVM_IMPORT_PROTO(vResOpen);                /* Fully implemented */
MVM_IMPORT_PROTO(vResOpenMode);            /* Fully implemented */
MVM_IMPORT_PROTO(vResRead);                /* Fully implemented */
MVM_IMPORT_PROTO(vResSeek);                /* Fully implemented */
MVM_IMPORT_PROTO(vResWrite);               /* Stub */
MVM_IMPORT_PROTO(vResetLights);            /* Stub */
MVM_IMPORT_PROTO(vScanKeys);               /* Stub */
MVM_IMPORT_PROTO(vSelectFont);             /* Stub */
MVM_IMPORT_PROTO(vSend);                   /* Stub */
MVM_IMPORT_PROTO(vSetActiveTexture);       /* Stub */
MVM_IMPORT_PROTO(vSetAmbientLight);        /* Stub */
MVM_IMPORT_PROTO(vSetCameraPos);           /* Stub */
MVM_IMPORT_PROTO(vSetDisplayWindow);       /* Stub */
MVM_IMPORT_PROTO(vSetLight);               /* Stub */
MVM_IMPORT_PROTO(vSetMaterial);            /* Stub */
MVM_IMPORT_PROTO(vSetMaterial2);           /* Stub */
MVM_IMPORT_PROTO(vSetMatrixMode);          /* Stub */
MVM_IMPORT_PROTO(vSetOrientation);         /* Stub */
MVM_IMPORT_PROTO(vSetRenderState);         /* Stub */
MVM_IMPORT_PROTO(vSetStackSize);           /* Stub */
MVM_IMPORT_PROTO(vSetTexture);             /* Stub */
MVM_IMPORT_PROTO(vSetViewport);            /* Stub */
MVM_IMPORT_PROTO(vSetZBuffer);             /* Stub */
MVM_IMPORT_PROTO(vSin);                    /* Fully implemented */
MVM_IMPORT_PROTO(vSleep);                  /* Stub */
MVM_IMPORT_PROTO(vSoundCtrl);              /* Stub */
MVM_IMPORT_PROTO(vSoundCtrlEx);            /* Stub */
MVM_IMPORT_PROTO(vSoundDispose);           /* Stub */
MVM_IMPORT_PROTO(vSoundDisposeHandle);     /* Stub */
MVM_IMPORT_PROTO(vSoundGetHandle);         /* Stub */
MVM_IMPORT_PROTO(vSoundGetStatus);         /* Stub */
MVM_IMPORT_PROTO(vSoundInit);              /* Stub */
MVM_IMPORT_PROTO(vSoundLoad);              /* Stub */
MVM_IMPORT_PROTO(vSoundLoadFile);          /* Stub */
MVM_IMPORT_PROTO(vSoundLoadResource);      /* Stub */
MVM_IMPORT_PROTO(vSoundLoadStream);        /* Stub */
MVM_IMPORT_PROTO(vSoundPause);             /* Stub */
MVM_IMPORT_PROTO(vSoundPlay);              /* Stub */
MVM_IMPORT_PROTO(vSoundResume);            /* Stub */
MVM_IMPORT_PROTO(vSoundSetFrequency);      /* Stub */
MVM_IMPORT_PROTO(vSoundSetMasterVolume);   /* Stub */
MVM_IMPORT_PROTO(vSoundSetMixerChannels);  /* Stub */
MVM_IMPORT_PROTO(vSoundSetPan);            /* Stub */
MVM_IMPORT_PROTO(vSoundSetParameters);     /* Stub */
MVM_IMPORT_PROTO(vSoundSetPosition);       /* Stub */
MVM_IMPORT_PROTO(vSoundSetPriority);       /* Stub */
MVM_IMPORT_PROTO(vSoundSetVolume);         /* Stub */
MVM_IMPORT_PROTO(vSoundStop);              /* Stub */
MVM_IMPORT_PROTO(vSoundStopLooping);       /* Stub */
MVM_IMPORT_PROTO(vSoundUpload);            /* Stub */
MVM_IMPORT_PROTO(vSprintf);                /* Stub */
MVM_IMPORT_PROTO(vSprintfVa);              /* Stub */
MVM_IMPORT_PROTO(vSpriteClear);            /* Stub */
MVM_IMPORT_PROTO(vSqrt);                   /* Fully implemented */
MVM_IMPORT_PROTO(vStrCat);                 /* Fully implemented */
MVM_IMPORT_PROTO(vStrCatU);                /* Fully implemented */
MVM_IMPORT_PROTO(vStrCmp);                 /* Fully implemented */
MVM_IMPORT_PROTO(vStrCmpU);                /* Fully implemented */
MVM_IMPORT_PROTO(vStrCpyU);                /* Fully implemented */
MVM_IMPORT_PROTO(vStrLenU);                /* Fully implemented */
MVM_IMPORT_PROTO(vStrToU);                 /* Fully implemented */
MVM_IMPORT_PROTO(vStreamAccept);           /* Stub */
MVM_IMPORT_PROTO(vStreamConnect);          /* Stub */
MVM_IMPORT_PROTO(vStreamFrom);             /* Fully implemented */
MVM_IMPORT_PROTO(vStreamMode);             /* Fully implemented */
MVM_IMPORT_PROTO(vStreamReady);            /* Fully implemented */
MVM_IMPORT_PROTO(vStreamTo);               /* Fully implemented */
MVM_IMPORT_PROTO(vSwap);                   /* Fully implemented */
MVM_IMPORT_PROTO(vSwap16);                 /* Fully implemented */
MVM_IMPORT_PROTO(vSwap32);                 /* Fully implemented */
MVM_IMPORT_PROTO(vTan);                    /* Fully implemented */
MVM_IMPORT_PROTO(vTaskAlive);              /* Stub */
MVM_IMPORT_PROTO(vTextExtent);             /* Stub */
MVM_IMPORT_PROTO(vTextExtentU);            /* Stub */
MVM_IMPORT_PROTO(vTextOut);                /* Stub */
MVM_IMPORT_PROTO(vTextOutU);               /* Stub */
MVM_IMPORT_PROTO(vThisTask);               /* Stub */
MVM_IMPORT_PROTO(vUID);                    /* Fully implemented */
MVM_IMPORT_PROTO(vUpdateSpriteMap);        /* Partially implemented */
MVM_IMPORT_PROTO(vVectorAdd);              /* Stub */
MVM_IMPORT_PROTO(vVectorArrayAdd);         /* Stub */
MVM_IMPORT_PROTO(vVectorArrayDelta);       /* Stub */
MVM_IMPORT_PROTO(vVectorMul);              /* Stub */
MVM_IMPORT_PROTO(vVectorNormalize);        /* Stub */
MVM_IMPORT_PROTO(vVectorProjectV3);        /* Stub */
MVM_IMPORT_PROTO(vVectorProjectV4);        /* Stub */
MVM_IMPORT_PROTO(vVectorSub);              /* Stub */
MVM_IMPORT_PROTO(vVectorTransformV3);      /* Stub */
MVM_IMPORT_PROTO(vVectorTransformV4);      /* Stub */
MVM_IMPORT_PROTO(vWaitVBL);                /* Stub */
MVM_IMPORT_PROTO(vYieldToSystem);          /* Stub */

/**********************************************************************************************************************
 *  LOCAL DATA
 *********************************************************************************************************************/

static const MVM_ImportBinding_t MVM_lImportBindings[] =
{
  MVM_BIND_IMPORT(DbgPrintf),
  MVM_BIND_IMPORT(vPrint),
  MVM_BIND_IMPORT(vClearScreen),
  MVM_BIND_IMPORT(vDrawLine),
  MVM_BIND_IMPORT(vDrawObject),
  MVM_BIND_IMPORT(vFillRect),
  MVM_BIND_IMPORT(vFlipScreen),
  MVM_BIND_IMPORT(vGetButtonData),
  MVM_BIND_IMPORT(vGetCaps),
  MVM_BIND_IMPORT(vGetPaletteEntry),
  MVM_BIND_IMPORT(vGetRandom),
  MVM_BIND_IMPORT(vGetTickCount),
  MVM_BIND_IMPORT(vMapDispose),
  MVM_BIND_IMPORT(vMapGetAttribute),
  MVM_BIND_IMPORT(vMapInit),
  MVM_BIND_IMPORT(vMapSetTile),
  MVM_BIND_IMPORT(vMapSetXY),
  MVM_BIND_IMPORT(vMemFree),
  MVM_BIND_IMPORT(vMsgBox),
  MVM_BIND_IMPORT(vNewPtr),
  MVM_BIND_IMPORT(vPlayResource),
  MVM_BIND_IMPORT(vSetActiveFont),
  MVM_BIND_IMPORT(vSetBackColor),
  MVM_BIND_IMPORT(vSetClipWindow),
  MVM_BIND_IMPORT(vSetForeColor),
  MVM_BIND_IMPORT(vSetPalette),
  MVM_BIND_IMPORT(vSetPaletteEntry),
  MVM_BIND_IMPORT(vSetRandom),
  MVM_BIND_IMPORT(vSetTransferMode),
  MVM_BIND_IMPORT(vSpriteBoxCollision),
  MVM_BIND_IMPORT(vSpriteCollision),
  MVM_BIND_IMPORT(vSpriteDispose),
  MVM_BIND_IMPORT(vSpriteInit),
  MVM_BIND_IMPORT(vSpriteSet),
  MVM_BIND_IMPORT(vStrCpy),
  MVM_BIND_IMPORT(vStrLen),
  MVM_BIND_IMPORT(vStreamClose),
  MVM_BIND_IMPORT(vStreamOpen),
  MVM_BIND_IMPORT(vStreamRead),
  MVM_BIND_IMPORT(vStreamSeek),
  MVM_BIND_IMPORT(vStreamWrite),
  MVM_BIND_IMPORT(vSysCtl),
  MVM_BIND_IMPORT(vTerminateVMGP),
  MVM_BIND_IMPORT(vTestKey),
  MVM_BIND_IMPORT(vUpdateMap),
  MVM_BIND_IMPORT(vUpdateSprite),
  MVM_BIND_IMPORT(vitoa),
  MVM_BIND_IMPORT(vDecompHdr),
  MVM_BIND_IMPORT(vDecompress),
  MVM_BIND_IMPORT(vDisposePtr),
  MVM_BIND_IMPORT(vBeep),
  MVM_BIND_IMPORT(vBoxInViewFrustum),
  MVM_BIND_IMPORT(vCharExtent),
  MVM_BIND_IMPORT(vCharExtentU),
  MVM_BIND_IMPORT(vCheckDataCert),
  MVM_BIND_IMPORT(vCheckDataCertFile),
  MVM_BIND_IMPORT(vCheckIMEI),
  MVM_BIND_IMPORT(vCheckNetwork),
  MVM_BIND_IMPORT(vCollisionBoxBox),
  MVM_BIND_IMPORT(vCollisionPointBox),
  MVM_BIND_IMPORT(vCollisionVectorPlane),
  MVM_BIND_IMPORT(vCollisionVectorPoly),
  MVM_BIND_IMPORT(vCopyRect),
  MVM_BIND_IMPORT(vCos),
  MVM_BIND_IMPORT(vCreateGrayValue),
  MVM_BIND_IMPORT(vCreatePlaneFromPoly),
  MVM_BIND_IMPORT(vCreateTask),
  MVM_BIND_IMPORT(vCreateTexture),
  MVM_BIND_IMPORT(vCrossProduct),
  MVM_BIND_IMPORT(vDeleteAllTextures),
  MVM_BIND_IMPORT(vDeleteTexture),
  MVM_BIND_IMPORT(vDisposeTask),
  MVM_BIND_IMPORT(vDiv),
  MVM_BIND_IMPORT(vDotProduct),
  MVM_BIND_IMPORT(vDrawBillboard),
  MVM_BIND_IMPORT(vDrawFlatPolygon),
  MVM_BIND_IMPORT(vDrawPolygon),
  MVM_BIND_IMPORT(vDrawTile),
  MVM_BIND_IMPORT(vFileClose),
  MVM_BIND_IMPORT(vFileCreate),
  MVM_BIND_IMPORT(vFileDelete),
  MVM_BIND_IMPORT(vFileOpen),
  MVM_BIND_IMPORT(vFileRead),
  MVM_BIND_IMPORT(vFileSeek),
  MVM_BIND_IMPORT(vFileWrite),
  MVM_BIND_IMPORT(vFindRGBIndex),
  MVM_BIND_IMPORT(vFrameTickCount),
  MVM_BIND_IMPORT(vFreeTexture),
  MVM_BIND_IMPORT(vGetInteger),
  MVM_BIND_IMPORT(vGetPixel),
  MVM_BIND_IMPORT(vGetPointerPos),
  MVM_BIND_IMPORT(vGetTime),
  MVM_BIND_IMPORT(vGetTimeDate),
  MVM_BIND_IMPORT(vGetTimeDateUTC),
  MVM_BIND_IMPORT(vGetVMGPInfo),
  MVM_BIND_IMPORT(vGetZBufferValue),
  MVM_BIND_IMPORT(vInit3D),
  MVM_BIND_IMPORT(vKillTask),
  MVM_BIND_IMPORT(vLightPoint),
  MVM_BIND_IMPORT(vMapGetTile),
  MVM_BIND_IMPORT(vMapHeaderUpdate),
  MVM_BIND_IMPORT(vMapSetAttribute),
  MVM_BIND_IMPORT(vMatrixGetCurrent),
  MVM_BIND_IMPORT(vMatrixIdentity),
  MVM_BIND_IMPORT(vMatrixInvert),
  MVM_BIND_IMPORT(vMatrixLookAt),
  MVM_BIND_IMPORT(vMatrixMultiply),
  MVM_BIND_IMPORT(vMatrixMultiply3x3),
  MVM_BIND_IMPORT(vMatrixOrtho),
  MVM_BIND_IMPORT(vMatrixPerspective),
  MVM_BIND_IMPORT(vMatrixRotateVector),
  MVM_BIND_IMPORT(vMatrixRotateX),
  MVM_BIND_IMPORT(vMatrixRotateY),
  MVM_BIND_IMPORT(vMatrixRotateZ),
  MVM_BIND_IMPORT(vMatrixScale),
  MVM_BIND_IMPORT(vMatrixSetCurrent),
  MVM_BIND_IMPORT(vMatrixSetLight),
  MVM_BIND_IMPORT(vMatrixSetProjection),
  MVM_BIND_IMPORT(vMatrixTranslate),
  MVM_BIND_IMPORT(vMatrixTranspose),
  MVM_BIND_IMPORT(vMaxFreeBlock),
  MVM_BIND_IMPORT(vMsgBoxU),
  MVM_BIND_IMPORT(vMul),
  MVM_BIND_IMPORT(vNewPtrDbg),
  MVM_BIND_IMPORT(vPlot),
  MVM_BIND_IMPORT(vPow),
  MVM_BIND_IMPORT(vReceive),
  MVM_BIND_IMPORT(vReceiveAny),
  MVM_BIND_IMPORT(vRenderPrimitive),
  MVM_BIND_IMPORT(vRenderPrimitiveIndexed),
  MVM_BIND_IMPORT(vResClose),
  MVM_BIND_IMPORT(vResOpen),
  MVM_BIND_IMPORT(vResOpenMode),
  MVM_BIND_IMPORT(vResRead),
  MVM_BIND_IMPORT(vResSeek),
  MVM_BIND_IMPORT(vResWrite),
  MVM_BIND_IMPORT(vResetLights),
  MVM_BIND_IMPORT(vScanKeys),
  MVM_BIND_IMPORT(vSelectFont),
  MVM_BIND_IMPORT(vSend),
  MVM_BIND_IMPORT(vSetActiveTexture),
  MVM_BIND_IMPORT(vSetAmbientLight),
  MVM_BIND_IMPORT(vSetCameraPos),
  MVM_BIND_IMPORT(vSetDisplayWindow),
  MVM_BIND_IMPORT(vSetLight),
  MVM_BIND_IMPORT(vSetMaterial),
  MVM_BIND_IMPORT(vSetMaterial2),
  MVM_BIND_IMPORT(vSetMatrixMode),
  MVM_BIND_IMPORT(vSetOrientation),
  MVM_BIND_IMPORT(vSetRenderState),
  MVM_BIND_IMPORT(vSetStackSize),
  MVM_BIND_IMPORT(vSetTexture),
  MVM_BIND_IMPORT(vSetViewport),
  MVM_BIND_IMPORT(vSetZBuffer),
  MVM_BIND_IMPORT(vSin),
  MVM_BIND_IMPORT(vSleep),
  MVM_BIND_IMPORT(vSoundCtrl),
  MVM_BIND_IMPORT(vSoundCtrlEx),
  MVM_BIND_IMPORT(vSoundDispose),
  MVM_BIND_IMPORT(vSoundDisposeHandle),
  MVM_BIND_IMPORT(vSoundGetHandle),
  MVM_BIND_IMPORT(vSoundGetStatus),
  MVM_BIND_IMPORT(vSoundInit),
  MVM_BIND_IMPORT(vSoundLoad),
  MVM_BIND_IMPORT(vSoundLoadFile),
  MVM_BIND_IMPORT(vSoundLoadResource),
  MVM_BIND_IMPORT(vSoundLoadStream),
  MVM_BIND_IMPORT(vSoundPause),
  MVM_BIND_IMPORT(vSoundPlay),
  MVM_BIND_IMPORT(vSoundResume),
  MVM_BIND_IMPORT(vSoundSetFrequency),
  MVM_BIND_IMPORT(vSoundSetMasterVolume),
  MVM_BIND_IMPORT(vSoundSetMixerChannels),
  MVM_BIND_IMPORT(vSoundSetPan),
  MVM_BIND_IMPORT(vSoundSetParameters),
  MVM_BIND_IMPORT(vSoundSetPosition),
  MVM_BIND_IMPORT(vSoundSetPriority),
  MVM_BIND_IMPORT(vSoundSetVolume),
  MVM_BIND_IMPORT(vSoundStop),
  MVM_BIND_IMPORT(vSoundStopLooping),
  MVM_BIND_IMPORT(vSoundUpload),
  MVM_BIND_IMPORT(vSprintf),
  MVM_BIND_IMPORT(vSprintfVa),
  MVM_BIND_IMPORT(vSpriteClear),
  MVM_BIND_IMPORT(vSqrt),
  MVM_BIND_IMPORT(vStrCat),
  MVM_BIND_IMPORT(vStrCatU),
  MVM_BIND_IMPORT(vStrCmp),
  MVM_BIND_IMPORT(vStrCmpU),
  MVM_BIND_IMPORT(vStrCpyU),
  MVM_BIND_IMPORT(vStrLenU),
  MVM_BIND_IMPORT(vStrToU),
  MVM_BIND_IMPORT(vStreamAccept),
  MVM_BIND_IMPORT(vStreamConnect),
  MVM_BIND_IMPORT(vStreamFrom),
  MVM_BIND_IMPORT(vStreamMode),
  MVM_BIND_IMPORT(vStreamReady),
  MVM_BIND_IMPORT(vStreamTo),
  MVM_BIND_IMPORT(vSwap),
  MVM_BIND_IMPORT(vSwap16),
  MVM_BIND_IMPORT(vSwap32),
  MVM_BIND_IMPORT(vTan),
  MVM_BIND_IMPORT(vTaskAlive),
  MVM_BIND_IMPORT(vTextExtent),
  MVM_BIND_IMPORT(vTextExtentU),
  MVM_BIND_IMPORT(vTextOut),
  MVM_BIND_IMPORT(vTextOutU),
  MVM_BIND_IMPORT(vThisTask),
  MVM_BIND_IMPORT(vUID),
  MVM_BIND_IMPORT(vUpdateSpriteMap),
  MVM_BIND_IMPORT(vVectorAdd),
  MVM_BIND_IMPORT(vVectorArrayAdd),
  MVM_BIND_IMPORT(vVectorArrayDelta),
  MVM_BIND_IMPORT(vVectorMul),
  MVM_BIND_IMPORT(vVectorNormalize),
  MVM_BIND_IMPORT(vVectorProjectV3),
  MVM_BIND_IMPORT(vVectorProjectV4),
  MVM_BIND_IMPORT(vVectorSub),
  MVM_BIND_IMPORT(vVectorTransformV3),
  MVM_BIND_IMPORT(vVectorTransformV4),
  MVM_BIND_IMPORT(vWaitVBL),
  MVM_BIND_IMPORT(vYieldToSystem),
};

/**********************************************************************************************************************
 *  LOCAL MACROS
 *********************************************************************************************************************/

#define MVM_DEFINE_ZERO_STUB(name)                                      \
  MVM_IMPORT_IMPL(name)                                                 \
  {                                                                     \
    if (!ctx)                                                           \
    {                                                                   \
      return false;                                                     \
    }                                                                   \
                                                                        \
    ctx->regs[VM_REG_R0] = 0u;                                          \
                                                                        \
    MVM_LOG_D(ctx,                                                      \
              "platform-import",                                        \
              "%s(p0=%08X p1=%08X p2=%08X p3=%08X) -> 0\n",             \
              #name,                                                    \
              ctx->regs[VM_REG_P0],                                     \
              ctx->regs[VM_REG_P1],                                     \
              ctx->regs[VM_REG_P2],                                     \
              ctx->regs[VM_REG_P3]);                                    \
                                                                        \
    return true;                                                        \
  }

/**********************************************************************************************************************
 *  GLOBAL FUNCTIONS
 *********************************************************************************************************************/

bool MVM_HandleImport(MpnVM_t *vm, const char *name)
{
  VMGPContext *ctx;
  const MVM_ImportBinding_t *binding;

  ctx = (VMGPContext *)vm;
  binding = NULL;

  if (!ctx || !name)
  {
    return false;
  }

  binding = MVM_lFindImportBinding(name);

  if (!binding || !binding->handler)
  {
    return false;
  }

  return binding->handler(ctx);
} /* End of MVM_HandleImport */

/**********************************************************************************************************************
 *  LOCAL FUNCTIONS
 *********************************************************************************************************************/

static const MVM_ImportBinding_t *MVM_lFindImportBinding(const char *name)
{
  uint32_t index;
  const MVM_ImportBinding_t *binding;

  index = 0u;
  binding = NULL;

  if (!name)
  {
    return NULL;
  }

  for (index = 0u; index < (uint32_t)(sizeof(MVM_lImportBindings) / sizeof(MVM_lImportBindings[0])); ++index)
  {
    binding = &MVM_lImportBindings[index];

    if (binding->name && strcmp(binding->name, name) == 0)
    {
      return binding;
    }
  }

  return NULL;
} /* End of MVM_lFindImportBinding */

static VMGPStream *MVM_lFindStream(VMGPContext *ctx, uint32_t handle)
{
  uint32_t index;

  index = 0u;

  for (index = 0u; index < VMGP_MAX_STREAMS; ++index)
  {
    if (ctx->streams[index].used && ctx->streams[index].handle == handle)
    {
      return &ctx->streams[index];
    }
  }

  return NULL;
} /* End of MVM_lFindStream */

static VMGPStream *MVM_lAllocStream(VMGPContext *ctx)
{
  uint32_t index;

  index = 0u;

  for (index = 0u; index < VMGP_MAX_STREAMS; ++index)
  {
    if (!ctx->streams[index].used)
    {
      memset(&ctx->streams[index], 0, sizeof(ctx->streams[index]));
      ctx->streams[index].used = true;
      ctx->streams[index].handle = index;

      return &ctx->streams[index];
    }
  }

  return NULL;
} /* End of MVM_lAllocStream */

static bool MVM_lReadStreamBytes(const VMGPContext *ctx, const VMGPStream *stream, uint32_t pos, void *dst, uint32_t size)
{
  if (!ctx || !stream || !dst)
  {
    return false;
  }

  if (pos > stream->size || size > (stream->size - pos))
  {
    return false;
  }

  if (stream->overlay_data)
  {
    memcpy(dst, stream->overlay_data + pos, size);

    return true;
  }

  return MVM_ReadImageRange(ctx, stream->file_offset + pos, dst, size);
} /* End of MVM_lReadStreamBytes */

static bool MVM_lReadLzHeader(const uint8_t *p,
                              size_t remain,
                              uint8_t *extended_offset_bits,
                              uint8_t *max_offset_bits,
                              uint32_t *uncompressed_size,
                              uint32_t *compressed_size)
{
  uint32_t raw_size;
  uint32_t packed_size;

  if (!p || remain < 22u || p[0] != 'L' || p[1] != 'Z')
  {
    return false;
  }

  raw_size = vm_read_u32_le(p + 4);
  packed_size = vm_read_u32_le(p + 8);

  if (extended_offset_bits)
  {
    *extended_offset_bits = p[3];
  }

  if (max_offset_bits)
  {
    *max_offset_bits = p[2];
  }

  if (compressed_size)
  {
    *compressed_size = packed_size;
  }

  if (raw_size == 0x200u && packed_size > 1u && packed_size < raw_size)
  {
    raw_size = packed_size - 1u;
  }

  if (uncompressed_size)
  {
    *uncompressed_size = raw_size;
  }

  return true;
} /* End of MVM_lReadLzHeader */

static bool MVM_lLzBitsValid(const LZBitStream *bit_stream)
{
  return bit_stream && bit_stream->bit_pos < bit_stream->size * 8u;
} /* End of MVM_lLzBitsValid */

static bool MVM_lReadLzByte(LZBitStream *bit_stream, uint32_t byte_index, uint8_t *value)
{
  bool valid;

  valid = false;

  if (!bit_stream || !value || byte_index >= bit_stream->size)
  {
    return false;
  }

  if (bit_stream->data)
  {
    *value = bit_stream->data[byte_index];

    return true;
  }

  if (bit_stream->cache_valid && bit_stream->cached_index == byte_index)
  {
    *value = bit_stream->cached_value;

    return true;
  }

  valid = bit_stream->ctx &&
          MVM_ReadImageRange(bit_stream->ctx, bit_stream->file_offset + byte_index, &bit_stream->cached_value, 1u);

  if (valid)
  {
    bit_stream->cached_index = byte_index;
    bit_stream->cache_valid = true;
    *value = bit_stream->cached_value;
  }

  return valid;
} /* End of MVM_lReadLzByte */

static uint32_t MVM_lReadLzBits(LZBitStream *bit_stream, uint32_t count)
{
  uint32_t result;
  uint32_t index;
  uint32_t byte_index;
  uint32_t bit_index;
  uint8_t byte;

  result = 0u;
  index = 0u;
  byte_index = 0u;
  bit_index = 0u;
  byte = 0u;

  for (index = 0u; index < count; ++index)
  {
    result <<= 1;

    if (MVM_lLzBitsValid(bit_stream))
    {
      byte_index = bit_stream->bit_pos >> 3;
      bit_index = 7u - (bit_stream->bit_pos & 7u);

      if (!MVM_lReadLzByte(bit_stream, byte_index, &byte))
      {
        bit_stream->bit_pos = bit_stream->size * 8u;
        break;
      }

      result |= (uint32_t)((byte >> bit_index) & 1u);
      bit_stream->bit_pos++;
    }
  }

  return result;
} /* End of MVM_lReadLzBits */

static uint32_t MVM_lDecompressLzContent(LZBitStream *bit_stream,
                                         uint8_t *dst,
                                         uint32_t dst_size,
                                         uint8_t extended_offset_bits,
                                         uint8_t max_offset_bits)
{
  uint32_t dst_pos;
  uint32_t prefix_bits;
  uint32_t copy_len;
  uint32_t back_offset;
  uint32_t index;
  uint32_t from;

  dst_pos = 0u;
  prefix_bits = 0u;
  copy_len = 0u;
  back_offset = 0u;
  index = 0u;
  from = 0u;

  while (dst_pos < dst_size && MVM_lLzBitsValid(bit_stream))
  {
    if (MVM_lReadLzBits(bit_stream, 1u) == 1u)
    {
      prefix_bits = 0u;
      copy_len = 2u;
      back_offset = 0u;

      while (prefix_bits < max_offset_bits && MVM_lReadLzBits(bit_stream, 1u) == 1u)
      {
        prefix_bits++;
      }

      if (prefix_bits != 0u)
      {
        copy_len = (MVM_lReadLzBits(bit_stream, prefix_bits) | (1u << prefix_bits)) + 1u;
      }

      if (copy_len == 2u)
      {
        back_offset = MVM_lReadLzBits(bit_stream, 8u) + 2u;
      }
      else
      {
        back_offset = MVM_lReadLzBits(bit_stream, extended_offset_bits) + copy_len;
      }

      for (index = 0u; index < copy_len && dst_pos < dst_size; ++index)
      {
        from = (back_offset <= dst_pos) ? (dst_pos - back_offset) : 0u;
        dst[dst_pos] = dst[from];
        dst_pos++;
      }
    }
    else
    {
      dst[dst_pos++] = (uint8_t)(MVM_lReadLzBits(bit_stream, 8u) & 0xFFu);
    }
  }

  return dst_pos;
} /* End of MVM_lDecompressLzContent */

static uint32_t MVM_lUnicodeStrLen(const uint8_t *src, size_t max_bytes)
{
  uint32_t length;
  uint16_t code_unit;

  length = 0u;
  code_unit = 0u;

  if (!src)
  {
    return 0u;
  }

  while ((length * 2u + 1u) < max_bytes)
  {
    code_unit = vm_read_u16_le(src + length * 2u);

    if (code_unit == 0u)
    {
      break;
    }

    length++;
  }

  return length;
} /* End of MVM_lUnicodeStrLen */

static int32_t MVM_lUnicodeStrCmp(const uint8_t *left, size_t left_bytes, const uint8_t *right, size_t right_bytes)
{
  uint16_t left_unit;
  uint16_t right_unit;
  uint32_t offset;

  left_unit = 0u;
  right_unit = 0u;
  offset = 0u;

  while ((offset + 1u) < left_bytes && (offset + 1u) < right_bytes)
  {
    left_unit = vm_read_u16_le(left + offset);
    right_unit = vm_read_u16_le(right + offset);

    if (left_unit != right_unit)
    {
      return (left_unit < right_unit) ? -1 : 1;
    }

    if (left_unit == 0u)
    {
      return 0;
    }

    offset += 2u;
  }

  return 0;
} /* End of MVM_lUnicodeStrCmp */

static int32_t MVM_lFixedFromDouble(double value)
{
  if (value >= 0.0)
  {
    return (int32_t)(value * 16384.0 + 0.5);
  }

  return (int32_t)(value * 16384.0 - 0.5);
} /* End of MVM_lFixedFromDouble */

static double MVM_lFixedToDouble(int32_t value)
{
  return (double)value / 16384.0;
} /* End of MVM_lFixedToDouble */

static bool MVM_lReadSpriteHeader(const VMGPContext *ctx,
                                  uint32_t sprite_addr,
                                  uint16_t *width,
                                  uint16_t *height)
{
  uint16_t legacy_width;
  uint16_t legacy_height;

  if (!ctx || !width || !height)
  {
    return false;
  }

  if (!MVM_RuntimeMemRangeOk(ctx, sprite_addr, 10u))
  {
    return false;
  }

  *width = vm_read_u16_le(ctx->mem + sprite_addr + 6u);
  *height = vm_read_u16_le(ctx->mem + sprite_addr + 8u);

  if (*width == 0u || *height == 0u)
  {
    legacy_width = vm_read_u16_le(ctx->mem + sprite_addr + 0u);
    legacy_height = vm_read_u16_le(ctx->mem + sprite_addr + 8u);
    if (legacy_width != 0u && legacy_height != 0u)
    {
      *width = legacy_width;
      *height = legacy_height;
    }
    else
    {
      legacy_width = vm_read_u16_le(ctx->mem + sprite_addr + 4u);
      legacy_height = vm_read_u16_le(ctx->mem + sprite_addr + 8u);
      if (legacy_width != 0u && legacy_height != 0u)
      {
        *width = legacy_width;
        *height = legacy_height;
      }
    }
  }

  return true;
} /* End of MVM_lReadSpriteHeader */

static bool MVM_lRectanglesOverlap(int32_t ax,
                                   int32_t ay,
                                   uint32_t aw,
                                   uint32_t ah,
                                   int32_t bx,
                                   int32_t by,
                                   uint32_t bw,
                                   uint32_t bh)
{
  return !((ax + (int32_t)aw) <= bx ||
           (bx + (int32_t)bw) <= ax ||
           (ay + (int32_t)ah) <= by ||
           (by + (int32_t)bh) <= ay);
} /* End of MVM_lRectanglesOverlap */

static bool MVM_lReadMapHeader(const VMGPContext *ctx, uint32_t header_addr, VMGPMapState *map_state)
{
  if (!ctx || !map_state)
  {
    return false;
  }

  if (!MVM_RuntimeMemRangeOk(ctx, header_addr, 24u))
  {
    return false;
  }

  map_state->valid = true;
  map_state->flags = ctx->mem[header_addr + 0u];
  map_state->format = ctx->mem[header_addr + 1u];
  map_state->width = ctx->mem[header_addr + 2u];
  map_state->height = ctx->mem[header_addr + 3u];
  map_state->animation_speed = ctx->mem[header_addr + 4u];
  map_state->animation_count = ctx->mem[header_addr + 5u];
  map_state->animation_active = ctx->mem[header_addr + 6u];
  map_state->x_pan = (int16_t)vm_read_u16_le(ctx->mem + header_addr + 8u);
  map_state->y_pan = (int16_t)vm_read_u16_le(ctx->mem + header_addr + 10u);
  map_state->x_pos = (int16_t)vm_read_u16_le(ctx->mem + header_addr + 12u);
  map_state->y_pos = (int16_t)vm_read_u16_le(ctx->mem + header_addr + 14u);
  map_state->map_data_addr = vm_read_u32_le(ctx->mem + header_addr + 16u);
  map_state->tile_data_addr = vm_read_u32_le(ctx->mem + header_addr + 20u);
  map_state->header_addr = header_addr;

  return true;
} /* End of MVM_lReadMapHeader */

static uint32_t MVM_lMapCellStride(const VMGPMapState *map_state)
{
  if (map_state && (map_state->flags & 0x01u) != 0u)
  {
    return 2u;
  }

  return 1u;
} /* End of MVM_lMapCellStride */

static uint8_t *MVM_lMapCellPtr(const VMGPContext *ctx, const VMGPMapState *map_state, uint32_t x, uint32_t y)
{
  uint32_t stride;
  uint32_t offset;

  if (!ctx || !map_state || !map_state->valid)
  {
    return NULL;
  }

  if (x >= map_state->width || y >= map_state->height)
  {
    return NULL;
  }

  stride = MVM_lMapCellStride(map_state);
  offset = map_state->map_data_addr + ((y * (uint32_t)map_state->width) + x) * stride;

  if (!MVM_RuntimeMemRangeOk(ctx, offset, stride))
  {
    return NULL;
  }

  return ctx->mem + offset;
} /* End of MVM_lMapCellPtr */

/**
 * @brief Reads one heap block header from guest memory.
 */
static bool MVM_lHeapReadBlock(const VMGPContext *ctx, uint32_t block_addr, uint32_t *size, bool *used)
{
  uint32_t payload_size;
  uint32_t flags;
  uint32_t next_addr;

  payload_size = 0u;
  flags = 0u;
  next_addr = 0u;

  if (!ctx ||
      block_addr < ctx->heap_base ||
      block_addr > ctx->heap_cur ||
      (ctx->heap_cur - block_addr) < HEAP_BLOCK_HEADER_SIZE ||
      !MVM_RuntimeMemRangeOk(ctx, block_addr, HEAP_BLOCK_HEADER_SIZE))
  {
    return false;
  }

  payload_size = vm_align4(vm_read_u32_le(ctx->mem + block_addr + 0u));
  flags = vm_read_u32_le(ctx->mem + block_addr + 4u);

  if (payload_size == 0u)
  {
    return false;
  }

  next_addr = block_addr + HEAP_BLOCK_HEADER_SIZE + payload_size;

  if (next_addr < block_addr || next_addr > ctx->heap_cur)
  {
    return false;
  }

  if (size)
  {
    *size = payload_size;
  }

  if (used)
  {
    *used = ((flags & HEAP_BLOCK_FLAG_USED) != 0u);
  }

  return true;
} /* End of MVM_lHeapReadBlock */

/**
 * @brief Writes one heap block header into guest memory.
 */
static void MVM_lHeapWriteBlock(VMGPContext *ctx, uint32_t block_addr, uint32_t size, bool used)
{
  vm_write_u32_le(ctx->mem + block_addr + 0u, vm_align4(size));
  vm_write_u32_le(ctx->mem + block_addr + 4u, used ? HEAP_BLOCK_FLAG_USED : 0u);
  MVM_WatchMemoryWrite(ctx, block_addr, HEAP_BLOCK_HEADER_SIZE, "heap-header");
} /* End of MVM_lHeapWriteBlock */

/**
 * @brief Coalesces adjacent free heap blocks and releases trailing free space.
 */
static void MVM_lHeapCompact(VMGPContext *ctx)
{
  uint32_t addr;
  uint32_t size;
  uint32_t next_addr;
  uint32_t next_size;
  bool used;
  bool next_used;

  addr = 0u;
  size = 0u;
  next_addr = 0u;
  next_size = 0u;
  used = false;
  next_used = false;

  if (!ctx || ctx->heap_cur <= ctx->heap_base)
  {
    return;
  }

  addr = ctx->heap_base;

  while (addr < ctx->heap_cur && MVM_lHeapReadBlock(ctx, addr, &size, &used))
  {
    next_addr = addr + HEAP_BLOCK_HEADER_SIZE + size;

    if (!used)
    {
      while (next_addr < ctx->heap_cur && MVM_lHeapReadBlock(ctx, next_addr, &next_size, &next_used) && !next_used)
      {
        size += HEAP_BLOCK_HEADER_SIZE + next_size;
        next_addr += HEAP_BLOCK_HEADER_SIZE + next_size;
      }

      MVM_lHeapWriteBlock(ctx, addr, size, false);

      if (next_addr == ctx->heap_cur)
      {
        ctx->heap_cur = addr;
        break;
      }
    }

    addr = next_addr;
  }
} /* End of MVM_lHeapCompact */

/**
 * @brief Finds one heap block by its guest payload pointer.
 */
static bool MVM_lHeapFindBlockByPayload(const VMGPContext *ctx,
                                        uint32_t payload_addr,
                                        uint32_t *block_addr,
                                        uint32_t *size,
                                        bool *used)
{
  uint32_t addr;
  uint32_t block_size;
  bool block_used;

  addr = 0u;
  block_size = 0u;
  block_used = false;

  if (!ctx || payload_addr < (ctx->heap_base + HEAP_BLOCK_HEADER_SIZE) || payload_addr >= ctx->heap_cur)
  {
    return false;
  }

  addr = ctx->heap_base;

  while (addr < ctx->heap_cur && MVM_lHeapReadBlock(ctx, addr, &block_size, &block_used))
  {
    if ((addr + HEAP_BLOCK_HEADER_SIZE) == payload_addr)
    {
      if (block_addr)
      {
        *block_addr = addr;
      }

      if (size)
      {
        *size = block_size;
      }

      if (used)
      {
        *used = block_used;
      }

      return true;
    }

    addr += HEAP_BLOCK_HEADER_SIZE + block_size;
  }

  return false;
} /* End of MVM_lHeapFindBlockByPayload */

/**
 * @brief Allocates one guest heap block using first-fit reuse.
 */
static bool MVM_lHeapAlloc(VMGPContext *ctx, uint32_t size, uint32_t *payload_addr)
{
  uint32_t addr;
  uint32_t block_size;
  uint32_t remainder_addr;
  uint32_t remainder_size;
  bool used;

  addr = 0u;
  block_size = 0u;
  remainder_addr = 0u;
  remainder_size = 0u;
  used = false;

  if (!ctx || !payload_addr)
  {
    return false;
  }

  size = vm_align4(size ? size : 4u);
  MVM_lHeapCompact(ctx);
  addr = ctx->heap_base;

  while (addr < ctx->heap_cur && MVM_lHeapReadBlock(ctx, addr, &block_size, &used))
  {
    if (!used && block_size >= size)
    {
      if (block_size >= (size + HEAP_BLOCK_HEADER_SIZE + 4u))
      {
        remainder_addr = addr + HEAP_BLOCK_HEADER_SIZE + size;
        remainder_size = block_size - size - HEAP_BLOCK_HEADER_SIZE;
        MVM_lHeapWriteBlock(ctx, addr, size, true);
        MVM_lHeapWriteBlock(ctx, remainder_addr, remainder_size, false);
      }
      else
      {
        MVM_lHeapWriteBlock(ctx, addr, block_size, true);
      }

      *payload_addr = addr + HEAP_BLOCK_HEADER_SIZE;
      return true;
    }

    addr += HEAP_BLOCK_HEADER_SIZE + block_size;
  }

  addr = vm_align4(ctx->heap_cur);

  if (addr > ctx->heap_limit ||
      (HEAP_BLOCK_HEADER_SIZE + size) > (ctx->heap_limit - addr))
  {
    return false;
  }

  MVM_lHeapWriteBlock(ctx, addr, size, true);
  ctx->heap_cur = addr + HEAP_BLOCK_HEADER_SIZE + size;
  *payload_addr = addr + HEAP_BLOCK_HEADER_SIZE;

  return true;
} /* End of MVM_lHeapAlloc */

/**
 * @brief Frees one guest heap block and compacts adjacent free blocks.
 */
static bool MVM_lHeapFree(VMGPContext *ctx, uint32_t payload_addr)
{
  uint32_t block_addr;
  uint32_t size;
  bool used;

  block_addr = 0u;
  size = 0u;
  used = false;

  if (!ctx || !MVM_lHeapFindBlockByPayload(ctx, payload_addr, &block_addr, &size, &used) || !used)
  {
    return false;
  }

  MVM_lHeapWriteBlock(ctx, block_addr, size, false);
  MVM_lHeapCompact(ctx);

  return true;
} /* End of MVM_lHeapFree */

/**
 * @brief Returns the largest currently free guest heap block.
 */
static uint32_t MVM_lHeapMaxFreeBlock(VMGPContext *ctx)
{
  uint32_t addr;
  uint32_t size;
  uint32_t free_bytes;
  bool used;

  addr = 0u;
  size = 0u;
  free_bytes = 0u;
  used = false;

  if (!ctx)
  {
    return 0u;
  }

  MVM_lHeapCompact(ctx);
  addr = ctx->heap_base;

  while (addr < ctx->heap_cur && MVM_lHeapReadBlock(ctx, addr, &size, &used))
  {
    if (!used && size > free_bytes)
    {
      free_bytes = size;
    }

    addr += HEAP_BLOCK_HEADER_SIZE + size;
  }

  if (ctx->heap_cur <= ctx->heap_limit && (ctx->heap_limit - ctx->heap_cur) > free_bytes)
  {
    free_bytes = ctx->heap_limit - ctx->heap_cur;
  }

  return free_bytes;
} /* End of MVM_lHeapMaxFreeBlock */

static MVM_DrawCommand_t *MVM_lAllocDrawCommand(VMGPContext *ctx, MVM_DrawCommandType_t type)
{
  MVM_DrawCommand_t *command;

  if (!ctx || ctx->draw_command_count >= VMGP_MAX_DRAW_COMMANDS)
  {
    return NULL;
  }

  command = &ctx->draw_commands[ctx->draw_command_count++];
  memset(command, 0, sizeof(*command));
  command->type = type;

  return command;
} /* End of MVM_lAllocDrawCommand */

/**
 * @brief SDK: Allocates one guest-memory block and returns one guest pointer in `r0`.
 * Call model: `sync/result`
 * Ownership: Returns one guest offset only; host code must not retain it as one native pointer.
 * Blocking: Non-blocking.
 * Status: Implemented with one first-fit guest heap allocator.
 */
MVM_IMPORT_IMPL(vNewPtr)
{
  uint32_t size;
  uint32_t addr;

  size = 0u;
  addr = 0u;

  size = ctx->regs[VM_REG_P0];
  ctx->regs[VM_REG_R0] = MVM_lHeapAlloc(ctx, size, &addr) ? addr : 0u;

  MVM_LOG_D(ctx,
            "heap-newptr",
            "vNewPtr(size=%u) -> %08X heap_cur=%08X\n",
            size,
            ctx->regs[VM_REG_R0],
            ctx->heap_cur);

  return true;
} /* End of vNewPtr */

/**
 * @brief SDK: Releases one guest-memory block referenced by `p0`.
 * Call model: `sync/fire-and-forget`
 * Ownership: Consumes one guest pointer value only for the duration of the call.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vDisposePtr)
{
  bool released;

  released = MVM_lHeapFree(ctx, ctx->regs[VM_REG_P0]);
  ctx->regs[VM_REG_R0] = 0u;

  MVM_LOG_D(ctx,
            "heap-free",
            "vDisposePtr(ptr=%08X) released=%u -> %08X\n",
            ctx->regs[VM_REG_P0],
            released ? 1u : 0u,
            ctx->regs[VM_REG_R0]);

  return true;
} /* End of vDisposePtr */

/**
 * @brief SDK: Releases one guest-memory block referenced by `p0`.
 * Call model: `sync/fire-and-forget`
 * Ownership: Consumes one guest pointer value only for the duration of the call.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vMemFree)
{
  bool released;

  released = MVM_lHeapFree(ctx, ctx->regs[VM_REG_P0]);
  ctx->regs[VM_REG_R0] = 0u;

  MVM_LOG_D(ctx,
            "heap-free",
            "vMemFree(ptr=%08X) released=%u -> %08X\n",
            ctx->regs[VM_REG_P0],
            released ? 1u : 0u,
            ctx->regs[VM_REG_R0]);

  return true;
} /* End of vMemFree */

/**
 * @brief SDK: Returns the size of the largest free heap block.
 * Call model: `sync/result`
 * Ownership: No guest memory access.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vMaxFreeBlock)
{
  uint32_t free_bytes;

  free_bytes = 0u;
  free_bytes = MVM_lHeapMaxFreeBlock(ctx);

  ctx->regs[VM_REG_R0] = free_bytes;

  MVM_LOG_D(ctx,
            "heap-maxfree",
            "vMaxFreeBlock() -> %u\n",
            free_bytes);

  return true;
} /* End of vMaxFreeBlock */

/**
 * @brief SDK: Allocates one guest-memory block and records debug call-site metadata in the emulator.
 * Call model: `sync/result`
 * Ownership: Returns one guest offset only; debug `file/line` parameters are not retained.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vNewPtrDbg)
{
  uint32_t size;
  uint32_t file;
  uint32_t line;

  size = ctx->regs[VM_REG_P0];
  file = ctx->regs[VM_REG_P1];
  line = ctx->regs[VM_REG_P2];

  ctx->regs[VM_REG_P0] = size;
  vNewPtr(ctx);

  MVM_LOG_D(ctx,
            "heap-newptrdbg",
            "vNewPtrDbg(size=%u file=%08X line=%u) -> %08X\n",
            size,
            file,
            line,
            ctx->regs[VM_REG_R0]);

  return true;
} /* End of vNewPtrDbg */

/**
 * @brief SDK: Returns the length of one zero-terminated guest string.
 * Call model: `sync/result`
 * Ownership: Reads guest memory only for the duration of the call.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vStrLen)
{
  uint32_t src;

  src = ctx->regs[VM_REG_P0];
  ctx->regs[VM_REG_R0] = (src < ctx->mem_size) ? MVM_RuntimeStrLen(ctx->mem + src, ctx->mem_size - src) : 0u;

  MVM_LOG_D(ctx,
            "str-len",
            "vStrLen(src=%08X) -> %u\n",
            src,
            ctx->regs[VM_REG_R0]);

  return true;
} /* End of vStrLen */

/**
 * @brief SDK: Copies one zero-terminated guest string from `p1` into destination buffer `p0`.
 * Call model: `sync/result`
 * Ownership: Reads guest memory from `p1`, writes guest memory to `p0`, and does not retain pointers.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vStrCpy)
{
  uint32_t dst;
  uint32_t src;
  size_t max_copy;
  size_t copied;

  dst = ctx->regs[VM_REG_P0];
  src = ctx->regs[VM_REG_P1];
  max_copy = 0u;
  copied = 0u;

  if (dst < ctx->mem_size && src < ctx->mem_size)
  {
    max_copy = ctx->mem_size - dst;
    copied = MVM_RuntimeStrLen(ctx->mem + src, ctx->mem_size - src);

    if ((copied + 1u) > max_copy)
    {
      copied = max_copy ? (max_copy - 1u) : 0u;
    }

    MVM_WatchMemoryWrite(ctx, dst, (uint32_t)(copied + 1u), "vStrCpy");
    memmove(ctx->mem + dst, ctx->mem + src, copied);

    if (max_copy)
    {
      ctx->mem[dst + copied] = 0u;
    }
  }

  ctx->regs[VM_REG_R0] = dst;

  MVM_LOG_D(ctx,
            "str-cpy",
            "vStrCpy(dst=%08X src=%08X copied=%u) -> %08X\n",
            dst,
            src,
            (uint32_t)copied,
            ctx->regs[VM_REG_R0]);

  return true;
} /* End of vStrCpy */

/**
 * @brief SDK: Compares two zero-terminated guest strings.
 * Call model: `sync/result`
 * Ownership: Reads guest memory only for the duration of the call.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vStrCmp)
{
  uint32_t left;
  uint32_t right;
  int32_t result;

  left = ctx->regs[VM_REG_P0];
  right = ctx->regs[VM_REG_P1];
  result = 0;

  if (left < ctx->mem_size && right < ctx->mem_size)
  {
    result = strcmp((const char *)(ctx->mem + left), (const char *)(ctx->mem + right));
  }

  ctx->regs[VM_REG_R0] = (uint32_t)result;

  MVM_LOG_D(ctx,
            "str-cmp",
            "vStrCmp(left=%08X right=%08X) -> %d\n",
            left,
            right,
            result);

  return true;
} /* End of vStrCmp */

/**
 * @brief SDK: Concatenates one zero-terminated guest string onto another.
 * Call model: `sync/result`
 * Ownership: Reads guest memory from `p1`, writes guest memory to `p0`, and does not retain pointers.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vStrCat)
{
  uint32_t dst;
  uint32_t src;
  uint32_t dst_len;
  size_t max_copy;
  size_t copied;

  dst = ctx->regs[VM_REG_P0];
  src = ctx->regs[VM_REG_P1];
  dst_len = 0u;
  max_copy = 0u;
  copied = 0u;

  if (dst < ctx->mem_size && src < ctx->mem_size)
  {
    dst_len = MVM_RuntimeStrLen(ctx->mem + dst, ctx->mem_size - dst);
    max_copy = ctx->mem_size - dst - dst_len;
    copied = MVM_RuntimeStrLen(ctx->mem + src, ctx->mem_size - src);

    if ((copied + 1u) > max_copy)
    {
      copied = max_copy ? (max_copy - 1u) : 0u;
    }

    MVM_WatchMemoryWrite(ctx, dst + dst_len, (uint32_t)(copied + 1u), "vStrCat");
    memmove(ctx->mem + dst + dst_len, ctx->mem + src, copied);

    if (max_copy)
    {
      ctx->mem[dst + dst_len + copied] = 0u;
    }
  }

  ctx->regs[VM_REG_R0] = dst;

  MVM_LOG_D(ctx,
            "str-cat",
            "vStrCat(dst=%08X src=%08X append=%u) -> %08X\n",
            dst,
            src,
            (uint32_t)copied,
            ctx->regs[VM_REG_R0]);

  return true;
} /* End of vStrCat */

/**
 * @brief SDK: Returns the length of one zero-terminated Unicode guest string.
 * Call model: `sync/result`
 * Ownership: Reads guest memory only for the duration of the call.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vStrLenU)
{
  uint32_t src;

  src = ctx->regs[VM_REG_P0];
  ctx->regs[VM_REG_R0] = (src < ctx->mem_size) ? MVM_lUnicodeStrLen(ctx->mem + src, ctx->mem_size - src) : 0u;

  MVM_LOG_D(ctx,
            "str-lenu",
            "vStrLenU(src=%08X) -> %u\n",
            src,
            ctx->regs[VM_REG_R0]);

  return true;
} /* End of vStrLenU */

/**
 * @brief SDK: Copies one zero-terminated Unicode guest string.
 * Call model: `sync/result`
 * Ownership: Reads guest memory from `p1`, writes guest memory to `p0`, and does not retain pointers.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vStrCpyU)
{
  uint32_t dst;
  uint32_t src;
  uint32_t copied_units;
  uint16_t code_unit;

  dst = ctx->regs[VM_REG_P0];
  src = ctx->regs[VM_REG_P1];
  copied_units = 0u;
  code_unit = 0u;

  while (MVM_RuntimeMemRangeOk(ctx, dst + copied_units * 2u, 2u) &&
         MVM_RuntimeMemRangeOk(ctx, src + copied_units * 2u, 2u))
  {
    code_unit = vm_read_u16_le(ctx->mem + src + copied_units * 2u);
    vm_write_u16_le(ctx->mem + dst + copied_units * 2u, code_unit);
    copied_units++;

    if (code_unit == 0u)
    {
      break;
    }
  }

  MVM_WatchMemoryWrite(ctx, dst, copied_units * 2u, "vStrCpyU");
  ctx->regs[VM_REG_R0] = dst;

  MVM_LOG_D(ctx,
            "str-cpyu",
            "vStrCpyU(dst=%08X src=%08X copied=%u) -> %08X\n",
            dst,
            src,
            copied_units ? (copied_units - 1u) : 0u,
            ctx->regs[VM_REG_R0]);

  return true;
} /* End of vStrCpyU */

/**
 * @brief SDK: Compares two zero-terminated Unicode guest strings.
 * Call model: `sync/result`
 * Ownership: Reads guest memory only for the duration of the call.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vStrCmpU)
{
  uint32_t left;
  uint32_t right;
  int32_t result;

  left = ctx->regs[VM_REG_P0];
  right = ctx->regs[VM_REG_P1];
  result = 0;

  if (left < ctx->mem_size && right < ctx->mem_size)
  {
    result = MVM_lUnicodeStrCmp(ctx->mem + left, ctx->mem_size - left, ctx->mem + right, ctx->mem_size - right);
  }

  ctx->regs[VM_REG_R0] = (uint32_t)result;

  MVM_LOG_D(ctx,
            "str-cmpu",
            "vStrCmpU(left=%08X right=%08X) -> %d\n",
            left,
            right,
            result);

  return true;
} /* End of vStrCmpU */

/**
 * @brief SDK: Concatenates one zero-terminated Unicode guest string onto another.
 * Call model: `sync/result`
 * Ownership: Reads guest memory from `p1`, writes guest memory to `p0`, and does not retain pointers.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vStrCatU)
{
  uint32_t dst;
  uint32_t src;
  uint32_t dst_units;
  uint32_t copied_units;
  uint16_t code_unit;

  dst = ctx->regs[VM_REG_P0];
  src = ctx->regs[VM_REG_P1];
  dst_units = 0u;
  copied_units = 0u;
  code_unit = 0u;

  if (!MVM_RuntimeMemRangeOk(ctx, dst, 2u) || !MVM_RuntimeMemRangeOk(ctx, src, 2u))
  {
    ctx->regs[VM_REG_R0] = dst;
    return true;
  }

  dst_units = MVM_lUnicodeStrLen(ctx->mem + dst, ctx->mem_size - dst);

  while (MVM_RuntimeMemRangeOk(ctx, dst + (dst_units + copied_units) * 2u, 2u) &&
         MVM_RuntimeMemRangeOk(ctx, src + copied_units * 2u, 2u))
  {
    code_unit = vm_read_u16_le(ctx->mem + src + copied_units * 2u);
    vm_write_u16_le(ctx->mem + dst + (dst_units + copied_units) * 2u, code_unit);
    copied_units++;

    if (code_unit == 0u)
    {
      break;
    }
  }

  MVM_WatchMemoryWrite(ctx, dst + dst_units * 2u, copied_units * 2u, "vStrCatU");
  ctx->regs[VM_REG_R0] = dst;

  MVM_LOG_D(ctx,
            "str-catu",
            "vStrCatU(dst=%08X src=%08X append=%u) -> %08X\n",
            dst,
            src,
            copied_units ? (copied_units - 1u) : 0u,
            ctx->regs[VM_REG_R0]);

  return true;
} /* End of vStrCatU */

/**
 * @brief SDK: Converts one ASCII guest string into Unicode.
 * Call model: `sync/result`
 * Ownership: Reads guest memory from `p1`, writes guest memory to `p0`, and does not retain pointers.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vStrToU)
{
  uint32_t dst;
  uint32_t src;
  uint32_t copied_units;
  uint8_t value;

  dst = ctx->regs[VM_REG_P0];
  src = ctx->regs[VM_REG_P1];
  copied_units = 0u;
  value = 0u;

  while (MVM_RuntimeMemRangeOk(ctx, dst + copied_units * 2u, 2u) &&
         MVM_RuntimeMemRangeOk(ctx, src + copied_units, 1u))
  {
    value = ctx->mem[src + copied_units];
    vm_write_u16_le(ctx->mem + dst + copied_units * 2u, value);
    copied_units++;

    if (value == 0u)
    {
      break;
    }
  }

  MVM_WatchMemoryWrite(ctx, dst, copied_units * 2u, "vStrToU");
  ctx->regs[VM_REG_R0] = dst;

  MVM_LOG_D(ctx,
            "str-tou",
            "vStrToU(dst=%08X src=%08X copied=%u) -> %08X\n",
            dst,
            src,
            copied_units ? (copied_units - 1u) : 0u,
            ctx->regs[VM_REG_R0]);

  return true;
} /* End of vStrToU */

/**
 * @brief SDK: Converts one integer value to its string representation.
 * Call model: `sync/result`
 * Ownership: Writes guest memory to `p1` during the call only.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vitoa)
{
  int32_t value;
  uint32_t buffer;
  uint32_t length;
  uint32_t index;
  char temp[32];
  int written;

  value = vm_reg_s32(ctx->regs[VM_REG_P0]);
  buffer = ctx->regs[VM_REG_P1];
  length = ctx->regs[VM_REG_P2];
  index = 0u;
  written = 0;
  memset(temp, 0, sizeof(temp));

  written = snprintf(temp, sizeof(temp), "%d", value);

  if (written < 0)
  {
    written = 0;
  }

  if (length > 0u && (uint32_t)written < length)
  {
    for (index = (uint32_t)written; index < length && index < (sizeof(temp) - 1u); ++index)
    {
      temp[index] = (char)ctx->regs[VM_REG_P3];
    }

    temp[index] = '\0';
    written = (int)index;
  }

  if (buffer < ctx->mem_size)
  {
    index = 0u;

    while (index < (uint32_t)written && MVM_RuntimeMemRangeOk(ctx, buffer + index, 1u))
    {
      ctx->mem[buffer + index] = (uint8_t)temp[index];
      index++;
    }

    if (MVM_RuntimeMemRangeOk(ctx, buffer + index, 1u))
    {
      ctx->mem[buffer + index] = 0u;
      MVM_WatchMemoryWrite(ctx, buffer, index + 1u, "vitoa");
    }
  }

  ctx->regs[VM_REG_R0] = buffer + (uint32_t)written;

  MVM_LOG_D(ctx,
            "itoa",
            "vitoa(val=%d buf=%08X len=%u pad=%02X) -> %08X\n",
            value,
            buffer,
            length,
            ctx->regs[VM_REG_P3] & 0xFFu,
            ctx->regs[VM_REG_R0]);

  return true;
} /* End of vitoa */

/**
 * @brief SDK: Opens one resource-backed guest stream and returns one handle in `r0`.
 * Call model: `sync/result`
 * Ownership: Uses guest register arguments only; returned handle remains VM-owned.
 * Blocking: Non-blocking.
 * Status: Implemented on top of the image provider and VM stream table.
 */
MVM_IMPORT_IMPL(vStreamOpen)
{
  uint32_t mode;
  uint32_t resource_id;
  VMGPStream *stream;
  const VMGPResource *resource;
  uint32_t stream_type;
  void *overlay_mem;

  mode = ctx->regs[VM_REG_P1];
  resource_id = mode >> 16;
  stream = NULL;
  resource = NULL;
  stream_type = mode & 0xFFu;
  overlay_mem = NULL;

  stream = MVM_lAllocStream(ctx);

  if (!stream)
  {
    ctx->regs[VM_REG_R0] = 0xFFFFFFFFu;

    return true;
  }

  if (resource_id != 0u)
  {
    resource = MVM_GetVmgpResource(ctx, resource_id);

    if (!resource)
    {
      memset(stream, 0, sizeof(*stream));
      ctx->regs[VM_REG_R0] = 0xFFFFFFFFu;

      return true;
    }

    stream->file_offset = ctx->res_file_offset + resource->offset;
    stream->size = resource->size;
    stream->resource_id = resource_id;
  }
  else
  {
    stream->file_offset = ctx->res_file_offset;
    stream->size = ctx->header.res_size;
  }

  stream->mode = mode;
  stream->pos = 0u;

  if (resource_id != 0u && stream_type == 7u && (mode & STREAM_WRITE_FLAG) != 0u)
  {
    overlay_mem = MVM_AcquireInitBuffer(ctx, stream->size);

    if (!overlay_mem || !MVM_ReadImageRange(ctx, stream->file_offset, overlay_mem, stream->size))
    {
      memset(stream, 0, sizeof(*stream));
      ctx->regs[VM_REG_R0] = 0xFFFFFFFFu;

      MVM_LOG_W(ctx,
                "stream-open",
                "vStreamOpen(mode=%08X resid=%u) rejected: writable resource overlay unavailable\n",
                mode,
                resource_id);

      return true;
    }

    stream->overlay_data = (uint8_t *)overlay_mem;
    stream->overlay_size = stream->size;
  }

  ctx->regs[VM_REG_R0] = stream->handle;
  MVM_EmitEvent(ctx, MVM_EVENT_RESOURCE_OPENED, resource_id, stream->handle);

  MVM_LOG_D(ctx,
            "stream-open",
            "vStreamOpen(mode=%08X resid=%u) -> handle=%u size=%u overlay=%u\n",
            mode,
            resource_id,
            stream->handle,
            stream->size,
            stream->overlay_data ? 1u : 0u);

  return true;
} /* End of vStreamOpen */

/**
 * @brief SDK: Repositions one open guest stream according to `where` and `whence`.
 * Call model: `sync/result`
 * Ownership: Uses one VM-owned stream handle only for the duration of the call.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vStreamSeek)
{
  VMGPStream *stream;
  int32_t where;
  uint32_t whence;
  int32_t pos;

  stream = MVM_lFindStream(ctx, ctx->regs[VM_REG_P0]);
  where = vm_reg_s32(ctx->regs[VM_REG_P1]);
  whence = ctx->regs[VM_REG_P2];
  pos = -1;

  if (!stream)
  {
    ctx->regs[VM_REG_R0] = 0xFFFFFFFFu;

    return true;
  }

  if (whence == 0u)
  {
    pos = where;
  }
  else if (whence == 1u)
  {
    pos = (int32_t)stream->pos + where;
  }
  else if (whence == 2u)
  {
    pos = (int32_t)stream->size + where;
  }

  if (pos < 0)
  {
    pos = 0;
  }

  if ((uint32_t)pos > stream->size)
  {
    pos = (int32_t)stream->size;
  }

  stream->pos = (uint32_t)pos;
  ctx->regs[VM_REG_R0] = stream->pos;

  MVM_LOG_D(ctx,
            "stream-seek",
            "vStreamSeek(handle=%u where=%d whence=%u) -> %u\n",
            ctx->regs[VM_REG_P0],
            where,
            whence,
            ctx->regs[VM_REG_R0]);

  return true;
} /* End of vStreamSeek */

/**
 * @brief SDK: Reads bytes from one open guest stream into guest memory.
 * Call model: `sync/result`
 * Ownership: Writes guest memory at `p1`; the stream handle remains VM-owned.
 * Blocking: Non-blocking for the intended provider model.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vStreamRead)
{
  VMGPStream *stream;
  uint32_t buffer;
  uint32_t count;
  uint32_t available;

  stream = MVM_lFindStream(ctx, ctx->regs[VM_REG_P0]);
  buffer = ctx->regs[VM_REG_P1];
  count = ctx->regs[VM_REG_P2];
  available = 0u;

  if (!stream || buffer >= ctx->mem_size)
  {
    ctx->regs[VM_REG_R0] = 0xFFFFFFFFu;

    return true;
  }

  available = (stream->pos < stream->size) ? (stream->size - stream->pos) : 0u;

  if (count > available)
  {
    count = available;
  }

  if (!MVM_RuntimeMemRangeOk(ctx, buffer, count))
  {
    count = (buffer < ctx->mem_size) ? (uint32_t)(ctx->mem_size - buffer) : 0u;
  }

  if (!MVM_lReadStreamBytes(ctx, stream, stream->pos, ctx->mem + buffer, count))
  {
    ctx->regs[VM_REG_R0] = 0xFFFFFFFFu;

    return true;
  }

  MVM_WatchMemoryWrite(ctx, buffer, count, "vStreamRead");
  stream->pos += count;
  ctx->regs[VM_REG_R0] = count;
  MVM_EmitEvent(ctx, MVM_EVENT_RESOURCE_READ, stream->resource_id, count);

  MVM_LOG_D(ctx,
            "stream-read",
            "vStreamRead(handle=%u buf=%08X count=%u) -> %u pos=%u\n",
            ctx->regs[VM_REG_P0],
            buffer,
            ctx->regs[VM_REG_P2],
            count,
            stream->pos);

  return true;
} /* End of vStreamRead */

/**
 * @brief SDK: Writes bytes from guest memory into one open guest stream.
 * Call model: `sync/result`
 * Ownership: Reads guest memory at `p1` during the call only; the stream handle remains VM-owned.
 * Blocking: Non-blocking.
 * Status: Implemented with one volatile sink model for writable streams in the default integration.
 */
MVM_IMPORT_IMPL(vStreamWrite)
{
  VMGPStream *stream;
  uint32_t buffer;
  uint32_t count;
  uint32_t writable;

  stream = MVM_lFindStream(ctx, ctx->regs[VM_REG_P0]);
  buffer = ctx->regs[VM_REG_P1];
  count = ctx->regs[VM_REG_P2];
  writable = 0u;

  if (!stream || !MVM_RuntimeMemRangeOk(ctx, buffer, count))
  {
    ctx->regs[VM_REG_R0] = 0xFFFFFFFFu;

    return true;
  }

  if ((stream->mode & STREAM_WRITE_FLAG) == 0u)
  {
    ctx->regs[VM_REG_R0] = 0xFFFFFFFFu;

    MVM_LOG_W(ctx,
              "stream-write",
              "vStreamWrite(handle=%u buf=%08X count=%u) rejected: not writable\n",
              ctx->regs[VM_REG_P0],
              buffer,
              count);

    return true;
  }

  writable = count;

  if (stream->overlay_data)
  {
    writable = (stream->pos < stream->overlay_size) ? (stream->overlay_size - stream->pos) : 0u;

    if (count < writable)
    {
      writable = count;
    }

    memcpy(stream->overlay_data + stream->pos, ctx->mem + buffer, writable);
    stream->pos += writable;
    ctx->regs[VM_REG_R0] = writable;
  }
  else
  {
    stream->pos += count;

    if (stream->pos > stream->size)
    {
      stream->size = stream->pos;
    }

    ctx->regs[VM_REG_R0] = count;
  }

  if (stream->pos > stream->size)
  {
    stream->size = stream->pos;
  }

  MVM_LOG_D(ctx,
            "stream-write",
            "vStreamWrite(handle=%u buf=%08X count=%u) -> %u pos=%u\n",
            ctx->regs[VM_REG_P0],
            buffer,
            count,
            ctx->regs[VM_REG_R0],
            stream->pos);

  return true;
} /* End of vStreamWrite */

/**
 * @brief SDK: Closes one guest stream handle.
 * Call model: `sync/fire-and-forget`
 * Ownership: Releases one VM-owned stream slot; no host ownership is retained.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vStreamClose)
{
  VMGPStream *stream;

  stream = MVM_lFindStream(ctx, ctx->regs[VM_REG_P0]);

  if (stream)
  {
    memset(stream, 0, sizeof(*stream));
  }

  ctx->regs[VM_REG_R0] = 0u;

  MVM_LOG_D(ctx,
            "stream-close",
            "vStreamClose(handle=%u) -> %u\n",
            ctx->regs[VM_REG_P0],
            ctx->regs[VM_REG_R0]);

  return true;
} /* End of vStreamClose */

/**
 * @brief SDK: Returns the mode flags of one open stream.
 * Call model: `sync/result`
 * Ownership: Uses one VM-owned stream handle only for the duration of the call.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vStreamMode)
{
  VMGPStream *stream;

  stream = MVM_lFindStream(ctx, ctx->regs[VM_REG_P0]);
  ctx->regs[VM_REG_R0] = stream ? stream->mode : 0xFFFFFFFFu;

  MVM_LOG_D(ctx,
            "stream-mode",
            "vStreamMode(handle=%u) -> %08X\n",
            ctx->regs[VM_REG_P0],
            ctx->regs[VM_REG_R0]);

  return true;
} /* End of vStreamMode */

/**
 * @brief SDK: Returns whether one stream is ready for I/O.
 * Call model: `polling`
 * Ownership: Uses one VM-owned stream handle only for the duration of the call.
 * Blocking: Non-blocking.
 * Status: Implemented with one always-ready file/resource model.
 */
MVM_IMPORT_IMPL(vStreamReady)
{
  VMGPStream *stream;

  stream = MVM_lFindStream(ctx, ctx->regs[VM_REG_P0]);
  ctx->regs[VM_REG_R0] = stream ? 1u : 0u;

  MVM_LOG_D(ctx,
            "stream-ready",
            "vStreamReady(handle=%u) -> %u\n",
            ctx->regs[VM_REG_P0],
            ctx->regs[VM_REG_R0]);

  return true;
} /* End of vStreamReady */

/**
 * @brief SDK: Returns the source endpoint identifier of one stream.
 * Call model: `sync/result`
 * Ownership: Uses one VM-owned stream handle only for the duration of the call.
 * Blocking: Non-blocking.
 * Status: Partial; resource/file streams do not expose endpoint metadata and return zero.
 */
MVM_IMPORT_IMPL(vStreamFrom)
{
  ctx->regs[VM_REG_R0] = 0u;

  MVM_LOG_D(ctx,
            "stream-from",
            "vStreamFrom(handle=%u) -> %u\n",
            ctx->regs[VM_REG_P0],
            ctx->regs[VM_REG_R0]);

  return true;
} /* End of vStreamFrom */

/**
 * @brief SDK: Returns the destination endpoint identifier of one stream.
 * Call model: `sync/result`
 * Ownership: Uses one VM-owned stream handle only for the duration of the call.
 * Blocking: Non-blocking.
 * Status: Partial; resource/file streams do not expose endpoint metadata and return zero.
 */
MVM_IMPORT_IMPL(vStreamTo)
{
  ctx->regs[VM_REG_R0] = 0u;

  MVM_LOG_D(ctx,
            "stream-to",
            "vStreamTo(handle=%u) -> %u\n",
            ctx->regs[VM_REG_P0],
            ctx->regs[VM_REG_R0]);

  return true;
} /* End of vStreamTo */

/**
 * @brief SDK: Parses one packed-resource header and optionally writes one info block into guest memory.
 * Call model: `sync/result`
 * Ownership: Reads guest memory at `p1`; writes guest memory at `p0` when provided.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vDecompHdr)
{
  uint32_t info;
  uint32_t header_addr;
  uint8_t extended_offset_bits;
  uint8_t max_offset_bits;
  uint32_t raw_size;
  uint32_t packed_size;

  info = ctx->regs[VM_REG_P0];
  header_addr = ctx->regs[VM_REG_P1];
  extended_offset_bits = 0u;
  max_offset_bits = 0u;
  raw_size = 0u;
  packed_size = 0u;

  if (!MVM_RuntimeMemRangeOk(ctx, header_addr, 22u) ||
      !MVM_lReadLzHeader(ctx->mem + header_addr,
                         ctx->mem_size - header_addr,
                         &extended_offset_bits,
                         &max_offset_bits,
                         &raw_size,
                         &packed_size))
  {
    ctx->regs[VM_REG_R0] = 0xFFFFFFFFu;

    return true;
  }

  if (info != 0u && MVM_RuntimeMemRangeOk(ctx, info, 20u))
  {
    ctx->mem[info + 0u] = 0u;
    ctx->mem[info + 1u] = 0u;
    vm_write_u16_le(ctx->mem + info + 2u, 0x1234u);
    vm_write_u16_le(ctx->mem + info + 4u, 0u);
    vm_write_u16_le(ctx->mem + info + 6u, 0u);
    vm_write_u32_le(ctx->mem + info + 8u, packed_size);
    vm_write_u32_le(ctx->mem + info + 12u, raw_size);
    vm_write_u32_le(ctx->mem + info + 16u, 0u);
  }

  ctx->regs[VM_REG_R0] = raw_size;

  MVM_LOG_D(ctx,
            "decomp-hdr",
            "vDecompHdr(info=%08X hdr=%08X) -> raw=%u packed=%u\n",
            info,
            header_addr,
            raw_size,
            packed_size);

  return true;
} /* End of vDecompHdr */

/**
 * @brief SDK: Expands one packed resource from guest memory or one open stream into guest memory.
 * Call model: `sync/result`
 * Ownership: Reads guest memory or provider-backed stream data and writes guest memory at `p1`.
 * Blocking: Non-blocking for the intended provider model.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vDecompress)
{
  uint32_t src;
  uint32_t dst;
  uint32_t stream_handle;
  VMGPStream *stream;
  const uint8_t *base;
  uint32_t available;
  uint32_t stream_base_pos;
  uint8_t extended_offset_bits;
  uint8_t max_offset_bits;
  uint32_t raw_size;
  uint32_t packed_size;
  uint32_t produced;
  uint32_t copy_size;
  uint32_t dst_limit;
  uint32_t heap_limit;
  uint32_t consumed;
  uint8_t header[22];
  const uint8_t *header_ptr;
  LZBitStream bit_stream;

  src = ctx->regs[VM_REG_P0];
  dst = ctx->regs[VM_REG_P1];
  stream_handle = ctx->regs[VM_REG_P2];
  stream = NULL;
  base = NULL;
  available = 0u;
  stream_base_pos = 0u;
  extended_offset_bits = 0u;
  max_offset_bits = 0u;
  raw_size = 0u;
  packed_size = 0u;
  produced = 0u;
  copy_size = 0u;
  dst_limit = 0u;
  heap_limit = 0u;
  consumed = 0u;
  header_ptr = NULL;
  memset(&bit_stream, 0, sizeof(bit_stream));

  if (src != 0u)
  {
    if (!MVM_RuntimeMemRangeOk(ctx, src, 22u))
    {
      ctx->regs[VM_REG_R0] = 0xFFFFFFFFu;

      return true;
    }

    base = ctx->mem + src;
    available = (uint32_t)(ctx->mem_size - src);
    header_ptr = base;
  }
  else
  {
    stream = MVM_lFindStream(ctx, stream_handle);

    if (!stream || stream->pos > stream->size || 22u > (stream->size - stream->pos))
    {
      ctx->regs[VM_REG_R0] = 0xFFFFFFFFu;

      return true;
    }

    if (!MVM_lReadStreamBytes(ctx, stream, stream->pos, header, sizeof(header)))
    {
      ctx->regs[VM_REG_R0] = 0xFFFFFFFFu;

      return true;
    }

    stream_base_pos = stream->pos;
    available = stream->size - stream->pos;
    header_ptr = header;
  }

  if (!MVM_lReadLzHeader(header_ptr, available, &extended_offset_bits, &max_offset_bits, &raw_size, &packed_size))
  {
    copy_size = available;

    if (dst >= ctx->mem_size)
    {
      ctx->regs[VM_REG_R0] = 0xFFFFFFFFu;

      return true;
    }

    dst_limit = ctx->mem_size - dst;

    if (dst < ctx->heap_cur)
    {
      heap_limit = ctx->heap_cur - dst;

      if (heap_limit < dst_limit)
      {
        dst_limit = heap_limit;
      }
    }

    if (copy_size > dst_limit)
    {
      copy_size = dst_limit;
    }

    if (copy_size > 0u)
    {
      if (base)
      {
        memcpy(ctx->mem + dst, base, copy_size);
      }
      else if (!MVM_lReadStreamBytes(ctx, stream, stream_base_pos, ctx->mem + dst, copy_size))
      {
        ctx->regs[VM_REG_R0] = 0xFFFFFFFFu;

        return true;
      }
    }

    if (stream)
    {
      stream->pos = stream_base_pos + copy_size;

      if (stream->pos > stream->size)
      {
        stream->pos = stream->size;
      }
    }

    ctx->regs[VM_REG_R0] = copy_size;

    MVM_LOG_D(ctx,
              "decompress-copy",
              "vDecompress(src=%08X dst=%08X stream=%u) raw-copy=%u\n",
              src,
              dst,
              stream_handle,
              copy_size);

    return true;
  }

  if (!MVM_RuntimeMemRangeOk(ctx, dst, raw_size))
  {
    ctx->regs[VM_REG_R0] = 0xFFFFFFFFu;

    return true;
  }

  if (packed_size > (available - 22u))
  {
    packed_size = available - 22u;
  }

  bit_stream.size = packed_size;

  if (base)
  {
    bit_stream.data = base + 22u;
  }
  else
  {
    bit_stream.ctx = ctx;
    bit_stream.file_offset = stream->file_offset + stream_base_pos + 22u;
  }

  produced = MVM_lDecompressLzContent(&bit_stream,
                                      ctx->mem + dst,
                                      raw_size,
                                      extended_offset_bits,
                                      max_offset_bits);

  if (stream)
  {
    consumed = 22u + packed_size;
    stream->pos = stream_base_pos + consumed;

    if (stream->pos > stream->size)
    {
      stream->pos = stream->size;
    }
  }

  ctx->regs[VM_REG_R0] = produced;

  MVM_LOG_D(ctx,
            "decompress-lz",
            "vDecompress(src=%08X dst=%08X stream=%u packed=%u raw=%u) -> %u\n",
            src,
            dst,
            stream_handle,
            packed_size,
            raw_size,
            produced);

  return true;
} /* End of vDecompress */

/**
 * @brief SDK: Queries device capabilities and writes the requested structure into guest memory.
 * Call model: `sync/result`
 * Ownership: Writes guest memory at `p1`; uses the selected device profile only during the call.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vGetCaps)
{
  const MpnDevProfile_t *profile;
  uint32_t query;
  uint32_t out;
  uint32_t result;

  profile = NULL;
  query = 0u;
  out = 0u;
  result = 0u;

  if (!ctx || !ctx->device_profile)
  {
    return false;
  }

  profile = ctx->device_profile;
  query = ctx->regs[VM_REG_P0];
  out = ctx->regs[VM_REG_P1];

  if ((profile->supported_caps & MVM_DEVICE_CAP_VIDEO) != 0u &&
      query == 0u &&
      MVM_RuntimeMemRangeOk(ctx, out, 8u))
  {
    vm_write_u16_le(ctx->mem + out + 0u, 8u);
    vm_write_u16_le(ctx->mem + out + 2u, 8u);
    vm_write_u16_le(ctx->mem + out + 4u, profile->screen_width);
    vm_write_u16_le(ctx->mem + out + 6u, profile->screen_height);
    result = 1u;
  }
  else if ((profile->supported_caps & MVM_DEVICE_CAP_COLOR) != 0u &&
           query == 2u &&
           MVM_RuntimeMemRangeOk(ctx, out, 4u))
  {
    vm_write_u16_le(ctx->mem + out + 0u, 4u);
    vm_write_u16_le(ctx->mem + out + 2u, profile->color_mode);
    result = 1u;
  }
  else if ((profile->supported_caps & MVM_DEVICE_CAP_SOUND) != 0u &&
           query == 3u &&
           MVM_RuntimeMemRangeOk(ctx, out, 4u))
  {
    vm_write_u16_le(ctx->mem + out + 0u, 4u);
    vm_write_u16_le(ctx->mem + out + 2u, profile->sound_flags);
    result = 1u;
  }
  else if ((profile->supported_caps & MVM_DEVICE_CAP_SYSTEM) != 0u &&
           query == 4u &&
           MVM_RuntimeMemRangeOk(ctx, out, 12u))
  {
    vm_write_u16_le(ctx->mem + out + 0u, 12u);
    vm_write_u16_le(ctx->mem + out + 2u, profile->system_flags);
    vm_write_u32_le(ctx->mem + out + 4u, profile->device_id);
    vm_write_u32_le(ctx->mem + out + 8u, 0u);
    result = 1u;
  }

  ctx->regs[VM_REG_R0] = result;

  MVM_LOG_D(ctx,
            "caps",
            "vGetCaps(query=%u out=%08X profile=%s) -> %u\n",
            query,
            out,
            profile->name ? profile->name : "<unnamed>",
            result);

  return true;
} /* End of vGetCaps */

/**
 * @brief SDK: Returns the current platform tick counter.
 * Call model: `polling`
 * Ownership: No guest memory access; uses the configured platform callback if present.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vGetTickCount)
{
  if (!ctx)
  {
    return false;
  }

  if (ctx->platform.get_ticks_ms)
  {
    ctx->tick_count = ctx->platform.get_ticks_ms(ctx->platform.user);
  }
  else
  {
    if (ctx->device_profile && ctx->device_profile->frame_interval_ms != 0u)
    {
      ctx->tick_count += ctx->device_profile->frame_interval_ms;
    }
    else
    {
      ctx->tick_count += 16u;
    }
  }

  ctx->regs[VM_REG_R0] = ctx->tick_count;

  MVM_LOG_D(ctx,
            "tick",
            "vGetTickCount() -> %08X\n",
            ctx->tick_count);

  return true;
} /* End of vGetTickCount */

/**
 * @brief SDK: Writes local date/time fields into one guest `VMDateTime` structure.
 * Call model: `sync/fire-and-forget`
 * Ownership: Writes guest memory at `p0` during the call only.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vGetTimeDate)
{
  time_t now;
  struct tm *tm_value;
  uint32_t out;

  now = 0;
  tm_value = NULL;
  out = ctx->regs[VM_REG_P0];

  if (!MVM_RuntimeMemRangeOk(ctx, out, 8u))
  {
    ctx->regs[VM_REG_R0] = 0u;
    return true;
  }

  now = time(NULL);
  tm_value = localtime(&now);

  if (!tm_value)
  {
    memset(ctx->mem + out, 0, 8u);
    ctx->regs[VM_REG_R0] = 0u;
    return true;
  }

  vm_write_u16_le(ctx->mem + out + 0u, (uint16_t)(tm_value->tm_year + 1900));
  vm_write_u16_le(ctx->mem + out + 2u, (uint16_t)tm_value->tm_mday);
  ctx->mem[out + 4u] = (uint8_t)(tm_value->tm_mon + 1);
  ctx->mem[out + 5u] = (uint8_t)tm_value->tm_hour;
  ctx->mem[out + 6u] = (uint8_t)tm_value->tm_min;
  ctx->mem[out + 7u] = (uint8_t)tm_value->tm_sec;
  MVM_WatchMemoryWrite(ctx, out, 8u, "vGetTimeDate");
  ctx->regs[VM_REG_R0] = 0u;

  MVM_LOG_D(ctx,
            "time-date",
            "vGetTimeDate(out=%08X) -> %04u-%02u-%02u %02u:%02u:%02u\n",
            out,
            (uint32_t)(tm_value->tm_year + 1900),
            (uint32_t)(tm_value->tm_mon + 1),
            (uint32_t)tm_value->tm_mday,
            (uint32_t)tm_value->tm_hour,
            (uint32_t)tm_value->tm_min,
            (uint32_t)tm_value->tm_sec);

  return true;
} /* End of vGetTimeDate */

/**
 * @brief SDK: Writes UTC date/time fields into one guest `VMDateTime` structure.
 * Call model: `sync/fire-and-forget`
 * Ownership: Writes guest memory at `p0` during the call only.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vGetTimeDateUTC)
{
  time_t now;
  struct tm *tm_value;
  uint32_t out;

  now = 0;
  tm_value = NULL;
  out = ctx->regs[VM_REG_P0];

  if (!MVM_RuntimeMemRangeOk(ctx, out, 8u))
  {
    ctx->regs[VM_REG_R0] = 0u;
    return true;
  }

  now = time(NULL);
  tm_value = gmtime(&now);

  if (!tm_value)
  {
    memset(ctx->mem + out, 0, 8u);
    ctx->regs[VM_REG_R0] = 0u;
    return true;
  }

  vm_write_u16_le(ctx->mem + out + 0u, (uint16_t)(tm_value->tm_year + 1900));
  vm_write_u16_le(ctx->mem + out + 2u, (uint16_t)tm_value->tm_mday);
  ctx->mem[out + 4u] = (uint8_t)(tm_value->tm_mon + 1);
  ctx->mem[out + 5u] = (uint8_t)tm_value->tm_hour;
  ctx->mem[out + 6u] = (uint8_t)tm_value->tm_min;
  ctx->mem[out + 7u] = (uint8_t)tm_value->tm_sec;
  MVM_WatchMemoryWrite(ctx, out, 8u, "vGetTimeDateUTC");
  ctx->regs[VM_REG_R0] = 0u;

  MVM_LOG_D(ctx,
            "time-date-utc",
            "vGetTimeDateUTC(out=%08X) -> %04u-%02u-%02u %02u:%02u:%02u\n",
            out,
            (uint32_t)(tm_value->tm_year + 1900),
            (uint32_t)(tm_value->tm_mon + 1),
            (uint32_t)tm_value->tm_mday,
            (uint32_t)tm_value->tm_hour,
            (uint32_t)tm_value->tm_min,
            (uint32_t)tm_value->tm_sec);

  return true;
} /* End of vGetTimeDateUTC */

/**
 * @brief SDK: Returns the current frame counter.
 * Call model: `polling`
 * Ownership: No guest memory access.
 * Blocking: Non-blocking.
 * Status: Implemented using the VM step counter as the current fallback frame source.
 */
MVM_IMPORT_IMPL(vFrameTickCount)
{
  ctx->regs[VM_REG_R0] = ctx->steps;

  MVM_LOG_D(ctx,
            "frame-tick",
            "vFrameTickCount() -> %08X\n",
            ctx->regs[VM_REG_R0]);

  return true;
} /* End of vFrameTickCount */

/**
 * @brief SDK: Returns the mophun API version.
 * Call model: `sync/result`
 * Ownership: No guest memory access.
 * Blocking: Non-blocking.
 * Status: Implemented with one fixed compatibility version value.
 */
MVM_IMPORT_IMPL(vGetVMGPInfo)
{
  ctx->regs[VM_REG_R0] = (2u << 16) | 50u;

  MVM_LOG_D(ctx,
            "vmgp-info",
            "vGetVMGPInfo() -> %08X\n",
            ctx->regs[VM_REG_R0]);

  return true;
} /* End of vGetVMGPInfo */

/**
 * @brief SDK: Returns one device-unique identifier.
 * Call model: `sync/result`
 * Ownership: No guest memory access.
 * Blocking: Non-blocking.
 * Status: Partial; returns one deterministic placeholder identifier.
 */
MVM_IMPORT_IMPL(vUID)
{
  ctx->regs[VM_REG_R0] = 0xDEADBEEFu;

  MVM_LOG_D(ctx,
            "uid",
            "vUID() -> %08X\n",
            ctx->regs[VM_REG_R0]);

  return true;
} /* End of vUID */

/**
 * @brief SDK: Seeds the guest-visible random source.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses one scalar seed value from `p0` only during the call.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vSetRandom)
{
  if (!ctx)
  {
    return false;
  }

  ctx->random_state = ctx->regs[VM_REG_P0] ? ctx->regs[VM_REG_P0] : 1u;
  ctx->regs[VM_REG_R0] = 0u;

  MVM_LOG_D(ctx,
            "random-seed",
            "vSetRandom(seed=%08X)\n",
            ctx->random_state);

  return true;
} /* End of vSetRandom */

/**
 * @brief SDK: Returns one guest-visible random value.
 * Call model: `sync/result`
 * Ownership: No guest memory access; uses the configured platform callback or VM fallback PRNG.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vGetRandom)
{
  uint32_t value;

  value = 0u;

  if (!ctx)
  {
    return false;
  }

  if (ctx->platform.get_random)
  {
    value = ctx->platform.get_random(ctx->platform.user);
  }
  else
  {
    ctx->random_state = ctx->random_state * 1103515245u + 12345u;
    value = (ctx->random_state >> 16) & 0xFFFFu;
  }

  ctx->regs[VM_REG_R0] = value;

  MVM_LOG_D(ctx,
            "random-value",
            "vGetRandom() -> %08X\n",
            value);

  return true;
} /* End of vGetRandom */

/**
 * @brief SDK: Requests immediate VM termination.
 * Call model: `sync/fire-and-forget`
 * Ownership: No guest memory access; the VM state changes through the internal control path.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vTerminateVMGP)
{
  if (!ctx)
  {
    return false;
  }

  MVM_LOG_I(ctx, "terminate", "vTerminateVMGP()\n");
  MVM_RequestExitRaw(ctx);
  ctx->regs[VM_REG_R0] = 0u;

  return true;
} /* End of vTerminateVMGP */

/**
 * @brief SDK: Presents the current guest frame to the platform backend.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses register arguments only and emits one frame-ready event; no framebuffer is retained by default.
 * Blocking: Non-blocking.
 * Status: Partial; default integration emits events but does not render.
 */
MVM_IMPORT_IMPL(vFlipScreen)
{
  if (!ctx)
  {
    return false;
  }

  ++ctx->frame_serial;

  MVM_LOG_D(ctx,
            "frame-ready",
            "vFlipScreen(p0=%08X p1=%08X p2=%08X p3=%08X)\n",
            ctx->regs[VM_REG_P0],
            ctx->regs[VM_REG_P1],
            ctx->regs[VM_REG_P2],
            ctx->regs[VM_REG_P3]);
  MVM_EmitEvent(ctx, MVM_EVENT_FRAME_READY, ctx->regs[VM_REG_P0], ctx->pc);
  ctx->regs[VM_REG_R0] = 0u;

  return true;
} /* End of vFlipScreen */

/**
 * @brief SDK: Requests playback of one resource-backed sound or tone.
 * Call model: `async/request`
 * Ownership: Uses scalar parameters only; no guest pointers are retained.
 * Blocking: Non-blocking.
 * Status: Partial; default integration emits one sound-request event but does not play audio.
 */
MVM_IMPORT_IMPL(vPlayResource)
{
  if (!ctx)
  {
    return false;
  }

  MVM_LOG_I(ctx,
            "sound-request",
            "vPlayResource(resource=%08X p1=%08X p2=%08X p3=%08X)\n",
            ctx->regs[VM_REG_P0],
            ctx->regs[VM_REG_P1],
            ctx->regs[VM_REG_P2],
            ctx->regs[VM_REG_P3]);
  MVM_EmitEvent(ctx, MVM_EVENT_SOUND_REQUESTED, ctx->regs[VM_REG_P0], ctx->regs[VM_REG_P1]);
  ctx->regs[VM_REG_R0] = 0u;

  return true;
} /* End of vPlayResource */

/**
 * @brief SDK: Clears the active drawing surface using the color in `p0`.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Partial; updates VM-side graphics state and emits logs, but does not render.
 */
MVM_IMPORT_IMPL(vClearScreen)
{
  ctx->clear_color = ctx->regs[VM_REG_P0];
  ctx->draw_command_count = 0u;
  ctx->regs[VM_REG_R0] = 0u;

  MVM_LOG_D(ctx,
            "clear-screen",
            "vClearScreen(color=%08X)\n",
            ctx->clear_color);

  return true;
} /* End of vClearScreen */

/**
 * @brief SDK: Selects the active font object and returns the previous font pointer.
 * Call model: `sync/result`
 * Ownership: Stores one guest pointer as VM-side state; no host pointer is retained.
 * Blocking: Non-blocking.
 * Status: Partial; tracks font selection but does not interpret glyph data yet.
 */
MVM_IMPORT_IMPL(vSetActiveFont)
{
  ctx->previous_font = ctx->active_font;
  ctx->active_font = ctx->regs[VM_REG_P0];
  ctx->regs[VM_REG_R0] = ctx->previous_font;

  MVM_LOG_D(ctx,
            "font-select",
            "vSetActiveFont(font=%08X) -> prev=%08X\n",
            ctx->active_font,
            ctx->regs[VM_REG_R0]);

  return true;
} /* End of vSetActiveFont */

/**
 * @brief SDK: Selects the current foreground color.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Implemented as VM-side graphics state.
 */
MVM_IMPORT_IMPL(vSetForeColor)
{
  ctx->fg_color = ctx->regs[VM_REG_P0];
  ctx->regs[VM_REG_R0] = 0u;

  MVM_LOG_D(ctx,
            "fg-color",
            "vSetForeColor(color=%08X)\n",
            ctx->fg_color);

  return true;
} /* End of vSetForeColor */

/**
 * @brief SDK: Selects the current background color.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Implemented as VM-side graphics state.
 */
MVM_IMPORT_IMPL(vSetBackColor)
{
  ctx->bg_color = ctx->regs[VM_REG_P0];
  ctx->regs[VM_REG_R0] = 0u;

  MVM_LOG_D(ctx,
            "bg-color",
            "vSetBackColor(color=%08X)\n",
            ctx->bg_color);

  return true;
} /* End of vSetBackColor */

/**
 * @brief SDK: Selects the current clip window.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Implemented as VM-side graphics state.
 */
MVM_IMPORT_IMPL(vSetClipWindow)
{
  ctx->clip_x0 = (uint16_t)(ctx->regs[VM_REG_P0] & 0xFFFFu);
  ctx->clip_y0 = (uint16_t)(ctx->regs[VM_REG_P1] & 0xFFFFu);
  ctx->clip_x1 = (uint16_t)(ctx->regs[VM_REG_P2] & 0xFFFFu);
  ctx->clip_y1 = (uint16_t)(ctx->regs[VM_REG_P3] & 0xFFFFu);
  ctx->regs[VM_REG_R0] = 0u;

  MVM_LOG_D(ctx,
            "clip-window",
            "vSetClipWindow(%u,%u,%u,%u)\n",
            (uint32_t)ctx->clip_x0,
            (uint32_t)ctx->clip_y0,
            (uint32_t)ctx->clip_x1,
            (uint32_t)ctx->clip_y1);

  return true;
} /* End of vSetClipWindow */

/**
 * @brief SDK: Selects the current transfer mode for drawing and text output.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Implemented as VM-side graphics state.
 */
MVM_IMPORT_IMPL(vSetTransferMode)
{
  ctx->transfer_mode = ctx->regs[VM_REG_P0];
  ctx->regs[VM_REG_R0] = 0u;

  MVM_LOG_D(ctx,
            "transfer-mode",
            "vSetTransferMode(mode=%08X)\n",
            ctx->transfer_mode);

  return true;
} /* End of vSetTransferMode */

/**
 * @brief SDK: Updates one palette entry.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Implemented as VM-side palette state.
 */
MVM_IMPORT_IMPL(vSetPaletteEntry)
{
  uint32_t index;

  index = ctx->regs[VM_REG_P0] & 0xFFu;
  ctx->palette_entries[index] = ctx->regs[VM_REG_P1];
  ctx->regs[VM_REG_R0] = 0u;

  MVM_LOG_D(ctx,
            "palette-entry",
            "vSetPaletteEntry(index=%u color=%08X)\n",
            index,
            ctx->palette_entries[index]);

  return true;
} /* End of vSetPaletteEntry */

/**
 * @brief SDK: Uploads one palette definition.
 * Call model: `sync/fire-and-forget`
 * Ownership: Reads guest memory during the call only.
 * Blocking: Non-blocking.
 * Status: Partial; copies palette words when the guest pointer is valid.
 */
MVM_IMPORT_IMPL(vSetPalette)
{
  uint32_t src;
  uint32_t count;
  uint32_t index;

  src = ctx->regs[VM_REG_P0];
  count = ctx->regs[VM_REG_P1];
  index = 0u;

  if (count > 256u)
  {
    count = 256u;
  }

  if (src < ctx->mem_size)
  {
    for (index = 0u; index < count && MVM_RuntimeMemRangeOk(ctx, src + index * 4u, 4u); ++index)
    {
      ctx->palette_entries[index] = vm_read_u32_le(ctx->mem + src + index * 4u);
    }
  }

  ctx->regs[VM_REG_R0] = 0u;

  MVM_LOG_D(ctx,
            "palette",
            "vSetPalette(src=%08X count=%u) copied=%u\n",
            src,
            count,
            index);

  return true;
} /* End of vSetPalette */

/**
 * @brief SDK: Returns one palette entry value.
 * Call model: `sync/result`
 * Ownership: Uses one scalar palette index only during the call.
 * Blocking: Non-blocking.
 * Status: Implemented as VM-side palette state lookup.
 */
MVM_IMPORT_IMPL(vGetPaletteEntry)
{
  uint32_t index;

  index = ctx->regs[VM_REG_P0] & 0xFFu;
  ctx->regs[VM_REG_R0] = ctx->palette_entries[index];

  MVM_LOG_D(ctx,
            "palette-get",
            "vGetPaletteEntry(index=%u) -> %08X\n",
            index,
            ctx->regs[VM_REG_R0]);

  return true;
} /* End of vGetPaletteEntry */

/**
 * @brief SDK: Draws one filled rectangle using the current foreground color.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses scalar coordinates only during the call.
 * Blocking: Non-blocking.
 * Status: Implemented as one validated draw request in the default integration.
 */
MVM_IMPORT_IMPL(vFillRect)
{
  MVM_DrawCommand_t *command;
  int16_t x0;
  int16_t y0;
  int16_t x1;
  int16_t y1;

  command = NULL;
  x0 = (int16_t)(ctx->regs[VM_REG_P0] & 0xFFFFu);
  y0 = (int16_t)(ctx->regs[VM_REG_P1] & 0xFFFFu);
  x1 = (int16_t)(ctx->regs[VM_REG_P2] & 0xFFFFu);
  y1 = (int16_t)(ctx->regs[VM_REG_P3] & 0xFFFFu);
  ctx->regs[VM_REG_R0] = 0u;
  command = MVM_lAllocDrawCommand(ctx, MVM_DRAW_FILL_RECT);
  if (command)
  {
    command->x0 = x0;
    command->y0 = y0;
    command->x1 = x1;
    command->y1 = y1;
    command->color = ctx->fg_color;
  }

  MVM_LOG_D(ctx,
            "fill-rect",
            "vFillRect(%d,%d,%d,%d fg=%08X clip=%u,%u,%u,%u)\n",
            (int32_t)x0,
            (int32_t)y0,
            (int32_t)x1,
            (int32_t)y1,
            ctx->fg_color,
            (uint32_t)ctx->clip_x0,
            (uint32_t)ctx->clip_y0,
            (uint32_t)ctx->clip_x1,
            (uint32_t)ctx->clip_y1);

  return true;
} /* End of vFillRect */

/**
 * @brief SDK: Draws one line primitive using the current foreground color.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses scalar coordinates only during the call.
 * Blocking: Non-blocking.
 * Status: Implemented as one validated draw request in the default integration.
 */
MVM_IMPORT_IMPL(vDrawLine)
{
  MVM_DrawCommand_t *command;
  int16_t x0;
  int16_t y0;
  int16_t x1;
  int16_t y1;

  command = NULL;
  x0 = (int16_t)(ctx->regs[VM_REG_P0] & 0xFFFFu);
  y0 = (int16_t)(ctx->regs[VM_REG_P1] & 0xFFFFu);
  x1 = (int16_t)(ctx->regs[VM_REG_P2] & 0xFFFFu);
  y1 = (int16_t)(ctx->regs[VM_REG_P3] & 0xFFFFu);
  ctx->regs[VM_REG_R0] = 0u;
  command = MVM_lAllocDrawCommand(ctx, MVM_DRAW_LINE);
  if (command)
  {
    command->x0 = x0;
    command->y0 = y0;
    command->x1 = x1;
    command->y1 = y1;
    command->color = ctx->fg_color;
  }

  MVM_LOG_D(ctx,
            "draw-line",
            "vDrawLine(%d,%d -> %d,%d fg=%08X)\n",
            (int32_t)x0,
            (int32_t)y0,
            (int32_t)x1,
            (int32_t)y1,
            ctx->fg_color);

  return true;
} /* End of vDrawLine */

/**
 * @brief SDK: Draws one sprite object at one screen position.
 * Call model: `sync/fire-and-forget`
 * Ownership: Reads one guest SPRITE header during the call only and retains no native pointer.
 * Blocking: Non-blocking.
 * Status: Implemented as one validated sprite-draw request in the default integration.
 */
MVM_IMPORT_IMPL(vDrawObject)
{
  MVM_DrawCommand_t *command;
  int16_t x;
  int16_t y;
  uint32_t sprite_addr;
  uint16_t width;
  uint16_t height;

  command = NULL;
  x = (int16_t)(ctx->regs[VM_REG_P0] & 0xFFFFu);
  y = (int16_t)(ctx->regs[VM_REG_P1] & 0xFFFFu);
  sprite_addr = ctx->regs[VM_REG_P2];
  width = 0u;
  height = 0u;
  ctx->regs[VM_REG_R0] = 0u;

  if (!MVM_lReadSpriteHeader(ctx, sprite_addr, &width, &height))
  {
    MVM_LOG_W(ctx,
              "draw-object",
              "vDrawObject(x=%d y=%d sprite=%08X) invalid sprite\n",
              (int32_t)x,
              (int32_t)y,
              sprite_addr);

    return true;
  }

  command = MVM_lAllocDrawCommand(ctx, MVM_DRAW_SPRITE);
  if (command)
  {
    command->x0 = x;
    command->y0 = y;
    command->width = width;
    command->height = height;
    command->color = ctx->fg_color;
    command->aux = sprite_addr;
  }

  MVM_LOG_D(ctx,
            "draw-object",
            "vDrawObject(x=%d y=%d sprite=%08X w=%u h=%u mode=%08X)\n",
            (int32_t)x,
            (int32_t)y,
            sprite_addr,
            (uint32_t)width,
            (uint32_t)height,
            ctx->transfer_mode);

  return true;
} /* End of vDrawObject */

/**
 * @brief SDK: Draws one text string using the current font and color state.
 * Call model: `sync/fire-and-forget`
 * Ownership: Reads guest memory only for the duration of the call and does not retain the string pointer.
 * Blocking: Non-blocking.
 * Status: Partial; validates and logs the text request but does not rasterize glyphs.
 */
MVM_IMPORT_IMPL(vPrint)
{
  MVM_DrawCommand_t *command;
  uint32_t mode;
  uint32_t x;
  uint32_t y;
  uint32_t str;
  uint32_t length;

  command = NULL;
  mode = ctx->regs[VM_REG_P0];
  x = ctx->regs[VM_REG_P1];
  y = ctx->regs[VM_REG_P2];
  str = ctx->regs[VM_REG_P3];
  length = 0u;

  if (str < ctx->mem_size)
  {
    length = MVM_RuntimeStrLen(ctx->mem + str, ctx->mem_size - str);
  }

  ctx->regs[VM_REG_R0] = 0u;
  command = MVM_lAllocDrawCommand(ctx, MVM_DRAW_TEXT);
  if (command)
  {
    command->x0 = (int16_t)(x & 0xFFFFu);
    command->y0 = (int16_t)(y & 0xFFFFu);
    command->width = (uint16_t)(length * 6u);
    command->height = 8u;
    command->color = ctx->fg_color;
    command->aux = str;
    command->aux2 = ctx->active_font;
  }

  MVM_LOG_D(ctx,
            "print",
            "vPrint(mode=%08X x=%u y=%u str=%08X len=%u font=%08X fg=%08X bg=%08X)\n",
            mode,
            x,
            y,
            str,
            length,
            ctx->active_font,
            ctx->fg_color,
            ctx->bg_color);

  return true;
} /* End of vPrint */

/**
 * @brief SDK: Returns the current button bit-mask.
 * Call model: `polling`
 * Ownership: No guest memory access.
 * Blocking: Non-blocking.
 * Status: Partial; returns VM-side button state, currently host-updated only by future platform integration.
 */
MVM_IMPORT_IMPL(vGetButtonData)
{
  ctx->regs[VM_REG_R0] = ctx->button_state;

  MVM_LOG_D(ctx,
            "button-data",
            "vGetButtonData() -> %08X\n",
            ctx->regs[VM_REG_R0]);

  return true;
} /* End of vGetButtonData */

/**
 * @brief SDK: Tests one key code against the current button state.
 * Call model: `polling`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Implemented for the documented ASCII keypad aliases and the
 *         SonyEricsson option key used by the reference runtimes. Other
 *         implementation-defined key codes currently return not pressed.
 */
MVM_IMPORT_IMPL(vTestKey)
{
  uint32_t key;
  uint32_t pressed;

  key = ctx->regs[VM_REG_P0];
  pressed = 0u;

  switch (key)
  {
    case '1':
      pressed = ((ctx->button_state & (MVM_KEY_UP_MASK | MVM_KEY_LEFT_MASK))
                 == (MVM_KEY_UP_MASK | MVM_KEY_LEFT_MASK)) ? 1u : 0u;
      break;

    case '2':
      pressed = ((ctx->button_state & MVM_KEY_UP_MASK) != 0u) ? 1u : 0u;
      break;

    case '3':
      pressed = ((ctx->button_state & (MVM_KEY_UP_MASK | MVM_KEY_RIGHT_MASK))
                 == (MVM_KEY_UP_MASK | MVM_KEY_RIGHT_MASK)) ? 1u : 0u;
      break;

    case '4':
      pressed = ((ctx->button_state & MVM_KEY_LEFT_MASK) != 0u) ? 1u : 0u;
      break;

    case '5':
    case '*':
      pressed = ((ctx->button_state & MVM_KEY_FIRE_MASK) != 0u) ? 1u : 0u;
      break;

    case '#':
      pressed = ((ctx->button_state & MVM_KEY_FIRE2_MASK) != 0u) ? 1u : 0u;
      break;

    case '0':
    case MVM_SE_OPTION_ASCII:
      pressed = ((ctx->button_state & MVM_KEY_SELECT_MASK) != 0u) ? 1u : 0u;
      break;

    case '6':
      pressed = ((ctx->button_state & MVM_KEY_RIGHT_MASK) != 0u) ? 1u : 0u;
      break;

    case '7':
      pressed = ((ctx->button_state & (MVM_KEY_DOWN_MASK | MVM_KEY_LEFT_MASK))
                 == (MVM_KEY_DOWN_MASK | MVM_KEY_LEFT_MASK)) ? 1u : 0u;
      break;

    case '8':
      pressed = ((ctx->button_state & MVM_KEY_DOWN_MASK) != 0u) ? 1u : 0u;
      break;

    case '9':
      pressed = ((ctx->button_state & (MVM_KEY_DOWN_MASK | MVM_KEY_RIGHT_MASK))
                 == (MVM_KEY_DOWN_MASK | MVM_KEY_RIGHT_MASK)) ? 1u : 0u;
      break;

    default:
      pressed = 0u;
      break;
  }

  ctx->regs[VM_REG_R0] = pressed;

  MVM_LOG_D(ctx,
            "test-key",
            "vTestKey(key=%08X) -> %u\n",
            key,
            ctx->regs[VM_REG_R0]);

  return true;
} /* End of vTestKey */

/**
 * @brief SDK: Displays one message box style prompt and returns one button/result code.
 * Call model: `sync/result`
 * Ownership: Reads guest message/title strings during the call only and retains no host pointer.
 * Blocking: Non-blocking in the default integration.
 * Status: Implemented with one log-only acknowledgement fallback that returns `1`.
 */
MVM_IMPORT_IMPL(vMsgBox)
{
  uint32_t flags;
  uint32_t message_addr;
  uint32_t title_addr;
  uint32_t message_len;
  uint32_t title_len;

  flags = ctx->regs[VM_REG_P0];
  message_addr = ctx->regs[VM_REG_P1];
  title_addr = ctx->regs[VM_REG_P2];
  message_len = 0u;
  title_len = 0u;

  if (message_addr < ctx->mem_size)
  {
    message_len = MVM_RuntimeStrLen(ctx->mem + message_addr, ctx->mem_size - message_addr);
  }

  if (title_addr < ctx->mem_size)
  {
    title_len = MVM_RuntimeStrLen(ctx->mem + title_addr, ctx->mem_size - title_addr);
  }

  ctx->regs[VM_REG_R0] = 1u;

  MVM_LOG_I(ctx,
            "msgbox",
            "vMsgBox(flags=%08X msg=%08X len=%u title=%08X title_len=%u) -> %u\n",
            flags,
            message_addr,
            message_len,
            title_addr,
            title_len,
            ctx->regs[VM_REG_R0]);

  return true;
} /* End of vMsgBox */

/**
 * @brief SDK: Performs one system control request.
 * Call model: `sync/result`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking in the default integration.
 * Status: Implemented with one generic success fallback.
 */
MVM_IMPORT_IMPL(vSysCtl)
{
  uint32_t cmd;
  uint32_t op;

  cmd = ctx->regs[VM_REG_P0];
  op = ctx->regs[VM_REG_P1];
  ctx->regs[VM_REG_R0] = 1u;

  MVM_LOG_D(ctx,
            "sysctl",
            "vSysCtl(cmd=%08X op=%08X) -> %u\n",
            cmd,
            op,
            ctx->regs[VM_REG_R0]);

  return true;
} /* End of vSysCtl */

/**
 * @brief SDK: Initializes the sprite slot table.
 * Call model: `sync/result`
 * Ownership: Uses one scalar slot count only during the call.
 * Blocking: Non-blocking.
 * Status: Implemented with one fixed VM-side sprite table.
 */
MVM_IMPORT_IMPL(vSpriteInit)
{
  uint32_t count;
  uint32_t index;

  count = ctx->regs[VM_REG_P0] & 0xFFu;

  if (count > VMGP_MAX_SPRITE_SLOTS)
  {
    ctx->regs[VM_REG_R0] = 0u;

    MVM_LOG_W(ctx,
              "sprite-init",
              "vSpriteInit(count=%u) exceeds max=%u\n",
              count,
              (uint32_t)VMGP_MAX_SPRITE_SLOTS);

    return true;
  }

  for (index = 0u; index < count; ++index)
  {
    ctx->sprite_slots[index].used = false;
    ctx->sprite_slots[index].sprite_addr = 0u;
    ctx->sprite_slots[index].x = 0;
    ctx->sprite_slots[index].y = 0;
  }

  ctx->sprite_slot_count = count;
  ctx->regs[VM_REG_R0] = 1u;

  MVM_LOG_D(ctx,
            "sprite-init",
            "vSpriteInit(count=%u) -> %u\n",
            count,
            ctx->regs[VM_REG_R0]);

  return true;
} /* End of vSpriteInit */

/**
 * @brief SDK: Releases the current sprite slot table contents.
 * Call model: `sync/fire-and-forget`
 * Ownership: Clears VM-side slot state only; guest sprite data remains guest-owned.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vSpriteDispose)
{
  uint32_t index;

  for (index = 0u; index < ctx->sprite_slot_count && index < VMGP_MAX_SPRITE_SLOTS; ++index)
  {
    ctx->sprite_slots[index].used = false;
    ctx->sprite_slots[index].sprite_addr = 0u;
    ctx->sprite_slots[index].x = 0;
    ctx->sprite_slots[index].y = 0;
  }

  ctx->sprite_slot_count = 0u;
  ctx->regs[VM_REG_R0] = 0u;

  MVM_LOG_D(ctx, "sprite-dispose", "vSpriteDispose()\n");

  return true;
} /* End of vSpriteDispose */

/**
 * @brief SDK: Assigns one sprite object to one sprite slot.
 * Call model: `sync/fire-and-forget`
 * Ownership: Stores one guest sprite pointer as VM-side state but does not retain one native pointer.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vSpriteSet)
{
  uint32_t slot;
  uint32_t sprite_addr;
  int16_t x;
  int16_t y;

  slot = ctx->regs[VM_REG_P0] & 0xFFu;
  sprite_addr = ctx->regs[VM_REG_P1];
  x = (int16_t)(ctx->regs[VM_REG_P2] & 0xFFFFu);
  y = (int16_t)(ctx->regs[VM_REG_P3] & 0xFFFFu);

  if (slot >= ctx->sprite_slot_count || slot >= VMGP_MAX_SPRITE_SLOTS)
  {
    MVM_LOG_W(ctx,
              "sprite-set",
              "vSpriteSet(slot=%u sprite=%08X x=%d y=%d) out of range\n",
              slot,
              sprite_addr,
              (int32_t)x,
              (int32_t)y);
    ctx->regs[VM_REG_R0] = 0u;

    return true;
  }

  ctx->sprite_slots[slot].used = (sprite_addr != 0u);
  ctx->sprite_slots[slot].sprite_addr = sprite_addr;
  ctx->sprite_slots[slot].x = x;
  ctx->sprite_slots[slot].y = y;
  ctx->regs[VM_REG_R0] = 0u;

  MVM_LOG_D(ctx,
            "sprite-set",
            "vSpriteSet(slot=%u sprite=%08X x=%d y=%d)\n",
            slot,
            sprite_addr,
            (int32_t)x,
            (int32_t)y);

  return true;
} /* End of vSpriteSet */

/**
 * @brief SDK: Checks one sprite slot against a slot range and returns the first colliding slot.
 * Call model: `sync/result`
 * Ownership: Reads guest sprite headers during the call only.
 * Blocking: Non-blocking.
 * Status: Implemented with axis-aligned bounding-box collision.
 */
MVM_IMPORT_IMPL(vSpriteCollision)
{
  uint32_t slot_check;
  uint32_t slot_from;
  uint32_t slot_to;
  uint16_t check_width;
  uint16_t check_height;
  uint16_t other_width;
  uint16_t other_height;
  uint32_t index;

  slot_check = ctx->regs[VM_REG_P0] & 0xFFu;
  slot_from = ctx->regs[VM_REG_P1] & 0xFFu;
  slot_to = ctx->regs[VM_REG_P2] & 0xFFu;
  ctx->regs[VM_REG_R0] = 0xFFFFFFFFu;

  if (slot_check >= ctx->sprite_slot_count ||
      !ctx->sprite_slots[slot_check].used ||
      !MVM_lReadSpriteHeader(ctx, ctx->sprite_slots[slot_check].sprite_addr, &check_width, &check_height))
  {
    return true;
  }

  if (slot_to >= ctx->sprite_slot_count)
  {
    slot_to = (ctx->sprite_slot_count > 0u) ? (ctx->sprite_slot_count - 1u) : 0u;
  }

  for (index = slot_from; index <= slot_to; ++index)
  {
    if (index == slot_check ||
        index >= ctx->sprite_slot_count ||
        !ctx->sprite_slots[index].used ||
        !MVM_lReadSpriteHeader(ctx, ctx->sprite_slots[index].sprite_addr, &other_width, &other_height))
    {
      continue;
    }

    if (MVM_lRectanglesOverlap(ctx->sprite_slots[slot_check].x,
                               ctx->sprite_slots[slot_check].y,
                               check_width,
                               check_height,
                               ctx->sprite_slots[index].x,
                               ctx->sprite_slots[index].y,
                               other_width,
                               other_height))
    {
      ctx->regs[VM_REG_R0] = index;
      break;
    }
  }

  MVM_LOG_D(ctx,
            "sprite-collision",
            "vSpriteCollision(slot=%u from=%u to=%u) -> %d\n",
            slot_check,
            slot_from,
            slot_to,
            (int32_t)ctx->regs[VM_REG_R0]);

  return true;
} /* End of vSpriteCollision */

/**
 * @brief SDK: Checks one box against a sprite-slot range and returns the first colliding slot.
 * Call model: `sync/result`
 * Ownership: Reads one guest rectangle and guest sprite headers during the call only.
 * Blocking: Non-blocking.
 * Status: Implemented with axis-aligned bounding-box collision.
 */
MVM_IMPORT_IMPL(vSpriteBoxCollision)
{
  uint32_t box_addr;
  uint32_t slot_from;
  uint32_t slot_to;
  int16_t box_x;
  int16_t box_y;
  uint16_t box_w;
  uint16_t box_h;
  uint16_t sprite_width;
  uint16_t sprite_height;
  uint32_t index;

  box_addr = ctx->regs[VM_REG_P0];
  slot_from = ctx->regs[VM_REG_P1] & 0xFFu;
  slot_to = ctx->regs[VM_REG_P2] & 0xFFu;
  ctx->regs[VM_REG_R0] = 0xFFFFFFFFu;

  if (!MVM_RuntimeMemRangeOk(ctx, box_addr, 8u))
  {
    return true;
  }

  box_x = (int16_t)vm_read_u16_le(ctx->mem + box_addr + 0u);
  box_y = (int16_t)vm_read_u16_le(ctx->mem + box_addr + 2u);
  box_w = vm_read_u16_le(ctx->mem + box_addr + 4u);
  box_h = vm_read_u16_le(ctx->mem + box_addr + 6u);

  if (slot_to >= ctx->sprite_slot_count)
  {
    slot_to = (ctx->sprite_slot_count > 0u) ? (ctx->sprite_slot_count - 1u) : 0u;
  }

  for (index = slot_from; index <= slot_to; ++index)
  {
    if (index >= ctx->sprite_slot_count ||
        !ctx->sprite_slots[index].used ||
        !MVM_lReadSpriteHeader(ctx, ctx->sprite_slots[index].sprite_addr, &sprite_width, &sprite_height))
    {
      continue;
    }

    if (MVM_lRectanglesOverlap(box_x,
                               box_y,
                               box_w,
                               box_h,
                               ctx->sprite_slots[index].x,
                               ctx->sprite_slots[index].y,
                               sprite_width,
                               sprite_height))
    {
      ctx->regs[VM_REG_R0] = index;
      break;
    }
  }

  MVM_LOG_D(ctx,
            "sprite-box-collision",
            "vSpriteBoxCollision(box=%08X from=%u to=%u) -> %d\n",
            box_addr,
            slot_from,
            slot_to,
            (int32_t)ctx->regs[VM_REG_R0]);

  return true;
} /* End of vSpriteBoxCollision */

/**
 * @brief SDK: Draws all configured sprite slots.
 * Call model: `sync/fire-and-forget`
 * Ownership: Reads VM-side slot metadata only during the call.
 * Blocking: Non-blocking.
 * Status: Partial; logs the update request and active slot count but does not render.
 */
MVM_IMPORT_IMPL(vUpdateSprite)
{
  uint32_t visible_count;
  uint32_t index;

  visible_count = 0u;

  for (index = 0u; index < ctx->sprite_slot_count; ++index)
  {
    if (ctx->sprite_slots[index].used)
    {
      ++visible_count;
    }
  }

  ctx->regs[VM_REG_R0] = 0u;

  MVM_LOG_D(ctx,
            "sprite-update",
            "vUpdateSprite(active=%u total=%u)\n",
            visible_count,
            ctx->sprite_slot_count);

  return true;
} /* End of vUpdateSprite */

/**
 * @brief SDK: Initializes one tilemap definition.
 * Call model: `sync/result`
 * Ownership: Copies one guest MAP_HEADER snapshot into VM-side state and retains guest data pointers symbolically only.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vMapInit)
{
  uint32_t header_addr;
  bool is_valid;

  header_addr = ctx->regs[VM_REG_P0];
  is_valid = MVM_lReadMapHeader(ctx, header_addr, &ctx->map_state);

  if (!is_valid)
  {
    memset(&ctx->map_state, 0, sizeof(ctx->map_state));
  }

  ctx->regs[VM_REG_R0] = is_valid ? 1u : 0u;

  MVM_LOG_D(ctx,
            "map-init",
            "vMapInit(map=%08X) -> %u width=%u height=%u flags=%02X data=%08X\n",
            header_addr,
            ctx->regs[VM_REG_R0],
            (uint32_t)ctx->map_state.width,
            (uint32_t)ctx->map_state.height,
            (uint32_t)ctx->map_state.flags,
            ctx->map_state.map_data_addr);

  return true;
} /* End of vMapInit */

/**
 * @brief SDK: Releases the current tilemap state.
 * Call model: `sync/fire-and-forget`
 * Ownership: Clears VM-side map state only.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vMapDispose)
{
  memset(&ctx->map_state, 0, sizeof(ctx->map_state));
  ctx->regs[VM_REG_R0] = 0u;

  MVM_LOG_D(ctx, "map-dispose", "vMapDispose()\n");

  return true;
} /* End of vMapDispose */

/**
 * @brief SDK: Changes the current map scroll offset.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Implemented as VM-side tilemap state.
 */
MVM_IMPORT_IMPL(vMapSetXY)
{
  ctx->map_state.x_pos = (int16_t)(ctx->regs[VM_REG_P0] & 0xFFFFu);
  ctx->map_state.y_pos = (int16_t)(ctx->regs[VM_REG_P1] & 0xFFFFu);
  ctx->regs[VM_REG_R0] = 0u;

  MVM_LOG_D(ctx,
            "map-set-xy",
            "vMapSetXY(x=%d y=%d)\n",
            (int32_t)ctx->map_state.x_pos,
            (int32_t)ctx->map_state.y_pos);

  return true;
} /* End of vMapSetXY */

/**
 * @brief SDK: Changes one tile value in the active tilemap.
 * Call model: `sync/fire-and-forget`
 * Ownership: Writes guest tilemap memory during the call only.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vMapSetTile)
{
  uint32_t x;
  uint32_t y;
  uint32_t tile;
  uint8_t *cell;
  uint32_t addr;

  x = ctx->regs[VM_REG_P0] & 0xFFu;
  y = ctx->regs[VM_REG_P1] & 0xFFu;
  tile = ctx->regs[VM_REG_P2] & 0xFFu;
  cell = MVM_lMapCellPtr(ctx, &ctx->map_state, x, y);

  if (cell)
  {
    addr = ctx->map_state.map_data_addr +
           ((y * (uint32_t)ctx->map_state.width) + x) * MVM_lMapCellStride(&ctx->map_state);
    cell[0] = (uint8_t)tile;
    MVM_WatchMemoryWrite(ctx, addr, 1u, "vMapSetTile");
  }

  ctx->regs[VM_REG_R0] = 0u;

  MVM_LOG_D(ctx,
            "map-set-tile",
            "vMapSetTile(x=%u y=%u tile=%u ok=%u)\n",
            x,
            y,
            tile,
            cell ? 1u : 0u);

  return true;
} /* End of vMapSetTile */

/**
 * @brief SDK: Returns one tile attribute value from the active tilemap.
 * Call model: `sync/result`
 * Ownership: Reads guest tilemap memory during the call only.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vMapGetAttribute)
{
  uint32_t x;
  uint32_t y;
  uint8_t *cell;

  x = ctx->regs[VM_REG_P0] & 0xFFu;
  y = ctx->regs[VM_REG_P1] & 0xFFu;
  cell = MVM_lMapCellPtr(ctx, &ctx->map_state, x, y);
  ctx->regs[VM_REG_R0] = 0u;

  if (cell && MVM_lMapCellStride(&ctx->map_state) > 1u)
  {
    ctx->regs[VM_REG_R0] = cell[1];
  }

  MVM_LOG_D(ctx,
            "map-get-attr",
            "vMapGetAttribute(x=%u y=%u) -> %u\n",
            x,
            y,
            ctx->regs[VM_REG_R0]);

  return true;
} /* End of vMapGetAttribute */

/**
 * @brief SDK: Draws the active tilemap.
 * Call model: `sync/fire-and-forget`
 * Ownership: Reads VM-side map state only during the call.
 * Blocking: Non-blocking.
 * Status: Partial; logs the update request but does not render.
 */
MVM_IMPORT_IMPL(vUpdateMap)
{
  ctx->regs[VM_REG_R0] = 0u;

  MVM_LOG_D(ctx,
            "map-update",
            "vUpdateMap(valid=%u pos=%d,%d size=%ux%u data=%08X)\n",
            ctx->map_state.valid ? 1u : 0u,
            (int32_t)ctx->map_state.x_pos,
            (int32_t)ctx->map_state.y_pos,
            (uint32_t)ctx->map_state.width,
            (uint32_t)ctx->map_state.height,
            ctx->map_state.map_data_addr);

  return true;
} /* End of vUpdateMap */

/**
 * @brief SDK: Draws the active tilemap and sprite table in one combined pass.
 * Call model: `sync/fire-and-forget`
 * Ownership: Reads VM-side map and sprite state only during the call.
 * Blocking: Non-blocking.
 * Status: Partial; chains the current map and sprite update handlers.
 */
MVM_IMPORT_IMPL(vUpdateSpriteMap)
{
  vUpdateMap(ctx);
  vUpdateSprite(ctx);
  ctx->regs[VM_REG_R0] = 0u;

  MVM_LOG_D(ctx, "sprite-map-update", "vUpdateSpriteMap()\n");

  return true;
} /* End of vUpdateSpriteMap */

/**
 * @brief SDK: Opens one resource stream and returns one handle in `r0`.
 * Call model: `sync/result`
 * Ownership: Uses guest arguments only for the duration of the call.
 * Blocking: Non-blocking.
 * Status: Implemented as an alias of `vStreamOpen()`.
 */
MVM_IMPORT_IMPL(vResOpen)
{
  return vStreamOpen(ctx);
} /* End of vResOpen */

/**
 * @brief SDK: Opens one resource stream with mode flags and returns one handle in `r0`.
 * Call model: `sync/result`
 * Ownership: Uses guest arguments only for the duration of the call.
 * Blocking: Non-blocking.
 * Status: Implemented as an alias of `vStreamOpen()`.
 */
MVM_IMPORT_IMPL(vResOpenMode)
{
  return vStreamOpen(ctx);
} /* End of vResOpenMode */

/**
 * @brief SDK: Reads bytes from one open resource stream.
 * Call model: `sync/result`
 * Ownership: Writes guest memory during the call only.
 * Blocking: Non-blocking.
 * Status: Implemented as an alias of `vStreamRead()`.
 */
MVM_IMPORT_IMPL(vResRead)
{
  return vStreamRead(ctx);
} /* End of vResRead */

/**
 * @brief SDK: Repositions one open resource stream.
 * Call model: `sync/result`
 * Ownership: Uses one VM-owned stream handle only for the duration of the call.
 * Blocking: Non-blocking.
 * Status: Implemented as an alias of `vStreamSeek()`.
 */
MVM_IMPORT_IMPL(vResSeek)
{
  return vStreamSeek(ctx);
} /* End of vResSeek */

/**
 * @brief SDK: Closes one resource stream.
 * Call model: `sync/fire-and-forget`
 * Ownership: Releases one VM-owned stream slot; no host ownership is retained.
 * Blocking: Non-blocking.
 * Status: Implemented as an alias of `vStreamClose()`.
 */
MVM_IMPORT_IMPL(vResClose)
{
  return vStreamClose(ctx);
} /* End of vResClose */

/**
 * @brief SDK: Opens one file and returns one handle in `r0`.
 * Call model: `sync/result`
 * Ownership: Uses guest arguments only for the duration of the call.
 * Blocking: Non-blocking.
 * Status: Implemented as an alias of `vStreamOpen()`.
 */
MVM_IMPORT_IMPL(vFileOpen)
{
  return vStreamOpen(ctx);
} /* End of vFileOpen */

/**
 * @brief SDK: Creates or truncates one file and returns one handle in `r0`.
 * Call model: `sync/result`
 * Ownership: Uses guest arguments only for the duration of the call.
 * Blocking: Non-blocking.
 * Status: Implemented as one file-macro alias over `vStreamOpen()`.
 */
MVM_IMPORT_IMPL(vFileCreate)
{
  ctx->regs[VM_REG_P1] = STREAM_WRITE_FLAG | STREAM_CREATE_FLAG | STREAM_TRUNC_FLAG;
  return vStreamOpen(ctx);
} /* End of vFileCreate */

/**
 * @brief SDK: Closes one file handle.
 * Call model: `sync/fire-and-forget`
 * Ownership: Releases one VM-owned stream slot; no host ownership is retained.
 * Blocking: Non-blocking.
 * Status: Implemented as an alias of `vStreamClose()`.
 */
MVM_IMPORT_IMPL(vFileClose)
{
  return vStreamClose(ctx);
} /* End of vFileClose */

/**
 * @brief SDK: Reads bytes from one open file handle.
 * Call model: `sync/result`
 * Ownership: Writes guest memory during the call only.
 * Blocking: Non-blocking.
 * Status: Implemented as an alias of `vStreamRead()`.
 */
MVM_IMPORT_IMPL(vFileRead)
{
  return vStreamRead(ctx);
} /* End of vFileRead */

/**
 * @brief SDK: Writes bytes to one open file handle.
 * Call model: `sync/result`
 * Ownership: Reads guest memory during the call only.
 * Blocking: Non-blocking.
 * Status: Partial; currently aliases `vStreamWrite()`, which remains stubbed.
 */
MVM_IMPORT_IMPL(vFileWrite)
{
  return vStreamWrite(ctx);
} /* End of vFileWrite */

/**
 * @brief SDK: Repositions one open file handle.
 * Call model: `sync/result`
 * Ownership: Uses one VM-owned stream handle only for the duration of the call.
 * Blocking: Non-blocking.
 * Status: Implemented as an alias of `vStreamSeek()`.
 */
MVM_IMPORT_IMPL(vFileSeek)
{
  return vStreamSeek(ctx);
} /* End of vFileSeek */

/**
 * @brief SDK: Deletes one file.
 * Call model: `sync/result`
 * Ownership: Uses guest arguments only for the duration of the call.
 * Blocking: Non-blocking.
 * Status: Partial; currently mapped to the documented `vStreamOpen(..., STREAM_DELETE)` file macro.
 */
MVM_IMPORT_IMPL(vFileDelete)
{
  ctx->regs[VM_REG_P1] = STREAM_DELETE_FLAG;
  return vStreamOpen(ctx);
} /* End of vFileDelete */

/**
 * @brief SDK: Calculates sine for one fixed-angle value where a full turn is `0x1000`.
 * Call model: `sync/result`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vSin)
{
  double radians;

  radians = ((double)vm_reg_s32(ctx->regs[VM_REG_P0]) / 4096.0) * (2.0 * M_PI);
  ctx->regs[VM_REG_R0] = (uint32_t)MVM_lFixedFromDouble(sin(radians));

  MVM_LOG_D(ctx,
            "math-sin",
            "vSin(angle=%08X) -> %08X\n",
            ctx->regs[VM_REG_P0],
            ctx->regs[VM_REG_R0]);

  return true;
} /* End of vSin */

/**
 * @brief SDK: Calculates cosine for one fixed-angle value where a full turn is `0x1000`.
 * Call model: `sync/result`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vCos)
{
  double radians;

  radians = ((double)vm_reg_s32(ctx->regs[VM_REG_P0]) / 4096.0) * (2.0 * M_PI);
  ctx->regs[VM_REG_R0] = (uint32_t)MVM_lFixedFromDouble(cos(radians));

  MVM_LOG_D(ctx,
            "math-cos",
            "vCos(angle=%08X) -> %08X\n",
            ctx->regs[VM_REG_P0],
            ctx->regs[VM_REG_R0]);

  return true;
} /* End of vCos */

/**
 * @brief SDK: Calculates tangent for one fixed-angle value where a full turn is `0x1000`.
 * Call model: `sync/result`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vTan)
{
  double radians;

  radians = ((double)vm_reg_s32(ctx->regs[VM_REG_P0]) / 4096.0) * (2.0 * M_PI);
  ctx->regs[VM_REG_R0] = (uint32_t)MVM_lFixedFromDouble(tan(radians));

  MVM_LOG_D(ctx,
            "math-tan",
            "vTan(angle=%08X) -> %08X\n",
            ctx->regs[VM_REG_P0],
            ctx->regs[VM_REG_R0]);

  return true;
} /* End of vTan */

/**
 * @brief SDK: Calculates square root for one fixed-point value.
 * Call model: `sync/result`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vSqrt)
{
  double value;

  value = MVM_lFixedToDouble(vm_reg_s32(ctx->regs[VM_REG_P0]));

  if (value < 0.0)
  {
    value = 0.0;
  }

  ctx->regs[VM_REG_R0] = (uint32_t)MVM_lFixedFromDouble(sqrt(value));

  MVM_LOG_D(ctx,
            "math-sqrt",
            "vSqrt(value=%08X) -> %08X\n",
            ctx->regs[VM_REG_P0],
            ctx->regs[VM_REG_R0]);

  return true;
} /* End of vSqrt */

/**
 * @brief SDK: Raises one fixed-point value to an integer exponent.
 * Call model: `sync/result`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vPow)
{
  double value;
  uint32_t exponent;

  value = MVM_lFixedToDouble(vm_reg_s32(ctx->regs[VM_REG_P0]));
  exponent = ctx->regs[VM_REG_P1] & 0xFFu;
  ctx->regs[VM_REG_R0] = (uint32_t)MVM_lFixedFromDouble(pow(value, (double)exponent));

  MVM_LOG_D(ctx,
            "math-pow",
            "vPow(value=%08X exp=%u) -> %08X\n",
            ctx->regs[VM_REG_P0],
            exponent,
            ctx->regs[VM_REG_R0]);

  return true;
} /* End of vPow */

/**
 * @brief SDK: Divides one fixed-point value by another.
 * Call model: `sync/result`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vDiv)
{
  int32_t left;
  int32_t right;
  int32_t result;

  left = vm_reg_s32(ctx->regs[VM_REG_P0]);
  right = vm_reg_s32(ctx->regs[VM_REG_P1]);
  result = 0;

  if (right != 0)
  {
    result = (int32_t)(((int64_t)left << 14) / right);
  }

  ctx->regs[VM_REG_R0] = (uint32_t)result;

  MVM_LOG_D(ctx,
            "math-div",
            "vDiv(left=%08X right=%08X) -> %08X\n",
            ctx->regs[VM_REG_P0],
            ctx->regs[VM_REG_P1],
            ctx->regs[VM_REG_R0]);

  return true;
} /* End of vDiv */

/**
 * @brief SDK: Multiplies one fixed-point value by another.
 * Call model: `sync/result`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vMul)
{
  int32_t left;
  int32_t right;
  int32_t result;

  left = vm_reg_s32(ctx->regs[VM_REG_P0]);
  right = vm_reg_s32(ctx->regs[VM_REG_P1]);
  result = (int32_t)(((int64_t)left * (int64_t)right) >> 14);
  ctx->regs[VM_REG_R0] = (uint32_t)result;

  MVM_LOG_D(ctx,
            "math-mul",
            "vMul(left=%08X right=%08X) -> %08X\n",
            ctx->regs[VM_REG_P0],
            ctx->regs[VM_REG_P1],
            ctx->regs[VM_REG_R0]);

  return true;
} /* End of vMul */

/**
 * @brief SDK: Converts one 16-bit little-endian value to native-endian.
 * Call model: `sync/result`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vSwap16)
{
  uint16_t value;

  value = (uint16_t)(ctx->regs[VM_REG_P0] & 0xFFFFu);
  ctx->regs[VM_REG_R0] = (uint32_t)((value << 8) | (value >> 8));

  MVM_LOG_D(ctx,
            "swap16",
            "vSwap16(value=%04X) -> %04X\n",
            (uint32_t)value,
            ctx->regs[VM_REG_R0] & 0xFFFFu);

  return true;
} /* End of vSwap16 */

/**
 * @brief SDK: Converts one 32-bit little-endian value to native-endian.
 * Call model: `sync/result`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vSwap32)
{
  uint32_t value;

  value = ctx->regs[VM_REG_P0];
  ctx->regs[VM_REG_R0] = ((value & 0x000000FFu) << 24) |
                         ((value & 0x0000FF00u) << 8) |
                         ((value & 0x00FF0000u) >> 8) |
                         ((value & 0xFF000000u) >> 24);

  MVM_LOG_D(ctx,
            "swap32",
            "vSwap32(value=%08X) -> %08X\n",
            value,
            ctx->regs[VM_REG_R0]);

  return true;
} /* End of vSwap32 */

/**
 * @brief SDK: Converts the endian of `n` values in guest memory, each `size` bytes wide.
 * Call model: `sync/fire-and-forget`
 * Ownership: Reads and writes guest memory during the call only.
 * Blocking: Non-blocking.
 * Status: Implemented.
 */
MVM_IMPORT_IMPL(vSwap)
{
  uint32_t ptr;
  uint32_t count;
  uint32_t size;
  uint32_t index;
  uint32_t left;
  uint32_t right;
  uint8_t tmp;

  ptr = ctx->regs[VM_REG_P0];
  count = ctx->regs[VM_REG_P1];
  size = ctx->regs[VM_REG_P2];
  index = 0u;
  left = 0u;
  right = 0u;
  tmp = 0u;

  if (size == 0u || !MVM_RuntimeMemRangeOk(ctx, ptr, count * size))
  {
    ctx->regs[VM_REG_R0] = 0u;
    return true;
  }

  for (index = 0u; index < count; ++index)
  {
    for (left = 0u, right = size - 1u; left < right; ++left, --right)
    {
      tmp = ctx->mem[ptr + index * size + left];
      ctx->mem[ptr + index * size + left] = ctx->mem[ptr + index * size + right];
      ctx->mem[ptr + index * size + right] = tmp;
    }
  }

  MVM_WatchMemoryWrite(ctx, ptr, count * size, "vSwap");
  ctx->regs[VM_REG_R0] = 0u;

  MVM_LOG_D(ctx,
            "swap",
            "vSwap(ptr=%08X count=%u size=%u)\n",
            ptr,
            count,
            size);

  return true;
} /* End of vSwap */

/**
 * @brief SDK: Debug print helper.
 * Call model: `sync/fire-and-forget`
 * Ownership: No guest memory ownership is retained.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(DbgPrintf)

/**
 * @brief SDK: Emits one alert tone.
 * Call model: `sync/fire-and-forget`
 * Ownership: No guest memory ownership is retained.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vBeep)

/**
 * @brief SDK: Tests whether one box is inside the current view frustum.
 * Call model: `sync/result`
 * Ownership: Uses guest arguments only for the duration of the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vBoxInViewFrustum)

/**
 * @brief SDK: Returns the extent of one character in the active font.
 * Call model: `sync/result`
 * Ownership: Uses guest arguments only for the duration of the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vCharExtent)

/**
 * @brief SDK: Returns the extent of one Unicode character in the active font.
 * Call model: `sync/result`
 * Ownership: Uses guest arguments only for the duration of the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vCharExtentU)

/**
 * @brief SDK: Copies one screen rectangle.
 * Call model: `sync/fire-and-forget`
 * Ownership: No guest memory ownership is retained.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vCopyRect)

/**
 * @brief SDK: Draws one billboard primitive.
 * Call model: `sync/fire-and-forget`
 * Ownership: No guest memory ownership is retained.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vDrawBillboard)

/**
 * @brief SDK: Draws one flat polygon primitive.
 * Call model: `sync/fire-and-forget`
 * Ownership: No guest memory ownership is retained.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vDrawFlatPolygon)

/**
 * @brief SDK: Draws one polygon primitive.
 * Call model: `sync/fire-and-forget`
 * Ownership: No guest memory ownership is retained.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vDrawPolygon)

/**
 * @brief SDK: Draws one tile primitive.
 * Call model: `sync/fire-and-forget`
 * Ownership: No guest memory ownership is retained.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vDrawTile)

/**
 * @brief SDK: Finds the palette index for one RGB value.
 * Call model: `sync/result`
 * Ownership: No guest memory ownership is retained.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vFindRGBIndex)

/**
 * @brief SDK: Reads one pixel value from the current drawing surface.
 * Call model: `sync/result`
 * Ownership: No guest memory ownership is retained.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vGetPixel)

/**
 * @brief SDK: Reads one depth-buffer value.
 * Call model: `sync/result`
 * Ownership: No guest memory ownership is retained.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vGetZBufferValue)

/**
 * @brief SDK: Initializes the 3D subsystem.
 * Call model: `sync/result`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vInit3D)

/**
 * @brief SDK: Draws one pixel.
 * Call model: `sync/fire-and-forget`
 * Ownership: No guest memory ownership is retained.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vPlot)

/**
 * @brief SDK: Renders one non-indexed primitive batch.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses guest pointers only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vRenderPrimitive)

/**
 * @brief SDK: Renders one indexed primitive batch.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses guest pointers only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vRenderPrimitiveIndexed)

/**
 * @brief SDK: Resets the active light set.
 * Call model: `sync/fire-and-forget`
 * Ownership: No guest memory ownership is retained.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vResetLights)

/**
 * @brief SDK: Selects one font.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSelectFont)

/**
 * @brief SDK: Selects one active texture.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSetActiveTexture)

/**
 * @brief SDK: Sets the ambient light state.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSetAmbientLight)

/**
 * @brief SDK: Sets the 3D camera position.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses guest pointers or scalars only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSetCameraPos)

/**
 * @brief SDK: Sets the active display window.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSetDisplayWindow)

/**
 * @brief SDK: Sets one light source.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses guest pointers or scalars only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSetLight)

/**
 * @brief SDK: Sets one material state block.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses guest pointers or scalars only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSetMaterial)

/**
 * @brief SDK: Sets one alternate material state block.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses guest pointers or scalars only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSetMaterial2)

/**
 * @brief SDK: Selects the active matrix mode.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSetMatrixMode)

/**
 * @brief SDK: Sets the screen orientation.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSetOrientation)

/**
 * @brief SDK: Sets one 3D render state.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSetRenderState)

/**
 * @brief SDK: Sets one texture object.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses guest pointers or scalars only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSetTexture)

/**
 * @brief SDK: Sets the active viewport.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSetViewport)

/**
 * @brief SDK: Enables or configures Z-buffering.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSetZBuffer)

/**
 * @brief SDK: Clears all sprite instances.
 * Call model: `sync/fire-and-forget`
 * Ownership: No guest memory ownership is retained.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSpriteClear)

/**
 * @brief SDK: Returns the extent of one text string.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vTextExtent)

/**
 * @brief SDK: Returns the extent of one Unicode text string.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vTextExtentU)

/**
 * @brief SDK: Draws one text string.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses guest pointers only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vTextOut)

/**
 * @brief SDK: Draws one Unicode text string.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses guest pointers only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vTextOutU)

/**
 * @brief SDK: Waits for vertical blank.
 * Call model: `polling`
 * Ownership: No guest memory ownership is retained.
 * Blocking: Non-blocking in the default integration.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vWaitVBL)

/**
 * @brief SDK: Verifies one data certificate stored in memory.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vCheckDataCert)

/**
 * @brief SDK: Verifies one data certificate stored in a stream.
 * Call model: `sync/result`
 * Ownership: Uses one VM-owned stream handle only for the duration of the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vCheckDataCertFile)

/**
 * @brief SDK: Checks one IMEI string against device capabilities.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vCheckIMEI)

/**
 * @brief SDK: Checks one network identifier string against the current device.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vCheckNetwork)

/**
 * @brief SDK: Tests box-box collision.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers or scalars only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vCollisionBoxBox)

/**
 * @brief SDK: Tests point-box collision.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers or scalars only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vCollisionPointBox)

/**
 * @brief SDK: Tests vector-plane collision.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers or scalars only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vCollisionVectorPlane)

/**
 * @brief SDK: Tests vector-polygon collision.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers or scalars only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vCollisionVectorPoly)

/**
 * @brief SDK: Creates one gray-scale color value.
 * Call model: `sync/result`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vCreateGrayValue)

/**
 * @brief SDK: Creates one plane equation from polygon data.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vCreatePlaneFromPoly)

/**
 * @brief SDK: Creates one task object.
 * Call model: `async/request`
 * Ownership: Uses guest pointers or scalars only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vCreateTask)

/**
 * @brief SDK: Creates one texture object.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vCreateTexture)

/**
 * @brief SDK: Computes one vector cross product.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vCrossProduct)

/**
 * @brief SDK: Deletes all texture objects.
 * Call model: `sync/fire-and-forget`
 * Ownership: No guest memory ownership is retained.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vDeleteAllTextures)

/**
 * @brief SDK: Deletes one texture object.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vDeleteTexture)

/**
 * @brief SDK: Disposes one task object.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vDisposeTask)

/**
 * @brief SDK: Computes one vector dot product.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vDotProduct)

/**
 * @brief SDK: Frees one texture allocation.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vFreeTexture)

/**
 * @brief SDK: Returns one integer parsed from text or state.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vGetInteger)

/**
 * @brief SDK: Reads one pointer or touch position.
 * Call model: `polling`
 * Ownership: May write guest memory depending on backend contract; default stub retains nothing.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vGetPointerPos)

/**
 * @brief SDK: Returns one current time value.
 * Call model: `sync/result`
 * Ownership: No guest memory ownership is retained.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vGetTime)

/**
 * @brief SDK: Terminates one task.
 * Call model: `async/request`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vKillTask)

/**
 * @brief SDK: Computes one point-light contribution.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vLightPoint)

/**
 * @brief SDK: Queries one tile value from the active map.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers or scalars only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vMapGetTile)

/**
 * @brief SDK: Updates the current map header state.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses guest pointers or scalars only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vMapHeaderUpdate)

/**
 * @brief SDK: Sets one map attribute.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses guest pointers or scalars only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vMapSetAttribute)

/**
 * @brief SDK: Returns the current matrix.
 * Call model: `sync/fire-and-forget`
 * Ownership: Writes guest memory during the call only.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vMatrixGetCurrent)

/**
 * @brief SDK: Loads identity into the active matrix.
 * Call model: `sync/fire-and-forget`
 * Ownership: No guest memory ownership is retained.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vMatrixIdentity)

/**
 * @brief SDK: Inverts one matrix.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vMatrixInvert)

/**
 * @brief SDK: Builds one look-at matrix.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses guest pointers only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vMatrixLookAt)

/**
 * @brief SDK: Multiplies one matrix pair.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vMatrixMultiply)

/**
 * @brief SDK: Multiplies one 3x3 matrix pair.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vMatrixMultiply3x3)

/**
 * @brief SDK: Builds one orthographic projection matrix.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vMatrixOrtho)

/**
 * @brief SDK: Builds one perspective projection matrix.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vMatrixPerspective)

/**
 * @brief SDK: Rotates one vector by the current matrix.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vMatrixRotateVector)

/**
 * @brief SDK: Applies one X rotation to the active matrix.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vMatrixRotateX)

/**
 * @brief SDK: Applies one Y rotation to the active matrix.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vMatrixRotateY)

/**
 * @brief SDK: Applies one Z rotation to the active matrix.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vMatrixRotateZ)

/**
 * @brief SDK: Applies one scale to the active matrix.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vMatrixScale)

/**
 * @brief SDK: Replaces the current matrix.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses guest pointers only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vMatrixSetCurrent)

/**
 * @brief SDK: Sets one light matrix.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses guest pointers only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vMatrixSetLight)

/**
 * @brief SDK: Sets the projection matrix.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses guest pointers only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vMatrixSetProjection)

/**
 * @brief SDK: Applies one translation to the active matrix.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vMatrixTranslate)

/**
 * @brief SDK: Transposes one matrix.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vMatrixTranspose)

/**
 * @brief SDK: Shows one Unicode message box.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers only during the call.
 * Blocking: Non-blocking in the default integration.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vMsgBoxU)

/**
 * @brief SDK: Receives one task message.
 * Call model: `polling`
 * Ownership: Uses guest pointers only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vReceive)

/**
 * @brief SDK: Receives any pending task message.
 * Call model: `polling`
 * Ownership: Uses guest pointers only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vReceiveAny)

/**
 * @brief SDK: Writes one resource stream.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vResWrite)

/**
 * @brief SDK: Scans the current key matrix.
 * Call model: `polling`
 * Ownership: May write guest memory depending on backend contract; default stub retains nothing.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vScanKeys)

/**
 * @brief SDK: Sends one task message.
 * Call model: `async/request`
 * Ownership: Uses guest pointers or scalars only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSend)

/**
 * @brief SDK: Suspends the current task for one duration.
 * Call model: `async/request`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking in the default integration.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSleep)

/**
 * @brief SDK: Controls one sound channel or mixer state.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers or scalars only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSoundCtrl)

/**
 * @brief SDK: Controls one extended sound state.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers or scalars only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSoundCtrlEx)

/**
 * @brief SDK: Disposes one loaded sound object.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses one handle only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSoundDispose)

/**
 * @brief SDK: Disposes one sound handle.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses one handle only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSoundDisposeHandle)

/**
 * @brief SDK: Returns one sound handle.
 * Call model: `sync/result`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSoundGetHandle)

/**
 * @brief SDK: Returns one sound status value.
 * Call model: `sync/result`
 * Ownership: Uses one handle only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSoundGetStatus)

/**
 * @brief SDK: Initializes the sound subsystem.
 * Call model: `sync/result`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSoundInit)

/**
 * @brief SDK: Loads one sound asset.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers or scalars only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSoundLoad)

/**
 * @brief SDK: Loads one sound asset from file.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers or scalars only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSoundLoadFile)

/**
 * @brief SDK: Loads one sound asset from resource.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers or scalars only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSoundLoadResource)

/**
 * @brief SDK: Loads one sound asset from stream.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers or scalars only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSoundLoadStream)

/**
 * @brief SDK: Pauses one sound channel.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses one handle only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSoundPause)

/**
 * @brief SDK: Starts playback on one sound channel.
 * Call model: `async/request`
 * Ownership: Uses guest pointers or scalars only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSoundPlay)

/**
 * @brief SDK: Resumes one paused sound channel.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses one handle only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSoundResume)

/**
 * @brief SDK: Sets one sound frequency.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSoundSetFrequency)

/**
 * @brief SDK: Sets the master volume.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSoundSetMasterVolume)

/**
 * @brief SDK: Sets the mixer channel count.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSoundSetMixerChannels)

/**
 * @brief SDK: Sets one sound pan value.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSoundSetPan)

/**
 * @brief SDK: Sets one sound parameter block.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses guest pointers or scalars only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSoundSetParameters)

/**
 * @brief SDK: Sets one sound position.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSoundSetPosition)

/**
 * @brief SDK: Sets one sound priority.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSoundSetPriority)

/**
 * @brief SDK: Sets one sound volume.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSoundSetVolume)

/**
 * @brief SDK: Stops one sound channel.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses one handle only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSoundStop)

/**
 * @brief SDK: Stops one looping sound channel.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses one handle only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSoundStopLooping)

/**
 * @brief SDK: Uploads one sound asset.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers or scalars only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSoundUpload)

/**
 * @brief SDK: Formats one string into a guest buffer.
 * Call model: `sync/result`
 * Ownership: Reads and writes guest memory during the call only.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSprintf)

/**
 * @brief SDK: Formats one string using a va-list style parameter block.
 * Call model: `sync/result`
 * Ownership: Reads and writes guest memory during the call only.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSprintfVa)

/**
 * @brief SDK: Accepts one incoming stream connection.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers or scalars only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vStreamAccept)

/**
 * @brief SDK: Connects one stream endpoint.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers or scalars only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vStreamConnect)

/**
 * @brief SDK: Returns whether one task is alive.
 * Call model: `polling`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vTaskAlive)

/**
 * @brief SDK: Returns the current task identifier.
 * Call model: `sync/result`
 * Ownership: No guest memory ownership is retained.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vThisTask)

/**
 * @brief SDK: Adds one vector pair.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vVectorAdd)

/**
 * @brief SDK: Adds one vector array pair.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vVectorArrayAdd)

/**
 * @brief SDK: Computes one vector array delta.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vVectorArrayDelta)

/**
 * @brief SDK: Multiplies one vector by one scalar or matrix.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vVectorMul)

/**
 * @brief SDK: Normalizes one vector.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vVectorNormalize)

/**
 * @brief SDK: Projects one 3D vector.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vVectorProjectV3)

/**
 * @brief SDK: Projects one 4D vector.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vVectorProjectV4)

/**
 * @brief SDK: Subtracts one vector pair.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vVectorSub)

/**
 * @brief SDK: Transforms one 3D vector.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vVectorTransformV3)

/**
 * @brief SDK: Transforms one 4D vector.
 * Call model: `sync/result`
 * Ownership: Uses guest pointers only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vVectorTransformV4)

/**
 * @brief SDK: Yields execution to the surrounding system.
 * Call model: `async/request`
 * Ownership: No guest memory ownership is retained.
 * Blocking: Non-blocking in the default integration.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vYieldToSystem)

/**
 * @brief SDK: Sets one task stack size.
 * Call model: `sync/fire-and-forget`
 * Ownership: Uses scalar arguments only during the call.
 * Blocking: Non-blocking.
 * Status: Stub.
 * Stub behavior: Logs the call and returns zero.
 */
MVM_DEFINE_ZERO_STUB(vSetStackSize)


/**********************************************************************************************************************
 *  END OF FILE MVM_Imports.c
 *********************************************************************************************************************/

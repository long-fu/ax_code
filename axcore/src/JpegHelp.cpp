#include "JpegHelp.hpp"
#include "ax_global_type.h"
#define AX_SHIFT_LEFT_ALIGN(a) (1 << (a))

#define AX_VDEC_WIDTH_ALIGN AX_SHIFT_LEFT_ALIGN(8)
#define AX_JDEC_WIDTH_ALIGN AX_SHIFT_LEFT_ALIGN(6)

#define AX_COMM_ALIGN(value, n) (((value) + (n) - 1) & ~((n) - 1))

#define STREAM_BUFFER_MAX_SIZE (3 * 1024 * 1024)

#define ALIGN_UP(x, align) (((x) + ((align) - 1)) & ~((align) - 1))

typedef struct axSAMPLE_STREAM_BUF_T
{
    AX_MEMORY_ADDR_T tBufAddr;
    AX_U32 uBufSize;
    AX_U8 *pBufBeforeFill;
    AX_U8 *pBufAfterFill;
    AX_BOOL bRingbuf;
} SAMPLE_STREAM_BUF_T;

typedef struct axSAMPLE_INPUT_FILE_INFO_T
{
    FILE *fInput;
    size_t curPos;
    size_t sFileSize;
    AX_PAYLOAD_TYPE_E enDecType;
} SAMPLE_INPUT_FILE_INFO_T;

#define M_DATA 0x00
#define M_SOF0 0xc0
#define M_DHT 0xc4
#define M_SOI 0xd8
#define M_EOI 0xd9
#define M_SOS 0xda
#define M_DQT 0xdb
#define M_DNL 0xdc
#define M_DRI 0xdd
#define M_APP0 0xe0
#define M_APPF 0xef
#define M_COM 0xfe

#define MAKEUS(a, b) ((unsigned short)(((unsigned short)(a)) << 8 | ((unsigned short)(b))))
#define MAKEUI(a, b, c, d) ((unsigned int)(((unsigned int)(a)) << 24 | ((unsigned int)(b)) << 16 | ((unsigned int)(c)) << 8 | ((unsigned int)(d))))

static int GetJPEGWidthHeight(const char *path, unsigned int *punWidth, unsigned int *punHeight)
{
    int Finished = 0;
    long offset = 0;
    unsigned short temp = 0;
    unsigned char id, ucHigh, ucLow;
    FILE *pfRead;

    *punWidth = 0;
    *punHeight = 0;
    pfRead = fopen(path, "rb");
    if (pfRead == 0)
    {
        // printf("[GetJPEGWidthHeight]:can't open file:%s\n", path);
        return -1;
    }

    while (!Finished)
    {
        if (!fread(&id, sizeof(char), 1, pfRead) || id != 0xff || !fread(&id, sizeof(char), 1, pfRead))
        {
            Finished = -2;
            break;
        }

        if (id >= M_APP0 && id <= M_APPF)
        {
            fread(&ucHigh, sizeof(char), 1, pfRead);
            fread(&ucLow, sizeof(char), 1, pfRead);
            temp = MAKEUS(ucHigh, ucLow);
            if (temp >= 2)
            {
                temp -= 2;
            }
            else
            {
                // 处理溢出情况
                // 例如，设置 result 为 0 或其他适当的值
                temp = 0;
            }

            offset = (long)(temp);
            fseek(pfRead, offset, SEEK_CUR);
            // fseek(pfRead, (long)(MAKEUS(ucHigh, ucLow) - 2), SEEK_CUR);
            continue;
        }

        switch (id)
        {
        case M_SOI:
            break;

        case M_COM:
        case M_DQT:
        case M_DHT:
        case M_DNL:
        case M_DRI:
            fread(&ucHigh, sizeof(char), 1, pfRead);
            fread(&ucLow, sizeof(char), 1, pfRead);
            temp = MAKEUS(ucHigh, ucLow);
            if (temp >= 2)
            {
                temp -= 2;
            }
            else
            {
                // 处理溢出情况
                // 例如，设置 result 为 0 或其他适当的值
                temp = 0;
            }

            offset = (long)(temp);
            fseek(pfRead, offset, SEEK_CUR);
            // fseek(pfRead, (long)(MAKEUS(ucHigh, ucLow) - 2), SEEK_CUR);
            break;

        case M_SOF0:
            fseek(pfRead, 3L, SEEK_CUR);
            fread(&ucHigh, sizeof(char), 1, pfRead);
            fread(&ucLow, sizeof(char), 1, pfRead);
            *punHeight = (unsigned int)MAKEUS(ucHigh, ucLow);
            fread(&ucHigh, sizeof(char), 1, pfRead);
            fread(&ucLow, sizeof(char), 1, pfRead);
            *punWidth = (unsigned int)MAKEUS(ucHigh, ucLow);
            return 0;

        case M_SOS:
        case M_EOI:
        case M_DATA:
            Finished = -1;
            break;

        default:
            fread(&ucHigh, sizeof(char), 1, pfRead);
            fread(&ucLow, sizeof(char), 1, pfRead);
            // printf("[GetJPEGWidthHeight]:unknown id: 0x%x ;  length=%hd\n", id, MAKEUS(ucHigh, ucLow));

            temp = MAKEUS(ucHigh, ucLow);
            if (temp >= 2)
            {
                temp -= 2;
            }
            else
            {
                // 处理溢出情况
                // 例如，设置 result 为 0 或其他适当的值
                temp = 0;
            }

            offset = (long)(temp);

            // if (fseek(pfRead, (long)(MAKEUS(ucHigh, ucLow) - 2), SEEK_CUR) != 0)
            if (fseek(pfRead, offset, SEEK_CUR) != 0)
                Finished = -2;
            break;
        }
    }

    if (Finished == -1)
        printf("[GetJPEGWidthHeight]:can't find SOF0!\n");
    else if (Finished == -2)
        printf("[GetJPEGWidthHeight]:jpeg format error!\n");
    return -1;
}

static 
AX_U32 CalcImgSize(AX_U32 nStride, AX_U32 nW, AX_U32 nH, AX_IMG_FORMAT_E eType, AX_U32 nAlign)
{
    AX_U32 nBpp = 0;
    if (nW == 0 || nH == 0)
    {
        // LOG_ERROR("Invalid width %d or height %d!", nW, nH);
        // LOG(ERROR) << "Invalid width or height " << nW << "x" << nH;
        return 0;
    }

    if (0 == nStride)
    {
        nStride = (0 == nAlign) ? nW : ALIGN_UP(nW, nAlign);
    }
    else
    {
        if (nAlign > 0)
        {
            if (nStride % nAlign)
            {
                // LOG_ERROR("stride: %u not %u aligned.!", nStride, nAlign);
                // LOG(ERROR) << "stride: not aligned.!" << nStride << " " << nAlign;
                return 0;
            }
        }
    }

    switch (eType)
    {
    case AX_FORMAT_YUV400:
        nBpp = 8;
        break;
    case AX_FORMAT_YUV420_PLANAR:
    case AX_FORMAT_YUV420_SEMIPLANAR:
    case AX_FORMAT_YUV420_SEMIPLANAR_VU:
        nBpp = 12;
        break;
    case AX_FORMAT_YUV420_SEMIPLANAR_10BIT_P101010:
        nBpp = 15;
        break;
    case AX_FORMAT_YUV422_INTERLEAVED_YUYV:
    case AX_FORMAT_YUV422_INTERLEAVED_UYVY:
    case AX_FORMAT_YUV422_SEMIPLANAR:
    case AX_FORMAT_RGB565:
    case AX_FORMAT_BGR565:
    case AX_FORMAT_ARGB4444:
    case AX_FORMAT_RGBA4444:
    case AX_FORMAT_ABGR4444:
    case AX_FORMAT_BGRA4444:
    case AX_FORMAT_RGBA5551:
    case AX_FORMAT_ARGB1555:
    case AX_FORMAT_ABGR1555:
    case AX_FORMAT_BGRA5551:
        nBpp = 16;
        break;
    case AX_FORMAT_YUV422_SEMIPLANAR_10BIT_P101010:
        nBpp = 20;
        break;
    case AX_FORMAT_YUV444_PACKED:
    case AX_FORMAT_RGB888:
    case AX_FORMAT_BGR888:
    case AX_FORMAT_ARGB8565:
    case AX_FORMAT_RGBA5658:
    case AX_FORMAT_ABGR8565:
    case AX_FORMAT_BGRA5658:
    case AX_FORMAT_YUV420_SEMIPLANAR_10BIT_P010:
        nBpp = 24;
        break;
    case AX_FORMAT_RGBA8888:
    case AX_FORMAT_ARGB8888:
    case AX_FORMAT_BGRA8888:
    case AX_FORMAT_ABGR8888:
    case AX_FORMAT_YUV422_SEMIPLANAR_10BIT_P010:
        nBpp = 32;
        break;
    default:
        nBpp = 0;
        break;
    }

    return nStride * nH * nBpp / 8;
}

static AX_U8 imgGetBytes(FILE *fInput, size_t pos)
{
    AX_U8 data = 0;
    AX_U8 readLen = 0;

    fseeko(fInput, pos, SEEK_SET);
    readLen = fread(&data, 1, 1, fInput);
    if (1 != readLen)
    {
    }
    // SAMPLE_CRIT_LOG(" read file failed\n");

    return data;
}

static AX_S32 __StreamReadFrameInRingBuf(const SAMPLE_INPUT_FILE_INFO_T *pstBsInfo, SAMPLE_STREAM_BUF_T *pstStreamBuf,
                                         size_t oStreamLen, size_t *pReadLen)
{
    off_t oOffset = 0;
    AX_U8 *pBufRd = NULL;
    AX_U8 *pBufStart = NULL;
    AX_U32 uBufSize = 0;
    AX_S32 sRet = 0;
    size_t sReadLen = 0;
    AX_U32 tmp_len = 0, right_len = 0, left_len = 0;

    if (pstBsInfo == NULL)
    {
        // LOG_ERROR("pstBsInfo == NULL\n");
        sRet = AX_ERR_VDEC_UNKNOWN;
        goto ERR_RET;
    }

    if (pstStreamBuf == NULL)
    {
        // LOG_ERROR("pstStreamBuf == NULL\n");
        sRet = AX_ERR_VDEC_UNKNOWN;
        goto ERR_RET;
    }

    pBufRd = pstStreamBuf->pBufAfterFill;
    pBufStart = (AX_U8 *)pstStreamBuf->tBufAddr.pVirAddr;
    uBufSize = pstStreamBuf->uBufSize;
    if (pBufStart == NULL)
    {
        // LOG_ERROR("pBufStart == NULL\n");
        sRet = AX_ERR_VDEC_UNKNOWN;
        goto ERR_RET;
    }

    if (uBufSize < oStreamLen)
    {
        // LOG_ERROR("uBufSize:0x%x < oStreamLen:0x%lx", uBufSize, oStreamLen);
        sRet = AX_ERR_VDEC_UNKNOWN;
        goto ERR_RET;
    }

    *pReadLen = 0;
    oOffset = (off_t)(pBufRd - pBufStart);
    pstStreamBuf->pBufBeforeFill = pstStreamBuf->pBufAfterFill;
    if ((oOffset + oStreamLen) < uBufSize)
    {
        sReadLen = fread(pBufRd, 1, oStreamLen, pstBsInfo->fInput);
        if (sReadLen != oStreamLen)
        {
            // LOG_ERROR("fread FAILED! sReadLen:0x%lx != oStreamLen:0x%lx", sReadLen, oStreamLen);
            sRet = AX_ERR_VDEC_UNKNOWN;
            goto ERR_RET;
        }

        pstStreamBuf->pBufAfterFill = pBufRd + sReadLen;
    }
    else
    {
        /* turnaround */
        right_len = uBufSize - oOffset;
        sReadLen = fread(pBufRd, 1, right_len, pstBsInfo->fInput);
        if (sReadLen != uBufSize - oOffset)
        {
            // LOG_ERROR("fread FAILED! sReadLen:0x%lx != (uBufSize:0x%x - oOffset:0x%lx):0x%x",
            //   sReadLen, uBufSize, oOffset, right_len);

            sRet = AX_ERR_VDEC_UNKNOWN;
            goto ERR_RET;
        }

        left_len = oStreamLen - (uBufSize - oOffset);
        tmp_len = fread(pBufStart, 1, left_len, pstBsInfo->fInput);
        if (tmp_len != left_len)
        {
            // LOG_ERROR("fread FAILED! tmp_len:0x%x != left_len:0x%x, oStreamLen:0x%lx uBufSize:0x%x oOffset:0x%lx",
            //           tmp_len, left_len, oStreamLen, uBufSize, oOffset);
            sRet = AX_ERR_VDEC_UNKNOWN;
            goto ERR_RET;
        }

        sReadLen += tmp_len;
        pstStreamBuf->pBufAfterFill = pBufStart + tmp_len;
    }

    *pReadLen = sReadLen;
ERR_RET:
    return sRet;
}

static AX_S32 StreamParserReadFrameJpeg(SAMPLE_INPUT_FILE_INFO_T *pstBsInfo, SAMPLE_STREAM_BUF_T *pstStreamBuf, size_t *pReadLen)
{
    size_t i, j;
    AX_U32 jpeg_thumb_in_stream = 0;
    AX_U64 tmp, tmp1, tmp_total = 0;
    size_t curPos = 0;
    AX_U32 imgLen = 0;
    AX_S32 s32Ret = 0;
    size_t stream_length = 0;
    AX_U8 *pBufStart = NULL;
    size_t sReadLen = 0;

    pBufStart = (AX_U8 *)pstStreamBuf->tBufAddr.pVirAddr;
    if (pBufStart == NULL)
    {
        // LOG_ERROR("pBufStart == NULL\n");
        return -1;
    }

    stream_length = pstBsInfo->sFileSize;
    for (i = pstBsInfo->curPos; i < stream_length; ++i)
    {
        if (0xFF == imgGetBytes(pstBsInfo->fInput, i))
        {
            /* if 0xFFE1 to 0xFFFD ==> skip  */
            if ((((i + 1) < stream_length) &&
                 0xE1 == imgGetBytes(pstBsInfo->fInput, i + 1)) ||
                (((i + 1) < stream_length) &&
                 0xE2 == imgGetBytes(pstBsInfo->fInput, i + 1)) ||
                (((i + 1) < stream_length) &&
                 0xE3 == imgGetBytes(pstBsInfo->fInput, i + 1)) ||
                (((i + 1) < stream_length) &&
                 0xE4 == imgGetBytes(pstBsInfo->fInput, i + 1)) ||
                (((i + 1) < stream_length) &&
                 0xE5 == imgGetBytes(pstBsInfo->fInput, i + 1)) ||
                (((i + 1) < stream_length) &&
                 0xE6 == imgGetBytes(pstBsInfo->fInput, i + 1)) ||
                (((i + 1) < stream_length) &&
                 0xE7 == imgGetBytes(pstBsInfo->fInput, i + 1)) ||
                (((i + 1) < stream_length) &&
                 0xE8 == imgGetBytes(pstBsInfo->fInput, i + 1)) ||
                (((i + 1) < stream_length) &&
                 0xE9 == imgGetBytes(pstBsInfo->fInput, i + 1)) ||
                (((i + 1) < stream_length) &&
                 0xEA == imgGetBytes(pstBsInfo->fInput, i + 1)) ||
                (((i + 1) < stream_length) &&
                 0xEB == imgGetBytes(pstBsInfo->fInput, i + 1)) ||
                (((i + 1) < stream_length) &&
                 0xEC == imgGetBytes(pstBsInfo->fInput, i + 1)) ||
                (((i + 1) < stream_length) &&
                 0xED == imgGetBytes(pstBsInfo->fInput, i + 1)) ||
                (((i + 1) < stream_length) &&
                 0xEE == imgGetBytes(pstBsInfo->fInput, i + 1)) ||
                (((i + 1) < stream_length) &&
                 0xEF == imgGetBytes(pstBsInfo->fInput, i + 1)))
            {
                /* increase counter */
                i += 2;

                /* check length vs. data */
                if ((i + 1) > (stream_length))
                {
                    s32Ret = AX_ERR_VDEC_UNKNOWN;
                    goto ret;
                }

                /* get length */
                tmp = imgGetBytes(pstBsInfo->fInput, i);
                tmp1 = imgGetBytes(pstBsInfo->fInput, i + 1);
                tmp_total = (tmp << 8) | tmp1;

                /* check length vs. data */
                if ((tmp_total + i) > (stream_length))
                {
                    s32Ret = AX_ERR_VDEC_UNKNOWN;
                    goto ret;
                }
                /* update */
                i += tmp_total - 1;
                continue;
            }

            /* if 0xFFC2 to 0xFFCB ==> skip  */
            if ((((i + 1) < stream_length) &&
                 0xC1 == imgGetBytes(pstBsInfo->fInput, i + 1)) ||
                (((i + 1) < stream_length) &&
                 0xC2 == imgGetBytes(pstBsInfo->fInput, i + 1)) ||
                (((i + 1) < stream_length) &&
                 0xC3 == imgGetBytes(pstBsInfo->fInput, i + 1)) ||
                (((i + 1) < stream_length) &&
                 0xC5 == imgGetBytes(pstBsInfo->fInput, i + 1)) ||
                (((i + 1) < stream_length) &&
                 0xC6 == imgGetBytes(pstBsInfo->fInput, i + 1)) ||
                (((i + 1) < stream_length) &&
                 0xC7 == imgGetBytes(pstBsInfo->fInput, i + 1)) ||
                (((i + 1) < stream_length) &&
                 0xC8 == imgGetBytes(pstBsInfo->fInput, i + 1)) ||
                (((i + 1) < stream_length) &&
                 0xC9 == imgGetBytes(pstBsInfo->fInput, i + 1)) ||
                (((i + 1) < stream_length) &&
                 0xCA == imgGetBytes(pstBsInfo->fInput, i + 1)) ||
                (((i + 1) < stream_length) &&
                 0xCB == imgGetBytes(pstBsInfo->fInput, i + 1)))
            {
                /* increase counter */
                i += 2;

                /* check length vs. data */
                if ((i + 1) > (stream_length))
                {
                    s32Ret = AX_ERR_VDEC_UNKNOWN;
                    goto ret;
                }

                /* get length */
                tmp = imgGetBytes(pstBsInfo->fInput, i);
                tmp1 = imgGetBytes(pstBsInfo->fInput, i + 1);
                tmp_total = (tmp << 8) | tmp1;

                /* check length vs. data */
                if ((tmp_total + i) > (stream_length))
                {
                    s32Ret = AX_ERR_VDEC_UNKNOWN;
                    goto ret;
                }
                /* update */
                i += tmp_total - 1;

                /* look for EOI */
                for (j = i; j < stream_length; ++j)
                {
                    if (0xFF == imgGetBytes(pstBsInfo->fInput, j))
                    {
                        /* EOI */
                        if (((j + 1) < stream_length) &&
                            0xD9 == imgGetBytes(pstBsInfo->fInput, j + 1))
                        {
                            /* check length vs. data */
                            if ((j + 2) >= (stream_length))
                            {
                                curPos = j + 2;
                                s32Ret = 0;
                                goto ret;
                            }
                            /* update */
                            i = j;
                            /* stil data left ==> continue */
                            continue;
                        }
                    }
                }
            }

            /* check if thumbnails in stream */
            if (((i + 1) < stream_length) &&
                0xE0 == imgGetBytes(pstBsInfo->fInput, i + 1))
            {
                if (((i + 9) < stream_length) &&
                    0x4A == imgGetBytes(pstBsInfo->fInput, i + 4) &&
                    0x46 == imgGetBytes(pstBsInfo->fInput, i + 5) &&
                    0x58 == imgGetBytes(pstBsInfo->fInput, i + 6) &&
                    0x58 == imgGetBytes(pstBsInfo->fInput, i + 7) &&
                    0x00 == imgGetBytes(pstBsInfo->fInput, i + 8) &&
                    0x10 == imgGetBytes(pstBsInfo->fInput, i + 9))
                {
                    jpeg_thumb_in_stream = 1;
                }
            }

            /* EOI */
            if (((i + 1) < stream_length) &&
                0xD9 == imgGetBytes(pstBsInfo->fInput, i + 1))
            {
                curPos = i + 2;
                /* update amount of thumbnail or full resolution image */
                if (jpeg_thumb_in_stream)
                {
                    jpeg_thumb_in_stream = 0;
                }
                else
                {
                    s32Ret = 0;
                    goto ret;
                }
            }
        }
    }

ret:
    imgLen = curPos > pstBsInfo->curPos ? curPos - pstBsInfo->curPos : 0;
    if (0 == s32Ret)
    {
        fseeko(pstBsInfo->fInput, pstBsInfo->curPos, SEEK_SET);
        if (pstStreamBuf->bRingbuf == AX_TRUE)
        {
            s32Ret = __StreamReadFrameInRingBuf(pstBsInfo, pstStreamBuf, imgLen, &sReadLen);
            if (s32Ret)
            {
                // LOG_ERROR("__StreamReadFrameInRingBuf FAILED! ret:0x%x", s32Ret);
                return -1;
            }
        }
        else
        {
            if (imgLen > pstStreamBuf->uBufSize)
            {
                // LOG_ERROR("bufSize is not enough(imgLen %d > bufSize %d), please increase STREAM_BUFFER_MAX_SIZE",
                //   imgLen, pstStreamBuf->uBufSize);
                return -1;
            }
            sReadLen = fread(pBufStart, 1, imgLen, pstBsInfo->fInput);
            if (sReadLen != imgLen)
            {
                // LOG_ERROR("fread FAILED! sReadLen:0x%lx != imgLen:0x%x", sReadLen, imgLen);
                return -1;
            }

            pstStreamBuf->pBufBeforeFill = pBufStart;
            pstStreamBuf->pBufAfterFill = pBufStart + sReadLen;
        }
        pstBsInfo->curPos += sReadLen;
    }

    *pReadLen = imgLen;
    return s32Ret;
}

static int JpegDecodeOneFrameInfo(AX_CHAR *pFilePath,
                                  int u32PicWidth,
                                  int u32PicHeight,
                                  AX_VIDEO_FRAME_INFO_T **destFrameInfo)
{
    int res = 0;
    AX_S32 s32Ret = 0;
    AX_S32 sRet = 0;
    AX_U64 streamPhyAddr = 0;
    AX_VOID *pStreamVirAddr = NULL;
    AX_U64 outPhyAddrDst = 0;
    AX_VOID *outVirAddrDst = NULL;
    AX_S32 heightAlign = 0;
    AX_S32 frmStride = 0;

    FILE *fInput = NULL;
    FILE *fp_out = NULL;
    AX_U32 uBufSize = 0;
    off_t inputFileSize = 0;
    AX_CHAR *streamFile = NULL;
    AX_VDEC_DEC_ONE_FRM_T decOneFrmParam;
    SAMPLE_INPUT_FILE_INFO_T stStreamInfo;
    SAMPLE_STREAM_BUF_T stStreamBuf;
    size_t sReadLen = 0;

    AX_U32 uPixBits = 0;
    *destFrameInfo = new AX_VIDEO_FRAME_INFO_T();

    AX_PAYLOAD_TYPE_E enDecType = PT_JPEG;

    streamFile = pFilePath;

    memset(&decOneFrmParam, 0x0, sizeof(AX_VDEC_DEC_ONE_FRM_T));
    memset(&stStreamInfo, 0x0, sizeof(SAMPLE_INPUT_FILE_INFO_T));
    memset(&stStreamBuf, 0x0, sizeof(SAMPLE_STREAM_BUF_T));

    memset((*destFrameInfo), 0x0, sizeof(AX_VIDEO_FRAME_INFO_T));
    /* Reading input file */
    fInput = fopen(streamFile, "rb");
    if (fInput == NULL)
    {
        // LOG_ERROR("Unable to open input stream file:%s\n", streamFile);
        // LOG(ERROR) << "Unable to open input stream file:" << streamFile;
        s32Ret = AX_ERR_VDEC_UNKNOWN;
        goto ERR_RET;
    }

    /* file i/o pointer to full */
    res = fseek(fInput, 0L, SEEK_END);
    if (res)
    {
        // LOG_ERROR("fseek FAILED! ret:%d\n", res);
        // LOG(ERROR) << "fseek FAILED! ret:" << res;
        s32Ret = AX_ERR_VDEC_UNKNOWN;
        goto ERR_RET_CLOSE_IN;
    }

    inputFileSize = ftello(fInput);
    rewind(fInput);

    stStreamBuf.uBufSize = inputFileSize > STREAM_BUFFER_MAX_SIZE ? STREAM_BUFFER_MAX_SIZE : inputFileSize;

    s32Ret = AX_SYS_MemAlloc(&streamPhyAddr, (AX_VOID **)&pStreamVirAddr,
                             stStreamBuf.uBufSize, 0x100, (AX_S8 *)"jpeg_decode_input");
    if (s32Ret != AX_SUCCESS)
    {
        // LOG_ERROR("AX_SYS_MemAlloc FAILED! uBufSize:0x%x ret:0x%x\n",
        //           stStreamBuf.uBufSize, s32Ret);
        // LOG(ERROR) << "AX_SYS_MemAlloc FAILED! size: " << stStreamBuf.uBufSize << " ret: " << s32Ret;
        goto ERR_RET_CLOSE_IN;
    }

    stStreamBuf.tBufAddr.pVirAddr = pStreamVirAddr;
    stStreamBuf.tBufAddr.u64PhyAddr = streamPhyAddr;

    uPixBits = 8;

    frmStride = AX_COMM_ALIGN(u32PicWidth * uPixBits, AX_JDEC_WIDTH_ALIGN * 8) / 8;
    heightAlign = ALIGN_UP(u32PicHeight, 2);

    uBufSize = AX_VDEC_GetPicBufferSize(heightAlign, frmStride,
                                        AX_FORMAT_YUV420_SEMIPLANAR,
                                        NULL,
                                        enDecType);

    s32Ret = AX_SYS_MemAlloc(&outPhyAddrDst, (AX_VOID **)&outVirAddrDst,
                             uBufSize, 0x1000, (AX_S8 *)"jpeg_decode_output");
    if (s32Ret != 0)
    {
        // LOG_ERROR("AX_SYS_MemAlloc FAILED! uBufSize:0x%x ret:0x%x\n",
        //           uBufSize, s32Ret);
        // LOG(ERROR) << "AX_SYS_MemAlloc FAILED! size: " << uBufSize << " ret: " << s32Ret;
        goto ERR_RET_FREE_STREAM;
    }

    stStreamInfo.fInput = fInput;
    stStreamInfo.sFileSize = inputFileSize;

    sRet = StreamParserReadFrameJpeg(&stStreamInfo, &stStreamBuf, &sReadLen);
    if (sRet)
    {
        // LOG_ERROR("StreamParserReadFrameJpeg FAILED! ret:0x%x\n", sRet);
        // LOG(ERROR) << "StreamParserReadFrameJpeg FAILED! ret: " << s32Ret;
        s32Ret = AX_ERR_VDEC_UNKNOWN;
        goto ERR_RET_FREE_OUT;
    }

    if (!sReadLen)
    {
        // LOG_ERROR("read jpeg frame FAILED!\n");
        // LOG(ERROR) << "read jpeg frame FAILED!";
        s32Ret = AX_ERR_VDEC_UNKNOWN;
        goto ERR_RET_FREE_OUT;
    }

    decOneFrmParam.stStream.pu8Addr = (AX_U8 *)stStreamBuf.tBufAddr.pVirAddr;
    decOneFrmParam.stStream.u64PhyAddr = stStreamBuf.tBufAddr.u64PhyAddr;
    decOneFrmParam.stStream.u32StreamPackLen = (AX_U32)sReadLen;

    decOneFrmParam.stFrame.u64VirAddr[0] = (AX_U64)outVirAddrDst;
    decOneFrmParam.stFrame.u64VirAddr[1] = (AX_U64)outVirAddrDst + frmStride * heightAlign;

    decOneFrmParam.stFrame.u64PhyAddr[0] = outPhyAddrDst;
    decOneFrmParam.stFrame.u64PhyAddr[1] = outPhyAddrDst + frmStride * heightAlign;

    decOneFrmParam.enOutputMode = AX_VDEC_OUTPUT_ORIGINAL;
    decOneFrmParam.enImgFormat = AX_FORMAT_YUV420_SEMIPLANAR;

    s32Ret = AX_VDEC_JpegDecodeOneFrame(&decOneFrmParam);

    if (s32Ret != AX_SUCCESS)
    {
        // LOG_ERROR("AX_VDEC_JpegDecodeOneFrame FAILED! ret:0x%x %s\n",
        //           s32Ret, AX_VdecRetStr1(s32Ret));
        // LOG(ERROR) << "AX_VDEC_JpegDecodeOneFrame FAILED! ret: " << s32Ret << " " << AX_VdecRetStr1(s32Ret);
        goto ERR_RET_FREE_OUT;
    }

    memset(&(*destFrameInfo)->stVFrame, 0x0, sizeof(AX_VIDEO_FRAME_T));
    memcpy(&(*destFrameInfo)->stVFrame, &decOneFrmParam.stFrame, sizeof(AX_VIDEO_FRAME_T));

    if (fInput != NULL)
    {
        res = fclose(fInput);
        if (res)
        {
            // LOG_ERROR("fclose FAILED! ret:%d\n", res);
            sRet = AX_ERR_VDEC_UNKNOWN;
        }
        fInput = NULL;
    }

    if (fp_out != NULL)
    {
        res = fclose(fp_out);
        if (res)
        {
            // LOG_ERROR("fclose FAILED! ret:%d\n", res);
            sRet = AX_ERR_VDEC_UNKNOWN;
        }
        fp_out = NULL;
    }

    s32Ret = AX_SYS_MemFree(streamPhyAddr, pStreamVirAddr);
    if (s32Ret != AX_SUCCESS)
    {
        // LOG_ERROR("AX_SYS_MemFree streamPhyAddr FAILED! s32Ret:0x%x\n", s32Ret);
        // LOG(ERROR) << "AX_SYS_MemFree streamPhyAddr FAILED! ret: " << s32Ret;
    }
    else
    {
        streamPhyAddr = 0;
        pStreamVirAddr = 0;
    }

    // s32Ret = AX_SYS_MemFree(outPhyAddrDst, (AX_VOID *)outVirAddrDst);
    // if (s32Ret != AX_SUCCESS)
    // {
    //     LOG_ERROR("AX_SYS_MemFree outPhyAddrDst FAILED! s32Ret:0x%x\n", s32Ret);
    // }

    if (s32Ret || sRet)
    {
        goto ERR_RET;
    }

    // g_u64GetFrmTag += 1;

    return 0;

ERR_RET_FREE_OUT:
    if (outVirAddrDst != NULL)
    {
        sRet = AX_SYS_MemFree(outPhyAddrDst, (AX_VOID *)outVirAddrDst);
        if (sRet != AX_SUCCESS)
        {
            // LOG_ERROR("AX_SYS_MemFree outPhyAddrDst FAILED! sRet:0x%x\n", sRet);
            // LOG(ERROR) << "AX_SYS_MemFree outPhyAddrDst FAILED! ret: " << sRet;
        }
        else
        {
            outPhyAddrDst = 0;
            outVirAddrDst = 0;
        }
    }
ERR_RET_FREE_STREAM:
    if (pStreamVirAddr != NULL)
    {
        sRet = AX_SYS_MemFree(streamPhyAddr, pStreamVirAddr);
        if (sRet != AX_SUCCESS)
        {
            // LOG_ERROR("1 AX_SYS_MemFree streamPhyAddr FAILED! sRet:0x%x\n", sRet);
            // LOG(ERROR) << "AX_SYS_MemFree streamPhyAddr FAILED! ret: " << sRet;
        }
        else
        {
            streamPhyAddr = 0;
            pStreamVirAddr = 0;
        }
    }
ERR_RET_CLOSE_IN:
    if (fInput)
    {
        res = fclose(fInput);
        if (res)
        {
            // LOG_ERROR("fclose FAILED! ret:%d\n", res);
        }
        fInput = NULL;
    }
ERR_RET:
    // LOG_ERROR("s32Ret:0x%x, sRet:0x%x", s32Ret, sRet);
    // LOG(ERROR) << "JpegDecodeOneFrameInfo s32Ret: " << s32Ret << " sRet: " << sRet;
    return s32Ret || sRet;
}

int JpegHelp::JpegDecode(AX_VIDEO_FRAME_INFO_T *tempImage,const std::string &file_path)
{
    // AX_VIDEO_FRAME_INFO_T *tempImage;

    AX_U32 picWidth, picHeight;

    int ret = GetJPEGWidthHeight(file_path.c_str(), &picWidth, &picHeight);

    // DLOG(INFO) << "Read JPEG image width:" << picWidth << " height:" << picHeight;

    ret = JpegDecodeOneFrameInfo((AX_CHAR *)file_path.c_str(), picWidth, picHeight, &tempImage);
    return ret;
}

/// @brief 把Image编码成Jpeg格式的图片
/// @param dest
/// @param src
/// @return
int JpegHelp::JpegEncode(std::vector<uint8_t> &dest, AX_VIDEO_FRAME_INFO_T* src)
{
	const static char *MEM_TOKEN = "JpegEncode";
	AX_JPEG_ENCODE_ONCE_PARAMS_T stJpegEncodeOnceParam;
	AX_IMG_FORMAT_E picFormat = AX_FORMAT_INVALID;
	AX_S32 s32Ret = AX_SUCCESS;
	AX_U32 frameSize = 0;
	AX_U32 input_width = 0;
	AX_U32 input_height = 0;

	memset(&stJpegEncodeOnceParam, 0x0, sizeof(stJpegEncodeOnceParam));

	AX_VIDEO_FRAME_INFO_T stFrame = *src;

	picFormat = stFrame.stVFrame.enImgFormat;
	input_width = stFrame.stVFrame.u32Width;
	input_height = stFrame.stVFrame.u32Height;

	if (stFrame.stVFrame.u32FrameSize == 0)
	{
		frameSize = CalcImgSize(stFrame.stVFrame.u32PicStride[0], stFrame.stVFrame.u32Width,
								stFrame.stVFrame.u32Height, stFrame.stVFrame.enImgFormat, 16);
	}
	else
	{
		frameSize = stFrame.stVFrame.u32FrameSize;
	}

	if (frameSize == 0)
	{
		return -1;
	}

	stJpegEncodeOnceParam.stJpegParam.u32Qfactor = 90;
	stJpegEncodeOnceParam.u32Width = input_width;
	stJpegEncodeOnceParam.u32Height = input_height;
	stJpegEncodeOnceParam.enImgFormat = picFormat;

	stJpegEncodeOnceParam.enStrmBufType = AX_STREAM_BUF_NON_CACHE;

	AX_U64 phyBuff = 0;
	AX_VOID *virBuff = NULL;

	s32Ret = AX_SYS_MemAlloc(&phyBuff, &virBuff, frameSize, 0, (AX_S8 *)MEM_TOKEN);
	if (s32Ret)
	{
		return s32Ret;
	}

	stJpegEncodeOnceParam.u32Len = frameSize;
	stJpegEncodeOnceParam.ulPhyAddr = phyBuff;
	stJpegEncodeOnceParam.pu8Addr = (AX_U8 *)virBuff;

	for (int i = 0; i < 3; i++)
	{
		stJpegEncodeOnceParam.u64PhyAddr[i] = stFrame.stVFrame.u64PhyAddr[i];
		stJpegEncodeOnceParam.u32PicStride[i] = stFrame.stVFrame.u32PicStride[i];
	}

	s32Ret = AX_VENC_JpegEncodeOneFrame(&stJpegEncodeOnceParam);
	if (AX_SUCCESS != s32Ret)
	{
		goto EXIT;
	}

	dest.resize(stJpegEncodeOnceParam.u32Len);
	memcpy(dest.data(), stJpegEncodeOnceParam.pu8Addr, stJpegEncodeOnceParam.u32Len);

EXIT:

	if (stJpegEncodeOnceParam.ulPhyAddr && (NULL != stJpegEncodeOnceParam.pu8Addr))
	{
		s32Ret = AX_SYS_MemFree(stJpegEncodeOnceParam.ulPhyAddr, stJpegEncodeOnceParam.pu8Addr);
		if (s32Ret != AX_SUCCESS)
		{
		}
		else
		{
			stJpegEncodeOnceParam.ulPhyAddr = 0;
			stJpegEncodeOnceParam.pu8Addr = nullptr;
		}
	}

	return s32Ret;
}
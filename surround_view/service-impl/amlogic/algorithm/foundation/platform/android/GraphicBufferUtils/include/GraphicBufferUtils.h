/* Copyright Statement:
*
* This software/firmware and related documentation ("Amlogic Software") are
* protected under relevant copyright laws. The information contained herein is
* confidential and proprietary to Amlogic Inc. and/or its licensors. Without
* the prior written permission of Amlogic inc. and/or its licensors, any
* reproduction, modification, use or disclosure of Amlogic Software, and
* information contained herein, in whole or in part, shall be strictly
* prohibited.
*
* Amlogic Inc. (C) 2013. All rights reserved.
*
* BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
* THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("Amlogic SOFTWARE")
* RECEIVED FROM Amlogic AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER
* ON AN "AS-IS" BASIS ONLY. Amlogic EXPRESSLY DISCLAIMS ANY AND ALL
* WARRANTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR
* NONINFRINGEMENT. NEITHER DOES Amlogic PROVIDE ANY WARRANTY WHATSOEVER WITH
* RESPECT TO THE SOFTWARE OF ANY THIRD PARTY WHICH MAY BE RENDERED BY,
* INCORPORATED IN, OR SUPPLIED WITH THE Amlogic SOFTWARE, AND RECEIVER AGREES
* TO LORESULT_OK ONLY TO SUCH THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO.
* RECEIVER EXPRESSLY ACKNOWLEDGES THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO
* OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES CONTAINED IN Amlogic
* SOFTWARE. Amlogic SHALL ALSO NOT BE RESPONSIBLE FOR ANY Amlogic SOFTWARE
* RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
* STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND Amlogic'S
* ENTIRE AND CUMULATIVE LIABILITY WITH RESPECT TO THE Amlogic SOFTWARE
* RELEASED HEREUNDER WILL BE, AT Amlogic'S OPTION, TO REVISE OR REPLACE THE
* Amlogic SOFTWARE AT ISSUE, OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE
* CHARGE PAID BY RECEIVER TO Amlogic FOR SUCH Amlogic SOFTWARE AT ISSUE.
*/

#ifndef GRAPHIC_BUFFER_H
#define GRAPHIC_BUFFER_H

#include <ui/GraphicBuffer.h>
#include <GLUtils.h>
#include "Defination.h"

class GraphicBufferUtils {
public:
    static int PixelFormatToEGLFormat(PixelFormat pixelFormat, EGLint &eglFormat);
    static int EGLFormatToPixelFormat(EGLint eglFormat, PixelFormat &pixelFormat);

    static int PixelFormatToGLFormat(PixelFormat pixelFormat, GLenum &glFormat);
    static int GLFormatToPixelFormat(GLenum glFormat, PixelFormat &pixelFormat);

    static int GraphicBufferToTexture(sp<GraphicBuffer> buffer, Texture &texture);

    static int Align(int n, int divisor);
    static int SwapWithFile(sp<GraphicBuffer> buffer, const char *path, int fromFile);
};

#endif // GRAPHIC_BUFFER_H

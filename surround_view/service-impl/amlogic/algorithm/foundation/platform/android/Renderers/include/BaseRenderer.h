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

#ifndef BASE_RENDERER_H
#define BASE_RENDERER_H

#include <GLUtils.h>

class BaseRenderer {
public:
    virtual ~BaseRenderer() {};

    virtual int Init();
    virtual int Deinit();

    virtual int SetParams(const char *type, ...);

    virtual int Begin();
    virtual int Render();
    virtual int Finish();

protected:
    pthread_mutex_t mLock = PTHREAD_MUTEX_INITIALIZER;
    int mInited = 0;

    size_t mViewTop = 0;
    size_t mViewLeft = 0;
    size_t mViewWidth = 0;
    size_t mViewHeight = 0;

    virtual int InitDatas();
    virtual int DeinitDatas();

    virtual EGLint InitEGL();
    virtual EGLint DeinitEGL();

    virtual GLenum InitGL();
    virtual GLenum DeinitGL();

    virtual GLenum Prepare();
    virtual GLenum Draw();
    virtual GLenum Complete();

private:
    virtual GLenum SetViewPortInternal();
};

#endif // BASE_RENDERER_H

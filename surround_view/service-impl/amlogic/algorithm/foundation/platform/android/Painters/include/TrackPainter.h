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

#ifndef TRACK_PAINTER_H
#define TRACK_PAINTER_H

#include "BasePainter.h"

class TrackPainter : public BasePainter {
public:
    TrackPainter();
    int SetParams(const char *type, ...);

protected:
    int InitDatas();
    int DeinitDatas();

    GLenum InitGL();
    GLenum DeinitGL();

    GLenum Draw();

private:
    GLuint mProgram;
    GLint mPositionLoc;
    GLint mMVPLoc;

    const char *mVertexShader;
    const char *mFragmentShader;

    size_t mMVPSize;
    GLfloat mMVP[MVPLEN];

    int mEnabled;

    double mCarD;
    double mCarL;
    double mCarW;
    double mCarA;

    size_t mTrackSegmentNum;
    double mTrackSegmentAngle;
    double mTrackLineLength;
    double mTrackLineYStart;
    GLfloat mTrackLineWidth;

    GLfloat *mVertices;
    GLenum mVertexMode;
    GLint mVertexLen;
    GLint mVertexNum;
    GLenum mVertexType;
    GLboolean mVertexNormalized;
    GLsizeiptr mVertexSize;

    int InitVertices();
    int UpdateVertices();

    DISALLOW_COPY_AND_ASSIGN(TrackPainter);
};

#endif // TRACK_PAINTER_H

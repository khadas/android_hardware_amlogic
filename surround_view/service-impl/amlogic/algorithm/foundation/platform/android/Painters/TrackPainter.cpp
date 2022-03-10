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

#define LOG_TAG "TrackPainter"

#include <math.h>
#include "include/TrackPainter.h"


TrackPainter::TrackPainter()
    : BasePainter() {
}

int TrackPainter::InitVertices() {
    GLint halfVertexNum = mVertexNum * mVertexLen >> 1;

    double positiveHalfW = mCarW / 2.0;
    double negativeHalfW = -positiveHalfW;

    double ySegment = mTrackLineLength / mTrackSegmentNum;
    double yPos = mTrackLineYStart;

    GLint index0 = 0;
    GLint index1 = halfVertexNum;

    while (index0 < halfVertexNum) {
        mVertices[index0] = negativeHalfW;
        mVertices[index1] = positiveHalfW;

        mVertices[index0+1] = yPos;
        mVertices[index1+1] = yPos;

        mVertices[index1+2] = 0.0;
        mVertices[index1+2] = 0.0;

        yPos += ySegment;

        index0 += mVertexLen;
        index1 += mVertexLen;
    }

    return RESULT_OK;
}

int TrackPainter::UpdateVertices() {
    GLint halfVertexNum = mVertexNum * mVertexLen >> 1;

    if (mCarA == 0.0) {
        double positiveHalfW = mCarW / 2.0;
        double negativeHalfW = -positiveHalfW;

        double ySegment = mTrackLineLength / mTrackSegmentNum;
        double yPos = mTrackLineYStart;

        GLint index0 = 0;
        GLint index1 = halfVertexNum;

        while (index0 < halfVertexNum) {
            mVertices[index0] = negativeHalfW;
            mVertices[index1] = positiveHalfW;

            mVertices[index0+1] = yPos;
            mVertices[index1+1] = yPos;

            yPos += ySegment;

            index0 += mVertexLen;
            index1 += mVertexLen;
        }
    } else {
        double tanA = tan(abs(mCarA));
        double xshift = mCarL / tanA;
        double r0 = xshift - mCarW/2.0;
        double r1 = r0 + mCarW;

        double cosAngle;
        double sinAngle;

        GLint index0 = 0;
        GLint index1 = halfVertexNum;
        double angle = 0.0;

        if (mCarA > 0.0) {
            while (index0 < halfVertexNum) {
                cosAngle = cos(angle);
                sinAngle = sin(angle);

                mVertices[index0] = r0 * cosAngle - xshift;
                mVertices[index1] = r1 * cosAngle - xshift;

                mVertices[index0+1] = r0 * sinAngle - mCarD;
                mVertices[index1+1] = r1 * sinAngle - mCarD;

                index0 += mVertexLen;
                index1 += mVertexLen;
                angle += mTrackSegmentAngle;
            }
        } else {
            while (index0 < halfVertexNum) {
                cosAngle = cos(angle);
                sinAngle = sin(angle);

                mVertices[index0] = xshift - r0 * cosAngle;
                mVertices[index1] = xshift - r1 * cosAngle;

                mVertices[index0+1] = r0 * sinAngle - mCarD;
                mVertices[index1+1] = r1 * sinAngle - mCarD;

                index0 += mVertexLen;
                index1 += mVertexLen;
                angle += mTrackSegmentAngle;
            }
        }
    }

    /*
    if (mCarA == 0.0) {
        double positiveHalfW = mCarW / 2.0;
        double negativeHalfW = -positiveHalfW;

        GLint index0 = 0;
        GLint index1 = halfVertexNum;

        while (index0 < halfVertexNum) {
            mVertices[index0] = negativeHalfW;
            mVertices[index1] = positiveHalfW;

            index0 += mVertexLen;
            index1 += mVertexLen;
        }
    } else {
        double tanA = tan(abs(mCarA));
        double xshift = mCarL/tanA;
        double r0 = xshift - mCarW/2.0;
        double r1 = r0 + mCarW;

        double r02 = pow(r0, 2);
        double r12 = pow(r1, 2);

        GLint index0 = 0;
        GLint index1 = halfVertexNum;

        if (mCarA > 0.0) {
            while (index0 < halfVertexNum) {
                mVertices[index0] = sqrt(r02 - pow(mVertices[index0+1], 2)) - xshift;
                mVertices[index1] = sqrt(r12 - pow(mVertices[index1+1], 2)) - xshift;

                index0 += mVertexLen;
                index1 += mVertexLen;
            }
        } else {
            while (index0 < halfVertexNum) {
                mVertices[index0] = xshift - sqrt(r02 - pow(mVertices[index0+1], 2));
                mVertices[index1] = xshift - sqrt(r12 - pow(mVertices[index1+1], 2));

                index0 += mVertexLen;
                index1 += mVertexLen;
            }
        }
    }
    */

    return RESULT_OK;
}

int TrackPainter::InitDatas() {
    mProgram = GL_NONE;

    mPositionLoc = -1;
    mMVPLoc = -1;

    static const char vertexShader[] =
            "#version 300 es                                    \n"
            "in vec4 vPosition;                                 \n"
            "uniform mat4 mvp;                                  \n"
            "void main()                                        \n"
            "{                                                  \n"
            "   gl_Position = mvp * vPosition;                  \n"
            "}                                                  \n";

    static const char fragmentShader[] =
            "#version 300 es                                    \n"
            "precision mediump float;                           \n"
            "out vec4 fragColor;                                \n"
            "void main()                                        \n"
            "{                                                  \n"
            "   fragColor = vec4(0.0, 1.0, 1.0, 0.7);           \n"
            "}                                                  \n";

    mVertexShader = vertexShader;
    mFragmentShader = fragmentShader;

    mMVPSize = sizeof(GLfloat) * MVPLEN;
    GLfloat mvp[] = {
       -0.9,    0.0,  0.0, 0.0,
        0.0,   -1.0,  0.0, 0.0,
        0.0,    0.0,  1.0, 0.0,
       -0.005, -0.91, 0.0, 1.0
    };
    memcpy(mMVP, mvp, mMVPSize);

    mEnabled = 0;

    mCarD = 0.5;
    mCarL = 1.0;
    mCarW = 0.8;
    mCarA = 0.0;

    mTrackSegmentNum = 15;
    mTrackSegmentAngle = 6.0 * 3.1415926 / 180.0;
    mTrackLineLength = 3.0 * mCarD;
    mTrackLineYStart = mCarD/2.0;
    mTrackLineWidth = 15.0;

    mVertexMode = GL_LINE_STRIP;
    mVertexLen = 3;
    mVertexNum =  (mTrackSegmentNum + 1) << 1;
    mVertexType = GL_FLOAT;
    mVertexNormalized = GL_FALSE;
    mVertexSize = mVertexLen * mVertexNum * sizeof(GLfloat);

    mVertices = (GLfloat *)malloc(mVertexSize);
    if (mVertices == nullptr) {
        LOGE("malloc faied!");
        return RESULT_BASE_MODULE_ERROR;
    }

    return InitVertices();
    // UpdateVertices();
}

int TrackPainter::DeinitDatas() {
    if (mVertices != nullptr) {
        free(mVertices);
        mVertices = nullptr;
    }

    return RESULT_OK;
}

GLenum TrackPainter::InitGL() {
    GLenum result = GLUtils::CreateProgram(&mProgram,
            mVertexShader, mFragmentShader);
    CHECK_RESULT_AND_RETURN(result, GL_NO_ERROR);

    mPositionLoc = glGetAttribLocation(mProgram, "vPosition");
    CHECK_GL_RESULT_AND_RETURN("glGetAttribLocation");

    mMVPLoc = glGetUniformLocation(mProgram, "mvp");
    CHECK_GL_RESULT_AND_RETURN("glGetUniformLocation");

    glLineWidth(mTrackLineWidth);
    CHECK_GL_RESULT_AND_RETURN("glLineWidth")

    LOGD("program: %d, position location: %d, mvp location: %d",
            mProgram, mPositionLoc, mMVPLoc);

    return GL_NO_ERROR;
}

GLenum TrackPainter::DeinitGL() {
    GLenum result = GLUtils::DeleteProgram(&mProgram);
    CHECK_RESULT_AND_RETURN(result, GL_NO_ERROR);

    return GL_NO_ERROR;
}

int TrackPainter::SetParams(const char *type, ...) {
    if (type == nullptr) {
        LOGE("bad parameter!");
        return RESULT_BAD_PARAMETER;
    }

    if (!strcasecmp(type, "car_infos")) {
        va_list args;
	    va_start(args, type);

        CarInfo *carInfos = (CarInfo *)va_arg(args, void *);
        size_t num = va_arg(args, size_t);

        va_end(args);

        if (carInfos == nullptr) {
            LOGE("bad parameter!");
            return RESULT_BAD_PARAMETER;
        }

        for (size_t index = 0; index < num; index++) {
            if (!strcasecmp(carInfos[index].name, "back_car")) {
                mEnabled = strcasecmp((char*) carInfos[index].value, "0");
                continue;
            }

            if (!strcasecmp(carInfos[index].name, "steering_angle")) {
                mCarA = atof((char *)(carInfos[index].value));
                if (mCarA > 1.57) {
                    mCarA = 1.57;
                } else if (mCarA < -1.57) {
                    mCarA = -1.57;
                }
                continue;
            }
        }

        LOGD("enabled: %d, angle: %f", mEnabled, mCarA);
        UpdateVertices();

        return RESULT_OK;
    }

    va_list args;
    va_start(args, type);
    int result = BasePainter::SetParams(type, args);
    va_end(args);

    return result;
}

GLenum TrackPainter::Draw() {
    if (mEnabled) {
        LOGD("---use program: %d", mProgram);
        glUseProgram(mProgram);
        CHECK_GL_RESULT_AND_RETURN("glUseProgram");

        glUniformMatrix4fv(mMVPLoc, 1, GL_FALSE, mMVP);
        CHECK_GL_RESULT_AND_RETURN("glUniformMatrix4fv");

        glVertexAttribPointer(mPositionLoc, mVertexLen, mVertexType,
                mVertexNormalized, 0, mVertices);
        CHECK_GL_RESULT_AND_RETURN("glVertexAttribPointer");

        glEnableVertexAttribArray(mPositionLoc);
        CHECK_GL_RESULT_AND_RETURN("glEnableVertexAttribArray");

        LOGD("glDrawArrays");
        glDrawArrays (mVertexMode, 0, mVertexNum>>1);
        CHECK_GL_RESULT_AND_RETURN("glDrawArrays");

        glDrawArrays (mVertexMode, mVertexNum>>1, mVertexNum>>1);
        CHECK_GL_RESULT_AND_RETURN("glDrawArrays");

        glDisableVertexAttribArray(mPositionLoc);
        CHECK_GL_RESULT_AND_RETURN("glDisableVertexAttribArray");

        LOGD("---use program: %d", GL_NONE);
        glUseProgram(GL_NONE);
        CHECK_GL_RESULT_AND_RETURN("glUseProgram");
    }

    return GL_NO_ERROR;
}


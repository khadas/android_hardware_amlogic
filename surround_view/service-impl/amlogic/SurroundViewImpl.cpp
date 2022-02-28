/*
 * Copyright (C) 2017 Amlogic Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSEERROR_CODE_INVALID_OPERATION.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *  DESCRIPTION
 *      This file implements a adaptor of audio decoder from Amlogic.
 *
 */

#include "include/SurroundViewImpl.h"

namespace android_auto {
namespace surround_view {

    bool SurroundViewImpl::SetStaticData(
            const SurroundViewStaticDataParams& static_data_params) {
        return true;
    }

    bool SurroundViewImpl::Start2dPipeline() {
        return true;
    }

    bool SurroundViewImpl::Start3dPipeline() {
        return true;
    }

    void SurroundViewImpl::Stop2dPipeline() {
        return;
    }

    void SurroundViewImpl::Stop3dPipeline() {
        return;
    }

    bool SurroundViewImpl::Update2dOutputResolution(const Size2dInteger& resolution) {
        return true;
    }

    bool SurroundViewImpl::Update3dOutputResolution(const Size2dInteger& resolution) {
        return true;
    }

    bool SurroundViewImpl::GetProjectionPointFromRawCameraToSurroundView2d(
            const Coordinate2dInteger& camera_point, int camera_index,
            Coordinate2dFloat* surround_view_2d_point) {
        return true;
    }

    bool SurroundViewImpl::GetProjectionPointFromRawCameraToSurroundView3d(
            const Coordinate2dInteger& camera_point, int camera_index,
            Coordinate3dFloat* surround_view_3d_point) {
        return true;
    }

    bool SurroundViewImpl::Get2dSurroundView(
            const std::vector<SurroundViewInputBufferPointers>& input_pointers,
            SurroundViewResultPointer* result_pointer) {
        return true;
    }

    bool SurroundViewImpl::Get3dSurroundView(
            const std::vector<SurroundViewInputBufferPointers>& input_pointers,
            const std::array<std::array<float, 4>, 4>& view_matrix,
            SurroundViewResultPointer* result_pointer) {
        return true;
    }

    bool SurroundViewImpl::Get3dSurroundView(
            const std::vector<SurroundViewInputBufferPointers>& input_pointers,
            const std::array<float, 4>& quaternion, const std::array<float, 3>& translation,
            SurroundViewResultPointer* result_pointer) {
        return true;
    }

    bool SurroundViewImpl::Set3dOverlay(const std::vector<Overlay>& overlays) {
        return true;
    }

    bool SurroundViewImpl::SetAnimations(const std::vector<AnimationParam>& car_animations) {
        return true;
    }

    std::vector<SurroundViewInputBufferPointers> SurroundViewImpl::ReadImages(
                    const char* filename0,
                    const char* filename1,
                    const char* filename2,
                    const char* filename3) {
        static std::vector<SurroundViewInputBufferPointers> temp;
        return temp;
    }

    void SurroundViewImpl::WriteImage(const SurroundViewResultPointer result_pointerer,
                    const char* filename) {
        return;
    }

    SurroundView *Create() {
       return new SurroundViewImpl();
    }

}
}


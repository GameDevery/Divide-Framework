#include "stdafx.h"

#include "Headers/PostFXWindow.h"
#include "Editor/Headers/Editor.h"
#include "Core/Headers/PlatformContext.h"
#include "Core/Headers/StringHelper.h"
#include "Platform/Video/Headers/GFXDevice.h"
#include "Rendering/Headers/Renderer.h"
#include "Rendering/PostFX/Headers/PostFX.h"
#include "Rendering/PostFX/CustomOperators/Headers/SSAOPreRenderOperator.h"
#include "Rendering/PostFX/CustomOperators/Headers/BloomPreRenderOperator.h"
#include "Rendering/PostFX/CustomOperators/Headers/DoFPreRenderOperator.h"
#include "Rendering/PostFX/CustomOperators/Headers/PostAAPreRenderOperator.h"

#undef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

namespace Divide {
    PostFXWindow::PostFXWindow(Editor& parent, PlatformContext& context, const Descriptor& descriptor)
        : PlatformContextComponent(context),
          DockedWindow(parent, descriptor),
          _postFX(context.gfx().getRenderer().postFX())
    {
    }

    void PostFXWindow::drawInternal() {
        PreRenderBatch* batch = _postFX.getFilterBatch();

        const auto checkBox = [this](FilterType filter, bool overrideScene = false, const char* label = "Enabled") {
            bool filterEnabled = _postFX.getFilterState(filter);
            if (ImGui::Checkbox(label, &filterEnabled)) {
                if (filterEnabled) {
                    _postFX.pushFilter(filter, overrideScene);
                } else {
                    _postFX.popFilter(filter, overrideScene);
                }
            }
        };

        F32 edgeThreshold = batch->edgeDetectionThreshold();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Edge Threshold: "); ImGui::SameLine();
        ImGui::PushItemWidth(170);
        if (ImGui::SliderFloat("##hidelabel", &edgeThreshold, 0.01f, 1.0f)) {
            batch->edgeDetectionThreshold(edgeThreshold);
        }
        ImGui::PopItemWidth();
        if (ImGui::CollapsingHeader("SS Antialiasing")) {
            PreRenderOperator* op = batch->getOperator(FilterType::FILTER_SS_ANTIALIASING);
            PostAAPreRenderOperator& aaOp = static_cast<PostAAPreRenderOperator&>(*op);
            I32 level = to_I32(aaOp.postAAQualityLevel());

            ImGui::AlignTextToFramePadding();
            ImGui::PushItemWidth(175);
            ImGui::Text("Quality level: "); ImGui::SameLine();
            ImGui::PushID("quality_level_slider");
            if (ImGui::SliderInt("##hidelabel", &level, 0, 5)) {
                aaOp.postAAQualityLevel(to_U8(level));
            }
            ImGui::PopID();
            ImGui::PopItemWidth();
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Method: "); ImGui::SameLine();
            static I32 selection = 0;
            const bool a = ImGui::RadioButton("SMAA", &selection, 0); ImGui::SameLine();
            const bool b = ImGui::RadioButton("FXAA", &selection, 1); ImGui::SameLine();
            const bool c = ImGui::RadioButton("NONE", &selection, 2);
            if (a || b|| c) {
                if (selection != 2) {
                    _postFX.pushFilter(FilterType::FILTER_SS_ANTIALIASING);
                    aaOp.useSMAA(selection == 0);
                } else {
                    _postFX.popFilter(FilterType::FILTER_SS_ANTIALIASING);
                }
            }
        }
        if (ImGui::CollapsingHeader("SS Ambient Occlusion")) {
            checkBox(FilterType::FILTER_SS_AMBIENT_OCCLUSION);
            PreRenderOperator* op = batch->getOperator(FilterType::FILTER_SS_AMBIENT_OCCLUSION);
            SSAOPreRenderOperator& ssaoOp = static_cast<SSAOPreRenderOperator&>(*op);
            F32 radius = ssaoOp.radius();
            F32 power = ssaoOp.power();
            if (ImGui::SliderFloat("Radius", &radius, 0.01f, 10.0f)) {
                ssaoOp.radius(radius);
            }
            if (ImGui::SliderFloat("Power", &power, 1.0f, 5.0f)) {
                ssaoOp.power(power);
            }
        }
        if (ImGui::CollapsingHeader("Depth of Field")) {
            checkBox(FilterType::FILTER_DEPTH_OF_FIELD);
            PreRenderOperator* op = batch->getOperator(FilterType::FILTER_DEPTH_OF_FIELD);
            DoFPreRenderOperator& dofOp = static_cast<DoFPreRenderOperator&>(*op);
            F32 focalDepth = dofOp.focalDepth();
            bool autoFocus = dofOp.autoFocus();
            if (ImGui::SliderFloat("Focal Depth", &focalDepth, 0.0f, 1.0f)) {
                dofOp.focalDepth(focalDepth);
            }
            if (ImGui::Checkbox("Auto Focus", &autoFocus)) {
                dofOp.autoFocus(autoFocus);
            }
        }
        if (ImGui::CollapsingHeader("Bloom")) {
            checkBox(FilterType::FILTER_BLOOM);
            PreRenderOperator* op = batch->getOperator(FilterType::FILTER_BLOOM);
            BloomPreRenderOperator& bloomOp = static_cast<BloomPreRenderOperator&>(*op);
            F32 factor = bloomOp.factor();
            F32 threshold = bloomOp.luminanceThreshold();
            if (ImGui::SliderFloat("Factor", &factor, 0.01f, 3.0f)) {
                bloomOp.factor(factor);
            }
            if (ImGui::SliderFloat("Luminance Threshold", &threshold, 0.0f, 1.0f)) {
                bloomOp.luminanceThreshold(threshold);
            }
        }

        if (ImGui::CollapsingHeader("Tone Mapping")) {
            bool adaptiveExposure = batch->adaptiveExposureControl();
            if (ImGui::Checkbox("Adaptive Exposure", &adaptiveExposure)) {
                batch->adaptiveExposureControl(adaptiveExposure);
            }

            ToneMapParams params = batch->toneMapParams();
            if (adaptiveExposure) {
                if (ImGui::SliderFloat("Min Log Luminance", &params.minLogLuminance, -16.0f, 0.0f)) {
                    batch->toneMapParams(params);
                }
                if (ImGui::SliderFloat("Max Log Luminance", &params.maxLogLuminance, 0.0f, 16.0f)) {
                    batch->toneMapParams(params);
                }
                if (ImGui::SliderFloat("Tau", &params.tau, 0.1f, 2.0f)) {
                    batch->toneMapParams(params);
                }
                if (ImGui::SliderFloat("Exposure", &params.manualExposureAdaptive, 0.01f, 1.99f)) {
                    batch->toneMapParams(params);
                }
            } else {
                if (ImGui::SliderFloat("Exposure", &params.manualExposure, 0.01f, 1.99f)) {
                    batch->toneMapParams(params);
                }
            }
            if (ImGui::SliderFloat("White Balance", &params.manualWhitePoint, 0.01f, 1.99f)) {
                batch->toneMapParams(params);
            }
            checkBox(FilterType::FILTER_LUT_CORECTION);
        }
        if (ImGui::CollapsingHeader("Test Effects")) {
            checkBox(FilterType::FILTER_NOISE, true, PostFX::FilterName(FilterType::FILTER_NOISE));
            checkBox(FilterType::FILTER_VIGNETTE, true, PostFX::FilterName(FilterType::FILTER_VIGNETTE));
            checkBox(FilterType::FILTER_UNDERWATER, true, PostFX::FilterName(FilterType::FILTER_UNDERWATER));
        }
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
        if (ImGui::CollapsingHeader("SS Reflections")) {
            checkBox(FilterType::FILTER_SS_REFLECTIONS);
        }
        if (ImGui::CollapsingHeader("Motion Blur")) {
            checkBox(FilterType::FILTER_MOTION_BLUR);
        }
        ImGui::PopItemFlag();
        ImGui::PopStyleVar();
    }
};
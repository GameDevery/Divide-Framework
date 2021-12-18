#include "stdafx.h"

#include "Headers/IMPrimitive.h"

#include "Core/Math/BoundingVolumes/Headers/OBB.h"
#include "Headers/CommandBufferPool.h"
#include "Platform/Video/Headers/GFXDevice.h"
#include "Platform/Video/Textures/Headers/Texture.h"

namespace Divide {

IMPrimitive::IMPrimitive(GFXDevice& context)
    : VertexDataInterface(context)
{
    assert(handle()._id != 0);
    _cmdBuffer = GFX::AllocateCommandBuffer();
}

IMPrimitive::~IMPrimitive() 
{
    DeallocateCommandBuffer(_cmdBuffer);
}

void IMPrimitive::reset() {
    resetWorldMatrix();
    resetViewport();
    _textureEntry = {};
    _cmdBufferDirty = true;
    clearBatch();
}

void IMPrimitive::fromOBB(const OBBDescriptor& box) {
    fromOBBs(&box, 1u);
}

void IMPrimitive::fromOBBs(const OBBDescriptor* boxes, const size_t count) {
    if (count == 0u) {
        return;
    }
    std::array<Line, 12> lines = {};

    // Create the object containing all of the lines
    beginBatch(true, 12 * to_U32(count) * 2 * 14, 2);
        for (size_t i = 0u; i < count; ++i) {
            const OBBDescriptor& descriptor = boxes[i];
            OBB::OOBBEdgeList edges = descriptor.box.edgeList();
            for (U8 j = 0u; j < 12u; ++j)
            {
                lines[j].positionStart(edges[j]._start);
                lines[j].positionEnd(edges[j]._end);
                lines[j].colourStart(descriptor.colour);
                lines[j].colourEnd(descriptor.colour);
            }

            fromLinesInternal(lines.data(), lines.size());
        }
    endBatch();
}

void IMPrimitive::fromBox(const BoxDescriptor& box) {
    fromBoxes(&box, 1u);
}

void IMPrimitive::fromBoxes(const BoxDescriptor* boxes, const size_t count) {
    if (count == 0u) {
        return;
    }

    // Create the object
    beginBatch(true, to_U32(count * 16u), 1);
        for (size_t i = 0u; i < count; ++i) {
            const BoxDescriptor& box = boxes[i];
            const UColour4& colour = box.colour;
            const vec3<F32>& min = box.min;
            const vec3<F32>& max = box.max;

            // Set it's colour
            attribute4f(to_base(AttribLocation::COLOR), Util::ToFloatColour(colour));
            // Draw the bottom loop
            begin(PrimitiveType::LINE_LOOP);
                vertex(min.x, min.y, min.z);
                vertex(max.x, min.y, min.z);
                vertex(max.x, min.y, max.z);
                vertex(min.x, min.y, max.z);
            end();
            // Draw the top loop
            begin(PrimitiveType::LINE_LOOP);
                vertex(min.x, max.y, min.z);
                vertex(max.x, max.y, min.z);
                vertex(max.x, max.y, max.z);
                vertex(min.x, max.y, max.z);
            end();
            // Connect the top to the bottom
            begin(PrimitiveType::LINES);
                vertex(min.x, min.y, min.z);
                vertex(min.x, max.y, min.z);
                vertex(max.x, min.y, min.z);
                vertex(max.x, max.y, min.z);
                vertex(max.x, min.y, max.z);
                vertex(max.x, max.y, max.z);
                vertex(min.x, min.y, max.z);
                vertex(min.x, max.y, max.z);
            end();
        }
    // Finish our object
    endBatch();
}

void IMPrimitive::fromSphere(const SphereDescriptor& sphere) {
    fromSpheres(&sphere, 1u);
}

void IMPrimitive::fromSpheres(const SphereDescriptor* spheres, const size_t count) {
    if (count == 0u) {
        return;
    }

    beginBatch(true, 32u * ((32u + 1) * 2), 1);
    for (size_t c = 0u; c < count; ++c) {
        const SphereDescriptor& sphere = spheres[c];
        const F32 drho = M_PI_f / sphere.stacks;
        const F32 dtheta = 2.f * M_PI_f / sphere.slices;
        const F32 ds = 1.f / sphere.slices;
        const F32 dt = 1.f / sphere.stacks;

        F32 t = 1.f;
        // Create the object
            attribute4f(to_base(AttribLocation::COLOR), Util::ToFloatColour(sphere.colour));
            begin(PrimitiveType::LINE_LOOP);
                for (U32 i = 0u; i < sphere.stacks; i++) {
                    const F32 rho = i * drho;
                    const F32 srho = std::sin(rho);
                    const F32 crho = std::cos(rho);
                    const F32 srhodrho = std::sin(rho + drho);
                    const F32 crhodrho = std::cos(rho + drho);

                    F32 s = 0.0f;
                    for (U32 j = 0; j <= sphere.slices; j++) {
                        const F32 theta = j == sphere.slices ? 0.0f : j * dtheta;
                        const F32 stheta = -std::sin(theta);
                        const F32 ctheta = std::cos(theta);

                        F32 x = stheta * srho;
                        F32 y = ctheta * srho;
                        F32 z = crho;
                        vertex(x * sphere.radius + sphere.center.x,
                               y * sphere.radius + sphere.center.y,
                               z * sphere.radius + sphere.center.z);
                        x = stheta * srhodrho;
                        y = ctheta * srhodrho;
                        z = crhodrho;
                        s += ds;
                        vertex(x * sphere.radius + sphere.center.x,
                               y * sphere.radius + sphere.center.y,
                               z * sphere.radius + sphere.center.z);
                    }
                    t -= dt;
                }
            end();
    }
    endBatch();
}

//ref: http://www.freemancw.com/2012/06/opengl-cone-function/
void IMPrimitive::fromCone(const ConeDescriptor& cone) {
    fromCones(&cone, 1u);
}

void IMPrimitive::fromCones(const ConeDescriptor* cones, const size_t count) {
    if (count == 0u) {
        return;
    }

    constexpr U8 slices = 16u;
    constexpr F32 angInc = 360.0f / slices * M_PIDIV180_f;

    beginBatch(true, to_U32(count * (slices + 1)), 1u);

    for (size_t i = 0u; i < count; ++i) {
        const ConeDescriptor& cone = cones[i];

        const vec3<F32> invDirection = -cone.direction;
        const vec3<F32> c = cone.root + -invDirection * cone.length;
        const vec3<F32> e0 = Perpendicular(invDirection);
        const vec3<F32> e1 = Cross(e0, invDirection);

        // calculate points around directrix
        std::array<vec3<F32>, slices> pts = {};
        for (size_t j = 0; j < pts.size(); ++j) {
            const F32 rad = angInc * j;
            pts[j] = c + (e0 * std::cos(rad) + e1 * std::sin(rad)) * cone.radius;
        }

        // draw cone top
        attribute4f(to_base(AttribLocation::COLOR), Util::ToFloatColour(cone.colour));
        // Top
        begin(PrimitiveType::TRIANGLE_FAN);
            vertex(cone.root);
            for (U8 j = 0u; j < slices; ++j) {
                vertex(pts[j]);
            }
        end();

        // Bottom
        begin(PrimitiveType::TRIANGLE_FAN);
            vertex(c);
            for (I8 j = slices - 1; j >= 0; --j) {
                vertex(pts[j]);
            }
        end();
    }
    endBatch();
}

void IMPrimitive::fromLines(const Line* lines, const size_t count) {
    if (count == 0u) {
        return;
    }

    // Check if we have a valid list. The list can be programmatically
    // generated, so this check is required
    // Create the object containing all of the lines
    beginBatch(true, to_U32(count) * 2 * 14, 2);
        fromLinesInternal(lines, count);
    // Finish our object
    endBatch();
}

void IMPrimitive::fromLinesInternal(const Line* lines, size_t count) {
    if (count == 0u) {
        return;
    }

    attribute4f(to_base(AttribLocation::COLOR), Util::ToFloatColour(lines[0].colourStart()));
    attribute2f(to_base(AttribLocation::GENERIC), vec2<F32>(1.f, 1.f));
    // Set the mode to line rendering
    begin(PrimitiveType::LINES);
    // Add every line in the list to the batch
    for (size_t i = 0u; i < count; ++i) {
        const Line& line = lines[i];
        attribute4f(to_base(AttribLocation::COLOR), Util::ToFloatColour(line.colourStart()));
        attribute2f(to_base(AttribLocation::GENERIC), vec2<F32>(line.widthStart(), line.widthEnd()));
        vertex(line.positionStart());

        attribute4f(to_base(AttribLocation::COLOR), Util::ToFloatColour(line.colourEnd()));
        attribute2f(to_base(AttribLocation::GENERIC), vec2<F32>(line.widthStart(), line.widthEnd()));
        vertex(line.positionEnd());

    }
    end();
}

void IMPrimitive::setPushConstants(const PushConstants& constants) {
    _additionalConstats = constants;
    _cmdBufferDirty = true;
}

void IMPrimitive::pipeline(const Pipeline& pipeline) noexcept {
    if (_pipeline == nullptr || *_pipeline != pipeline) {
        _pipeline = &pipeline;
        _cmdBufferDirty = true;
    }
}

void IMPrimitive::texture(const Texture& texture, const size_t samplerHash) {
    TextureEntry tempEntry{};
    tempEntry._data = texture.data();
    tempEntry._sampler = samplerHash;
    tempEntry._binding = to_U8(TextureUsage::UNIT0);

    if (_textureEntry != tempEntry) {
        _textureEntry = tempEntry;
        _cmdBufferDirty = true;
    }
}

GFX::CommandBuffer& IMPrimitive::toCommandBuffer() const {
    if (_cmdBufferDirty) {
        _cmdBuffer->clear();

        DIVIDE_ASSERT(_pipeline->shaderProgramHandle() != 0, "IMPrimitive error: Draw call received without a valid shader defined!");

        GFX::EnqueueCommand(*_cmdBuffer, GFX::BindPipelineCommand{ _pipeline });

        PushConstants& constants = GFX::EnqueueCommand<GFX::SendPushConstantsCommand>(*_cmdBuffer)->_constants;
        // Inform the shader if we have (or don't have) a texture
        constants.set(_ID("useTexture"), GFX::PushConstantType::BOOL, IsValid(_textureEntry));
        // Upload the primitive's world matrix to the shader
        constants.set(_ID("dvd_WorldMatrix"), GFX::PushConstantType::MAT4, worldMatrix());

        if (!_additionalConstats.empty()) {
            bool partial = false;
            Merge(constants, _additionalConstats, partial);
        }

        if (IsValid(_textureEntry)) {
            GFX::EnqueueCommand<GFX::BindDescriptorSetsCommand>(*_cmdBuffer)->_set._textureData.add(_textureEntry);
        }

        if (_viewport != Rect<I32>(-1)) {
            GFX::EnqueueCommand(*_cmdBuffer, GFX::SetViewportCommand{ _viewport });
        }

        GenericDrawCommand cmd{};
        cmd._primitiveType = PrimitiveType::TRIANGLE_STRIP;
        cmd._sourceBuffer = handle();
        GFX::EnqueueCommand(*_cmdBuffer, GFX::DrawCommand{ cmd });

        _cmdBufferDirty = false;
    }

    return *_cmdBuffer;
}


};
/*
    Copyright 2023 Jesse Talavera-Greenberg

    melonDS DS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS DS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS DS. If not, see http://www.gnu.org/licenses/.
*/

#include "software.hpp"

#include <retro_assert.h>

#include <NDS.h>
#include <gfx/scaler/pixconv.h>

#include "config/config.hpp"
#include "config/types.hpp"
#include "input/input.hpp"
#include "message/error.hpp"
#include "screenlayout.hpp"
#include "tracy.hpp"

using glm::ivec2;
using glm::mat3;
using glm::vec3;
using glm::uvec2;
using std::span;

MelonDsDs::SoftwareRenderState::SoftwareRenderState(const CoreConfig& config) noexcept :
    buffer(1, 1),
    hybridBuffer(1, 1),
    hybridScaler(
        SCALER_FMT_ARGB8888,
        SCALER_FMT_ARGB8888,
        config.ScreenFilter() == ScreenFilter::Nearest ? SCALER_TYPE_POINT : SCALER_TYPE_BILINEAR,
        NDS_SCREEN_WIDTH,
        NDS_SCREEN_HEIGHT,
        NDS_SCREEN_WIDTH * config.HybridRatio(),
        NDS_SCREEN_HEIGHT * config.HybridRatio()
    ) {
}

// TODO: Consider using RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER
void MelonDsDs::SoftwareRenderState::Render(
    melonDS::NDS& nds,
    const InputState& inputState,
    const CoreConfig& config,
    const ScreenLayoutData& screenLayout
) noexcept {
    ZoneScopedN(TracyFunction);

    buffer.SetSize(screenLayout.BufferSize());

    if (IsHybridLayout(screenLayout.Layout()) || IsLargeScreenLayout(screenLayout.Layout())) {
        uvec2 requiredHybridBufferSize = NDS_SCREEN_SIZE<unsigned> * screenLayout.HybridRatio();
        hybridBuffer.SetSize(requiredHybridBufferSize);

        auto filter = config.ScreenFilter() == ScreenFilter::Nearest ? SCALER_TYPE_POINT : SCALER_TYPE_BILINEAR;
        hybridScaler.SetScalerType(filter);
        hybridScaler.SetOutSize(requiredHybridBufferSize.x, requiredHybridBufferSize.y);
    }

    const uint32_t* topScreenBuffer = nds.GPU.Framebuffer[nds.GPU.FrontBuffer][0].get();
    const uint32_t* bottomScreenBuffer = nds.GPU.Framebuffer[nds.GPU.FrontBuffer][1].get();
    CombineScreens(
        span<const uint32_t, NDS_SCREEN_AREA<size_t>>(topScreenBuffer, NDS_SCREEN_AREA<size_t>),
        span<const uint32_t, NDS_SCREEN_AREA<size_t>>(bottomScreenBuffer, NDS_SCREEN_AREA<size_t>),
        screenLayout
    );

    if (!nds.IsLidClosed() && inputState.CursorVisible()) {
        DrawCursor(inputState, config, screenLayout);
    }

    retro::video_refresh(buffer[0], buffer.Width(), buffer.Height(), buffer.Stride());

#ifdef HAVE_TRACY
    if (tracy::ProfilerAvailable()) {
        // If Tracy is connected...
        ZoneScopedN("MelonDsDs::render::RenderSoftware::SendFrameToTracy");
        std::unique_ptr<uint8_t[]> frame = std::make_unique<uint8_t[]>(buffer.Width() * buffer.Height() * 4);
        {
            ZoneScopedN("conv_argb8888_abgr8888");
            conv_argb8888_abgr8888(frame.get(), buffer[0], buffer.Width(), buffer.Height(), buffer.Stride(), buffer.Stride());
        }
        // libretro wants pixels in XRGB8888 format,
        // but Tracy wants them in XBGR8888 format.

        FrameImage(frame.get(), buffer.Width(), buffer.Height(), 0, false);
    }
#endif
}

void MelonDsDs::SoftwareRenderState::Render(
    const error::ErrorScreen& error,
    const ScreenLayoutData& screenLayout
) noexcept {
    ZoneScopedN(TracyFunction);

    buffer.SetSize(screenLayout.BufferSize());
    CombineScreens(error.TopScreen(), error.BottomScreen(), screenLayout);

    retro::video_refresh(buffer[0], buffer.Width(), buffer.Height(), buffer.Stride());
}

void MelonDsDs::SoftwareRenderState::CopyScreen(const uint32_t* src, uvec2 destTranslation, ScreenLayout layout) noexcept {
    ZoneScopedN(TracyFunction);
    // Only used for software rendering

    // melonDS's software renderer draws each emulated screen to its own buffer,
    // and then the frontend combines them based on the current layout.
    // In the original buffer, all pixels are contiguous in memory.
    // If a screen doesn't need anything drawn to its side (such as blank space or another screen),
    // then we can just copy the entire screen at once.
    // But if a screen *does* need anything drawn on either side of it,
    // then its pixels can't all be contiguous in memory.
    // In that case, we have to copy each row of pixels individually to a different offset.
    if (LayoutSupportsDirectCopy(layout)) {
        buffer.CopyDirect(src, destTranslation);
    }
    else {
        // Not all of this screen's pixels will be contiguous in memory, so we have to copy them row by row
        buffer.CopyRows(src, destTranslation, NDS_SCREEN_SIZE<unsigned>);
    }
}

void MelonDsDs::SoftwareRenderState::DrawCursor(const InputState& input, const CoreConfig& config,
    const ScreenLayoutData& screenLayout
) noexcept {
    ZoneScopedN(TracyFunction);
    // Only used for software rendering

    if (screenLayout.Layout() == ScreenLayout::TopOnly)
        return;

    ivec2 cursorSize = ivec2(config.CursorSize());
    ivec2 clampedTouch = clamp(input.TouchPosition(), ivec2(0), ivec2(NDS_SCREEN_WIDTH - 1, NDS_SCREEN_HEIGHT - 1));
    ivec2 transformedTouch = screenLayout.GetBottomScreenMatrix() * vec3(clampedTouch, 1);

    uvec2 start = clamp(transformedTouch - ivec2(cursorSize), ivec2(0), ivec2(buffer.Size()));
    uvec2 end = clamp(transformedTouch + ivec2(cursorSize), ivec2(0), ivec2(buffer.Size()));
    int scale;
    if (screenLayout.Layout() == ScreenLayout::LargescreenBottom ||
        screenLayout.Layout() == ScreenLayout::FlippedLargescreenBottom) {
        scale = screenLayout.HybridRatio();  // e.g., 2, 3, ...
    } else {
        scale = 1;
    }
    int centerX = (start.x + end.x) / 2;
    int centerY = (start.y + end.y) / 2;
    // scale >= 1 (integer)

    // Framebuffer size
    const int W = int(buffer.Size().x);
    const int H = int(buffer.Size().y);
    const int cx = int(centerX);
    const int cy = int(centerY);

    // Safe bounds for the scaled 5x5 base area (from base coords -2..+2)
    // dest x range for base b ∈ [-2..+2] is [cx + b*scale .. cx + b*scale + (scale-1)]
    const int sx = std::max(0,                cx - 2*scale);
    const int sy = std::max(0,                cy - 2*scale);
    const int ex = std::min(W - 1,            cx + 2*scale + (scale - 1));
    const int ey = std::min(H - 1,            cy + 2*scale + (scale - 1));

    // Floor-division helper for symmetric mapping
    auto floor_div = [](int a, int b) {
        int q = a / b, r = a % b;
        return (r && (a < 0)) ? (q - 1) : q;
    };

    auto set_px = [&](int x, int y, uint32_t c) {
        uint32_t& p = buffer[uvec2(x, y)];
        p = c;
    };

    // Base pattern (unscaled):
    // - White fill: |bx|<=1 && |by|<=1           --> 3x3
    // - Black ring: (|by|==2 && |bx|<=1) || (|bx|==2 && |by|<=1)
    // - Corners (|bx|==2 && |by|==2) are skipped
    for (int y = sy; y <= ey; ++y) {
        for (int x = sx; x <= ex; ++x) {
            const int bx = floor_div(x - cx, scale);  // base-space X in [-2..2]
            const int by = floor_div(y - cy, scale);  // base-space Y in [-2..2]

            const int abx = std::abs(bx);
            const int aby = std::abs(by);

            bool inWhite3x3 = (abx <= 1 && aby <= 1);
            bool isCorner   = (abx == 2 && aby == 2);
            bool inRing     = !isCorner &&
                            ( (aby == 2 && abx <= 1) || (abx == 2 && aby <= 1) );

            if (inWhite3x3) {
                set_px(x, y, 0xFFFFFFFF); // ARGB opaque white
            } else if (inRing) {
                set_px(x, y, 0xFF000000); // ARGB opaque black
            }
            // else: outside the 4x4-without-corners ring → leave untouched
        }
    }

}

void MelonDsDs::SoftwareRenderState::CombineScreens(
    std::span<const uint32_t, NDS_SCREEN_AREA<size_t>> topBuffer,
    std::span<const uint32_t, NDS_SCREEN_AREA<size_t>> bottomBuffer,
    const ScreenLayoutData& screenLayout
) noexcept {
    ZoneScopedN(TracyFunction);

    buffer.Clear();
    ScreenLayout layout = screenLayout.Layout();

    if (IsHybridLayout(layout)) {
        auto primaryBuffer = layout == ScreenLayout::HybridTop || layout == ScreenLayout::FlippedHybridTop ? topBuffer : bottomBuffer;

        hybridScaler.Scale(hybridBuffer[0], primaryBuffer.data());
        buffer.CopyRows(
            hybridBuffer[0],
            screenLayout.GetHybridScreenTranslation(),
            NDS_SCREEN_SIZE<unsigned> * screenLayout.HybridRatio()
        );

        HybridSideScreenDisplay smallScreenLayout = screenLayout.HybridSmallScreenLayout();

        if (smallScreenLayout == HybridSideScreenDisplay::Both || layout == ScreenLayout::HybridBottom || layout == ScreenLayout::FlippedHybridBottom) {
            // If we should display both screens, or if the bottom one is the primary...
            buffer.CopyRows(topBuffer.data(), screenLayout.GetTopScreenTranslation(), NDS_SCREEN_SIZE<unsigned>);
        }

        if (smallScreenLayout == HybridSideScreenDisplay::Both || layout == ScreenLayout::HybridTop || layout == ScreenLayout::FlippedHybridTop) {
            // If we should display both screens, or if the top one is being focused...
            buffer.CopyRows(bottomBuffer.data(), screenLayout.GetBottomScreenTranslation(), NDS_SCREEN_SIZE<unsigned>);
        }
    } 
    else if (IsLargeScreenLayout(layout)) {
        bool focusTop = layout == ScreenLayout::LargescreenTop || layout == ScreenLayout::FlippedLargescreenTop;
        if (focusTop) {
            auto primaryBuffer = topBuffer;
            hybridScaler.Scale(hybridBuffer[0], primaryBuffer.data());
            buffer.CopyRows(
                hybridBuffer[0],
                screenLayout.GetTopScreenTranslation(),
                NDS_SCREEN_SIZE<unsigned> * screenLayout.HybridRatio()
            );
            // If the top screen is the primary copy the bottom to the small screen
            CopyScreen(bottomBuffer.data(), screenLayout.GetBottomScreenTranslation(), layout);
        } else {
            auto primaryBuffer = bottomBuffer;
            hybridScaler.Scale(hybridBuffer[0], primaryBuffer.data());
            buffer.CopyRows(
                hybridBuffer[0],
                screenLayout.GetBottomScreenTranslation(),
                NDS_SCREEN_SIZE<unsigned> * screenLayout.HybridRatio()
            );            
            // If the bottom screen is the primary copy the top to the small screen
            CopyScreen(topBuffer.data(), screenLayout.GetTopScreenTranslation(), layout);
        }
    } 
    else {
        if (layout != ScreenLayout::BottomOnly)
            CopyScreen(topBuffer.data(), screenLayout.GetTopScreenTranslation(), layout);

        if (layout != ScreenLayout::TopOnly)
            CopyScreen(bottomBuffer.data(), screenLayout.GetBottomScreenTranslation(), layout);
    }
}


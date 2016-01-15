/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// #include "GLContext.h"
#include "WebGLExtensions.h"
#include "nsIFrame.h"
#include "mozilla/unused.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/gfx/2D.h"

using namespace mozilla::gfx;

namespace mozilla {
    
WebGLExtensionTextureFromElement::WebGLExtensionTextureFromElement(WebGLContext* webgl)
    : WebGLExtensionBase(webgl)
{
    const uint sw = 256, sh = 256;
    mTarget = gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(
                IntSize(ceil(sw), ceil(sh)), SurfaceFormat::B8G8R8A8);
}

WebGLExtensionTextureFromElement::~WebGLExtensionTextureFromElement()
{
    // TODO: Release mTarget
}

void
WebGLExtensionTextureFromElement::TexImage2D(GLenum rawTexImageTarget, GLint level, 
                                            dom::Element& elem)
{
    const char funcName[] = "texImage2D";
    const uint8_t funcDims = 2;

    TexImageTarget target;
    WebGLTexture* tex = mContext->ActiveBoundTextureForTexImageTarget(rawTexImageTarget);

    nsIFrame* frame = elem.GetPrimaryFrame(Flush_Layout);

    if (frame) {
        frame = nsLayoutUtils::GetStyleFrame(frame);    
    }
    
    if (!frame) {
        return;
    }

    RefPtr<nsPresContext> presContext = frame->PresContext();

    // Do some transform
    // End of transform

    // mTarget.
    nscolor backgroundColor = 0xffffffff;
    uint32_t renderDocFlags = (nsIPresShell::RENDER_IGNORE_VIEWPORT_SCROLLING |
                             nsIPresShell::RENDER_DOCUMENT_RELATIVE);
    RefPtr<gfxContext> thebes;
    RefPtr<DrawTarget> drawDT;
    const uint sw = 256, sh = 256;

    nsRect r(nsPresContext::CSSPixelsToAppUnits((float)0),
           nsPresContext::CSSPixelsToAppUnits((float)0),
           nsPresContext::CSSPixelsToAppUnits((float)sw),
           nsPresContext::CSSPixelsToAppUnits((float)sh));

    // drawDT = gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(
    //             IntSize(ceil(sw), ceil(sh)), SurfaceFormat::B8G8R8A8);
    // if (!drawDT) {
    //   return;
    // }

    thebes = new gfxContext(mTarget);

    nsCOMPtr<nsIPresShell> shell = presContext->PresShell();
    Unused << shell->RenderDocument(r, renderDocFlags, backgroundColor, thebes);
    // Render document https://dxr.mozilla.org/mozilla-central/source/dom/canvas/CanvasRenderingContext2D.cpp#4911

    if (mTarget) {
        RefPtr<SourceSurface> snapshot = mTarget->Snapshot();
        RefPtr<DataSourceSurface> data = snapshot->GetDataSurface();

        DataSourceSurface::MappedSurface rawData;
        if (NS_WARN_IF(!data->Map(DataSourceSurface::READ, &rawData))) {
            return;
        }
        RefPtr<SourceSurface> source =
          mTarget->CreateSourceSurfaceFromData(rawData.mData,
                                               data->GetSize(),
                                               rawData.mStride,
                                               data->GetFormat());
        data->Unmap();

        if (!source) {
          return;
        }

        const double alpha = 1.0;
        const uint w = sw, h = sh;
        gfx::Rect destRect(0, 0, w, h);
        gfx::Rect sourceRect(0, 0, sw, sh);
        mTarget->DrawSurface(source, destRect, sourceRect,
                             DrawSurfaceOptions(gfx::Filter::POINT),
                             DrawOptions(alpha, mozilla::gfx::CompositionOp::OP_OVER,
                                         AntialiasMode::NONE));
        mTarget->Flush();
    }

    // // note that x and y are coordinates in the document that
    // // we're drawing; x and y are drawn to 0,0 in current user
    // // space.
    // RedrawUser(gfxRect(0, 0, w, h));

    // Render domElement into a WebGL texture. Currently, it supports <img>, <canvas>, 
    // and <video> elements
    const bool isSubImage = false;
    const GLint xOffset = 0;
    const GLint yOffset = 0;
    const GLint zOffset = 0;
    ErrorResult out_error;

    tex->TexOrSubImage(isSubImage, funcName, rawTexImageTarget, 0, LOCAL_GL_RGBA, xOffset,
                       yOffset, zOffset, LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE, &elem, &out_error);
}

void
WebGLExtensionTextureFromElement::TexSubImage2D(GLenum target, GLint level, 
                                            GLint xoffset, GLint yoffset, dom::Element& elem) 
{
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionTextureFromElement, MOZ_texture_from_element)

} // namespace mozilla
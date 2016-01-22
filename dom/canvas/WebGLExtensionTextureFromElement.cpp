/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"
#include "TexUnpackBlob.h"
#include "nsIFrame.h"
#include "mozilla/unused.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/dom/HTMLIFrameElement.h"

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
    const bool isSubImage = false;
    const GLint xOffset = 0;
    const GLint yOffset = 0;
    const GLint zOffset = 0;
    ErrorResult out_error;

    WebGLTexture* tex = mContext->ActiveBoundTextureForTexImageTarget(rawTexImageTarget);
    TexImageTarget target;
    nsIDocument* contentDocument = nullptr;

    if (dom::HTMLIFrameElement* iframeElem =
        dom::HTMLIFrameElement::FromContentOrNull(&elem)) {
        contentDocument = iframeElem->GetContentDocument();
    } else {
        contentDocument = elem.GetComposedDoc();
    }

    nsIPresShell* shell = contentDocument->GetShell();

    // Do some transform
    // End of transform

    nscolor backgroundColor = 0xffffffff;
    uint32_t renderDocFlags = nsIPresShell::RENDER_DOCUMENT_RELATIVE;
    RefPtr<gfxContext> thebes;
    const uint sw = 256, sh = 256;

    nsRect r(nsPresContext::CSSPixelsToAppUnits((float)0),
        nsPresContext::CSSPixelsToAppUnits((float)0),
        nsPresContext::CSSPixelsToAppUnits((float)sw),
        nsPresContext::CSSPixelsToAppUnits((float)sh));

    thebes = new gfxContext(mTarget);

    if (shell && false) {
        Unused << shell->RenderDocument(r, renderDocFlags, backgroundColor, thebes);
        // Render document https://dxr.mozilla.org/mozilla-central/source/dom/canvas/CanvasRenderingContext2D.cpp#4911
        RefPtr<SourceSurface> snapshot = mTarget->Snapshot();

        UniquePtr<webgl::TexUnpackBlob> blob;
        blob.reset(new webgl::TexUnpackSurface(snapshot, false));
        const GLint border = 0;
        
        tex->TexOrSubImage(isSubImage, funcName, rawTexImageTarget, level, LOCAL_GL_RGBA, 
            xOffset, yOffset, zOffset, border, LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE, blob.get());

        return;
    }

    // // note that x and y are coordinates in the document that
    // // we're drawing; x and y are drawn to 0,0 in current user
    // // space.
    // RedrawUser(gfxRect(0, 0, w, h));
 
    // Render domElement into a WebGL texture. Currently, it supports <img>, <canvas>,
    // and <video> elements
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
/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// #include "GLContext.h"
#include "WebGLExtensions.h"

namespace mozilla {

WebGLExtensionTextureFromElement::WebGLExtensionTextureFromElement(WebGLContext* webgl)
	: WebGLExtensionBase(webgl)
{
}

WebGLExtensionTextureFromElement::~WebGLExtensionTextureFromElement()
{
}

void
WebGLExtensionTextureFromElement::TexImage2D(GLenum target, GLint level, 
											dom::Element& elem)
{
}

void
WebGLExtensionTextureFromElement::TexSubImage2D(GLenum target, GLint level, 
											GLint xoffset, GLint yoffset, dom::Element& elem) 
{
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionTextureFromElement, MOZ_texture_from_element)

} // namespace mozilla
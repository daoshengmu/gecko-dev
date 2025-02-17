/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGTextElement_h
#define mozilla_dom_SVGTextElement_h

#include "mozilla/dom/SVGTextPositioningElement.h"

nsresult NS_NewSVGTextElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

typedef SVGTextPositioningElement SVGTextElementBase;

class SVGTextElement final : public SVGTextElementBase {
 protected:
  explicit SVGTextElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  virtual JSObject* WrapNode(JSContext* cx,
                             JS::Handle<JSObject*> aGivenProto) override;

  friend nsresult(::NS_NewSVGTextElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));

 public:
  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* aAttribute) const override;

  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

 protected:
  virtual EnumAttributesInfo GetEnumInfo() override;
  virtual LengthAttributesInfo GetLengthInfo() override;

  nsSVGEnum mEnumAttributes[1];
  virtual nsSVGEnum* EnumAttributes() override { return mEnumAttributes; }

  nsSVGLength2 mLengthAttributes[1];
  virtual nsSVGLength2* LengthAttributes() override {
    return mLengthAttributes;
  }
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_SVGTextElement_h

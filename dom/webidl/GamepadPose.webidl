/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Pref="dom.gamepad.extensions.enabled"]
interface GamepadPose {
  readonly attribute boolean hasOrientation;
  readonly attribute boolean hasPosition;
  [Constant, Throws] readonly attribute Float32Array? position;
  [Constant, Throws] readonly attribute Float32Array? linearVelocity;
  [Constant, Throws] readonly attribute Float32Array? linearAcceleration;
  [Constant, Throws] readonly attribute Float32Array? orientation;
  [Constant, Throws] readonly attribute Float32Array? angularVelocity;
  [Constant, Throws] readonly attribute Float32Array? angularAcceleration;
};
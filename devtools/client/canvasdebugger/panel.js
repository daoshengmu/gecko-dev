/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cc, Ci, Cu, Cr } = require("chrome");
const EventEmitter = require("devtools/shared/event-emitter");
const { CanvasFront } = require("devtools/shared/fronts/canvas");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");

function CanvasDebuggerPanel(iframeWindow, toolbox) {
  this.panelWin = iframeWindow;
  this._toolbox = toolbox;
  this._destroyer = null;

  EventEmitter.decorate(this);
}

exports.CanvasDebuggerPanel = CanvasDebuggerPanel;

CanvasDebuggerPanel.prototype = {
  /**
   * Open is effectively an asynchronous constructor.
   *
   * @return object
   *         A promise that is resolved when the Canvas Debugger completes opening.
   */
  open: async function() {
    this.panelWin.gToolbox = this._toolbox;
    this.panelWin.gTarget = this.target;
    this.panelWin.gFront = new CanvasFront(this.target.client, this.target.form);

    await this.panelWin.startupCanvasDebugger();

    this.isReady = true;
    this.emit("ready");
    return this;
  },

  // DevToolPanel API

  get target() {
    return this._toolbox.target;
  },

  destroy: function() {
    // Make sure this panel is not already destroyed.
    if (this._destroyer) {
      return this._destroyer;
    }

    return this._destroyer = this.panelWin.shutdownCanvasDebugger().then(() => {
      // Destroy front to ensure packet handler is removed from client
      this.panelWin.gFront.destroy();
      this.emit("destroyed");
    });
  },
};

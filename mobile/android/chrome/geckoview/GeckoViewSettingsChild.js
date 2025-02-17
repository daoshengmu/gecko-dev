/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/GeckoViewChildModule.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  GeckoViewUtils: "resource://gre/modules/GeckoViewUtils.jsm",
});

XPCOMUtils.defineLazyGetter(
  this, "MOBILE_USER_AGENT",
  function() {
    return Cc["@mozilla.org/network/protocol;1?name=http"]
           .getService(Ci.nsIHttpProtocolHandler).userAgent;
  });

XPCOMUtils.defineLazyGetter(
  this, "DESKTOP_USER_AGENT",
  function() {
    return MOBILE_USER_AGENT
           .replace(/Android \d.+?; [a-zA-Z]+/, "X11; Linux x86_64")
           .replace(/Gecko\/[0-9\.]+/, "Gecko/20100101");
  });

XPCOMUtils.defineLazyGetter(
  this, "VR_USER_AGENT",
  function() {
    return MOBILE_USER_AGENT.replace(/Mobile/, "Mobile VR");
  });

// This needs to match GeckoSessionSettings.java
const USER_AGENT_MODE_MOBILE = 0;
const USER_AGENT_MODE_DESKTOP = 1;
const USER_AGENT_MODE_VR = 2;

// Handles GeckoView content settings including:
// * tracking protection
// * user agent mode
class GeckoViewSettingsChild extends GeckoViewChildModule {
  onInit() {
    debug `onInit`;
    this._userAgentMode = USER_AGENT_MODE_MOBILE;
    this._userAgentOverride = null;
  }

  onSettingsUpdate() {
    debug `onSettingsUpdate ${this.settings}`;

    this.displayMode = this.settings.displayMode;
    this.useTrackingProtection = !!this.settings.useTrackingProtection;
    this.userAgentMode = this.settings.userAgentMode;
    this.userAgentOverride = this.settings.userAgentOverride;
    this.allowJavascript = this.settings.allowJavascript;
  }

  get useTrackingProtection() {
    return docShell.useTrackingProtection;
  }

  set useTrackingProtection(aUse) {
    docShell.useTrackingProtection = aUse;
  }

  get userAgent() {
    if (this.userAgentOverride !== null) {
      return this.userAgentOverride;
    }
    if (this.userAgentMode === USER_AGENT_MODE_DESKTOP) {
      return DESKTOP_USER_AGENT;
    }
    if (this.userAgentMode === USER_AGENT_MODE_VR) {
      return VR_USER_AGENT;
    }
    return null;
  }

  get userAgentMode() {
    return this._userAgentMode;
  }

  set userAgentMode(aMode) {
    if (this.userAgentMode === aMode) {
      return;
    }
    this._userAgentMode = aMode;
    const docShell = content && GeckoViewUtils.getRootDocShell(content);
    if (docShell) {
      docShell.customUserAgent = this.userAgent;
    } else {
      warn `Failed to set custom user agent. Doc shell not found`;
    }
    if (this._userAgentOverride !== null) {
      return;
    }
    const utils = content.windowUtils;
    utils.setDesktopModeViewport(aMode === USER_AGENT_MODE_DESKTOP);
  }

  get userAgentOverride() {
    return this._userAgentOverride;
  }

  set userAgentOverride(aUserAgent) {
    if (aUserAgent === this._userAgentOverride) {
      return;
    }
    this._userAgentOverride = aUserAgent;
    const docShell = content && GeckoViewUtils.getRootDocShell(content);
    if (docShell) {
      docShell.customUserAgent = this.userAgent;
    } else {
      warn `Failed to set custom user agent. Doc shell not found`;
    }
    const utils = content.windowUtils;
    if (aUserAgent === null) {
      utils.setDesktopModeViewport(this._userAgentMode === USER_AGENT_MODE_DESKTOP);
      return;
    }
    utils.setDesktopModeViewport(false);
  }

  get displayMode() {
    const docShell = content && GeckoViewUtils.getRootDocShell(content);
    return docShell ? docShell.displayMode
                    : Ci.nsIDocShell.DISPLAY_MODE_BROWSER;
  }

  set displayMode(aMode) {
    const docShell = content && GeckoViewUtils.getRootDocShell(content);
    if (docShell) {
      docShell.displayMode = aMode;
    }
  }

  get allowJavascript() {
    return docShell.allowJavascript;
  }

  set allowJavascript(aAllowJavascript) {
    docShell.allowJavascript = aAllowJavascript;
  }
}

let {debug, warn} = GeckoViewSettingsChild.initLogging("GeckoViewSettings");
let module = GeckoViewSettingsChild.create(this);

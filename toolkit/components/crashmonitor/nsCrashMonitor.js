/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

var Scope = {};
ChromeUtils.import("resource://gre/modules/CrashMonitor.jsm", Scope);
var MonitorAPI = Scope.CrashMonitor;

function CrashMonitor() {}

CrashMonitor.prototype = {

  classID: Components.ID("{d9d75e86-8f17-4c57-993e-f738f0d86d42}"),
  contractID: "@mozilla.org/toolkit/crashmonitor;1",

  QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver]),

  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
    case "profile-after-change":
      MonitorAPI.init();
    }
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([CrashMonitor]);

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

function DEBUGLOG(str) {
	if (typeof str === "string") {
	  console.log(str);
	} else {
	  console.log(JSON.stringify(str));
	}
	console.log("\n");
}

let Profiler = {

	initialize: function() {
		this._profiler = Cc["@mozilla.org/tools/profiler;1"].getService(Ci.nsIProfiler);
	},

	_profiler_start: function(entries, interval, features, threads) {
    if (threads.length) {
      try {
        this._profiler.StartProfiler(entries, interval, features, features.length, threads, threads.length);
        return;
      } catch (e) {
      }
    }
    this._profiler.StartProfiler(entries, interval, features, features.length);  
  },

  start: function profiler_start(entries, interval, features, threads, cb) {
  	this._profiler_start(entries, interval, features, threads);
  },

  stop: function profiler_stop(cb) {
    this._profiler.StopProfiler();
    if (cb) cb.call(this);
  },

  getFeatures: function profiler_getFeatures(cb) {
    let features = this._profiler.GetFeatures([]);
    if (cb) cb.call(this, features);
  },

  getProfile: function profiler_getProfile(cb) {
    let profile = this._profiler.getProfileData();
    if (cb) cb.call(this, profile);
  },

  shutdown: function profiler_shutdown() {
  
  },

  isActive: function profiler_isActive(cb) {
    let isActive = this._profiler.IsActive();
    if (cb) cb.call(this, isActive);
  },

  startProfile: function() {
  	var selectedFeatures = [];
		selectedFeatures.push("leaf");
		// selectedFeatures.push("js");
		// selectedFeatures.push("stackwalk");
		// selectedFeatures.push("adb");
		// selectedFeatures.push("gc");
		// selectedFeatures.push("performanceReporter");
		selectedFeatures.push("gpu");
		var entries = 1000000;
		var interval = 1;
		var threadFilter = 'GeckoMain,Compositor';  // For test

		DEBUGLOG("Profiler: Start to profile.");

		this.start(entries, interval, selectedFeatures, threadFilter.split(','));    
	},

  stopProfile: function() {

		this.isActive(function(isActive) {
			DEBUGLOG("isActive: " + JSON.stringify(isActive));
		});

		this.getProfile(function(profile) {
	  	DEBUGLOG("PROFILERDEBUG: " + JSON.stringify(profile));
		});

		this.stop();

		DEBUGLOG("stop profile...");

		this.isActive(function(isActive) {
			DEBUGLOG("isActive: " + JSON.stringify(isActive));
		});

	},

}


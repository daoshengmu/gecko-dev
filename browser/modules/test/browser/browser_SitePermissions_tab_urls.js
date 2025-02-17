/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource:///modules/SitePermissions.jsm", this);

function newURI(url) {
  return Services.io.newURI(url);
}

// This tests the key used to store the URI -> permission map on a tab.
add_task(async function testTemporaryPermissionTabURLs() {

  // Prevent showing a dialog for https://name:password@example.com
  SpecialPowers.pushPrefEnv({set: [
        ["network.http.phishy-userpass-length", 2048],
  ]});

  // This usually takes about 60 seconds on 32bit Linux debug,
  // due to the combinatory nature of the test that is hard to fix.
  requestLongerTimeout(2);


  let same = [ newURI("https://example.com"),
               newURI("https://example.com/sub/path"),
               newURI("https://example.com:443") ];
  let different = [ newURI("https://example.com"),
                    newURI("https://name:password@example.com"),
                    newURI("https://test1.example.com"),
                    newURI("http://example.com"),
                    newURI("http://example.org") ];

  let id = "microphone";

  await BrowserTestUtils.withNewTab("about:blank", async function(browser) {
    for (let uri of same) {
        let loaded = BrowserTestUtils.browserLoaded(browser, false, uri.spec);
        BrowserTestUtils.loadURI(browser, uri.spec);
        await loaded;

        SitePermissions.set(uri, id, SitePermissions.BLOCK, SitePermissions.SCOPE_TEMPORARY, browser);

        for (let uri2 of same) {
          let loaded2 = BrowserTestUtils.browserLoaded(browser, false, uri2.spec);
          BrowserTestUtils.loadURI(browser, uri2.spec);
          await loaded2;

          Assert.deepEqual(SitePermissions.get(uri2, id, browser), {
            state: SitePermissions.BLOCK,
            scope: SitePermissions.SCOPE_TEMPORARY,
          }, `${uri.spec} should share tab permissions with ${uri2.spec}`);
        }

        SitePermissions.clearTemporaryPermissions(browser);
    }

    for (let uri of different) {
      let loaded = BrowserTestUtils.browserLoaded(browser, false, uri.spec);
      BrowserTestUtils.loadURI(browser, uri.spec);
      await loaded;

      SitePermissions.set(uri, id, SitePermissions.BLOCK, SitePermissions.SCOPE_TEMPORARY, browser);

      Assert.deepEqual(SitePermissions.get(uri, id, browser), {
        state: SitePermissions.BLOCK,
        scope: SitePermissions.SCOPE_TEMPORARY,
      });

      for (let uri2 of different) {
        loaded = BrowserTestUtils.browserLoaded(browser, false, uri2.spec);
        BrowserTestUtils.loadURI(browser, uri2.spec);
        await loaded;

        if (uri2 != uri) {
          Assert.deepEqual(SitePermissions.get(uri2, id, browser), {
            state: SitePermissions.UNKNOWN,
            scope: SitePermissions.SCOPE_PERSISTENT,
          }, `${uri.spec} should not share tab permissions with ${uri2.spec}`);
        }
      }

      SitePermissions.clearTemporaryPermissions(browser);
    }
  });

});


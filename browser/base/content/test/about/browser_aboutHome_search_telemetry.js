/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

ignoreAllUncaughtExceptions();

add_task(async function() {
  info("Check that performing a search fires a search event and records to Telemetry.");

  await BrowserTestUtils.withNewTab({ gBrowser, url: "about:home" }, async function(browser) {
    let currEngine = Services.search.defaultEngine;
    let engine = await promiseNewEngine("searchSuggestionEngine.xml");
    // Make this actually work in healthreport by giving it an ID:
    Object.defineProperty(engine.wrappedJSObject, "identifier",
                          { value: "org.mozilla.testsearchsuggestions" });

    let p = promiseContentSearchChange(browser, engine.name);
    Services.search.defaultEngine = engine;
    await p;

    await ContentTask.spawn(browser, { expectedName: engine.name }, async function(args) {
      let engineName = content.wrappedJSObject.gContentSearchController.defaultEngine.name;
      is(engineName, args.expectedName, "Engine name in DOM should match engine we just added");
    });

    let numSearchesBefore = 0;
    // Get the current number of recorded searches.
    let histogramKey = engine.identifier + ".abouthome";
    try {
      let hs = Services.telemetry.getKeyedHistogramById("SEARCH_COUNTS").snapshot();
      if (histogramKey in hs) {
        numSearchesBefore = hs[histogramKey].sum;
      }
    } catch (ex) {
      // No searches performed yet, not a problem, |numSearchesBefore| is 0.
    }

    let searchStr = "a search";

    let expectedURL = Services.search.defaultEngine
      .getSubmission(searchStr, null, "homepage").uri.spec;
    let promise = waitForDocLoadAndStopIt(expectedURL, browser);

    // Perform a search to increase the SEARCH_COUNT histogram.
    await ContentTask.spawn(browser, { searchStr }, async function(args) {
      let doc = content.document;
      info("Perform a search.");
      let el = doc.querySelector(["#searchText", "#newtab-search-text"]);
      el.value = args.searchStr;
      doc.getElementById("searchSubmit").click();
    });

    await promise;

    // Make sure the SEARCH_COUNTS histogram has the right key and count.
    let hs = Services.telemetry.getKeyedHistogramById("SEARCH_COUNTS").snapshot();
    Assert.ok(histogramKey in hs, "histogram with key should be recorded");
    Assert.equal(hs[histogramKey].sum, numSearchesBefore + 1,
                 "histogram sum should be incremented");

    Services.search.defaultEngine = currEngine;
    try {
      Services.search.removeEngine(engine);
    } catch (ex) {}
  });
});

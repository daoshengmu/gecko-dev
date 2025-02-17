var policy; // To make sure we never leave up an activated domain policy after a failed test, let's make this global.
function activateDomainPolicy() {
  const ssm = Services.scriptSecurityManager;
  policy = ssm.activateDomainPolicy();
}

function deactivateDomainPolicy() {
  if (policy) {
    policy.deactivate();
    policy = null;
  }
}

async function test_domainPolicy() {

  ChromeUtils.defineModuleGetter(this, "Promise", "resource://gre/modules/Promise.jsm");
  let outerDeferred = Promise.defer();
  let currentTask = outerDeferred.promise;
  SpecialPowers.pushPrefEnv(
    {set: [["dom.ipc.browser_frames.oop_by_default", false],
          ["browser.pagethumbnails.capturing_disabled", false],
          ["dom.mozBrowserFramesEnabled", false]]},
    () => { return outerDeferred.resolve(); });
  await currentTask;

  // Create tab
  let tab;

  // Init test
  function initProcess() {
    tab = BrowserTestUtils.addTab(gBrowser);
    gBrowser.selectedTab = tab;

    let initPromise = ContentTask.spawn(tab.linkedBrowser, null, function() {
      ChromeUtils.import("resource://gre/modules/PromiseUtils.jsm");
      function loadBase() {
        let deferred = PromiseUtils.defer();
        let listener = (event) => {
          removeEventListener("DOMDocElementInserted", listener, true);
          let listener2 = (event2) => {
            content.removeEventListener("load", listener2);
            deferred.resolve();
          };
          content.addEventListener("load", listener2);
        };
        addEventListener("DOMDocElementInserted", listener, true);
        return deferred.promise;
      }

      return loadBase();
    });
    BrowserTestUtils.loadURI(tab.linkedBrowser, "http://mochi.test:8888/browser/dom/ipc/tests/file_domainPolicy_base.html");
    return initPromise;
  }

  // We use ContentTask for the tests, but we also want to pass some data and some helper functions too.
  // To do that, we serialize an input object via JSON |ipcArgs| and some shared helper functions |initUtils|
  // and eval them in the content process.
  var ipcArgs = {};
  function initUtils(obj) {
    obj.checkScriptEnabled = function(win, expectEnabled) {
      win.wrappedJSObject.gFiredOnclick = false;
      win.document.body.dispatchEvent(new win.Event("click"));
      return { passed: win.wrappedJSObject.gFiredOnclick == expectEnabled,
               msg: `Checking script-enabled for ${win.name} (${win.location})`};
    };

    obj.navigateFrame = function(ifr, src) {
      let deferred = PromiseUtils.defer();
      function onload() {
        ifr.removeEventListener("load", onload);
        deferred.resolve();
      }
      ifr.addEventListener("load", onload);
      ifr.setAttribute("src", src);
      return deferred.promise;
    };
  }

  function runTest(test) {
    return ContentTask.spawn(tab.linkedBrowser,
      "ipcArgs = " + JSON.stringify(ipcArgs) + "; (" + initUtils.toSource() + ")(utils)", test);
  }

  function checkAndCleanup(result) {
    result = [].concat(result);
    for (var i in result)
      ok(result[i].passed, result[i].msg);
    gBrowser.removeTab(tab);
    deactivateDomainPolicy();
    ipcArgs = {};
  }

  function testDomain(domain) {
    ipcArgs.domain = domain;
    return (aUtils) => {
      ChromeUtils.import("resource://gre/modules/PromiseUtils.jsm");
      // eslint-disable-next-line no-shadow
      var ipcArgs;
      var utils = {};
      // eslint-disable-next-line no-eval
      eval(aUtils);

      let path = "/browser/dom/ipc/tests/file_disableScript.html";
      let deferred = PromiseUtils.defer();
      var rootFrame = content.document.getElementById("root");
      utils.navigateFrame(rootFrame, ipcArgs.domain + path).then(() => {
        deferred.resolve(utils.checkScriptEnabled(rootFrame.contentWindow, false));
      });
      return deferred.promise;
    };
  }

  info("Testing simple blocklist policy");

  info("Creating child process first, activating domainPolicy after");
  currentTask = initProcess();
  await currentTask;
  activateDomainPolicy();
  var bl = policy.blocklist;
  bl.add(Services.io.newURI("http://example.com"));
  currentTask = runTest(testDomain("http://example.com"));
  checkAndCleanup(await currentTask);

  info("Activating domainPolicy first, creating child process after");
  activateDomainPolicy();
  bl = policy.blocklist;
  bl.add(Services.io.newURI("http://example.com"));
  currentTask = initProcess();
  await currentTask;
  currentTask = runTest(testDomain("http://example.com"));
  checkAndCleanup(await currentTask);

  function testList(expectEnabled, list) {
    ipcArgs.expectEnabled = expectEnabled;
    ipcArgs.list = list;
    return (aUtils) => {
      ChromeUtils.import("resource://gre/modules/PromiseUtils.jsm");
      // eslint-disable-next-line no-shadow
      var ipcArgs;
      var utils = {};
      // eslint-disable-next-line no-eval
      eval(aUtils);

      var results = [];
      var testListInternal = function(internalExpectEnabled, internalList, idx) {
        idx = idx || 0;
        let deferred = PromiseUtils.defer();
        let path = "/browser/dom/ipc/tests/file_disableScript.html";
        let target = internalList[idx] + path;
        var rootFrame = content.document.getElementById("root");
        utils.navigateFrame(rootFrame, target).then(function() {
          results.push(utils.checkScriptEnabled(rootFrame.contentWindow, internalExpectEnabled));
          if (idx == internalList.length - 1)
            deferred.resolve(results);
          else
            testListInternal(internalExpectEnabled, internalList, idx + 1).then(function(retArg) { deferred.resolve(retArg); });
        });
        return deferred.promise;
      };
      return testListInternal(ipcArgs.expectEnabled, ipcArgs.list);
    };
  }

  let testPolicy = {
     exceptions: ["http://test1.example.com", "http://example.com"],
     superExceptions: ["http://test2.example.org", "https://test1.example.com"],
     exempt: ["http://test1.example.com", "http://example.com",
              "http://test2.example.org", "http://sub1.test2.example.org",
              "https://sub1.test1.example.com"],
     notExempt: ["http://test2.example.com", "http://sub1.test1.example.com",
                 "http://www.example.com", "https://test2.example.com",
                 "https://example.com", "http://test1.example.org"],
  };

  function activate(isBlock, exceptions, superExceptions) {
    activateDomainPolicy();
    let set = isBlock ? policy.blocklist : policy.allowlist;
    let superSet = isBlock ? policy.superBlocklist : policy.superAllowlist;
    for (let e of exceptions)
      set.add(makeURI(e));
    for (let e of superExceptions)
      superSet.add(makeURI(e));
  }

  info("Testing Blocklist-style Domain Policy");
  info("Activating domainPolicy first, creating child process after");
  activate(true, testPolicy.exceptions, testPolicy.superExceptions);
  currentTask = initProcess();
  await currentTask;
  let results = [];
  currentTask = runTest(testList(true, testPolicy.notExempt));
  results = results.concat(await currentTask);
  currentTask = runTest(testList(false, testPolicy.exempt));
  results = results.concat(await currentTask);
  checkAndCleanup(results);

  info("Creating child process first, activating domainPolicy after");
  currentTask = initProcess();
  await currentTask;
  activate(true, testPolicy.exceptions, testPolicy.superExceptions);
  results = [];
  currentTask = runTest(testList(true, testPolicy.notExempt));
  results = results.concat(await currentTask);
  currentTask = runTest(testList(false, testPolicy.exempt));
  results = results.concat(await currentTask);
  checkAndCleanup(results);

  info("Testing Allowlist-style Domain Policy");
  let deferred = Promise.defer();
  currentTask = deferred.promise;
  SpecialPowers.pushPrefEnv({set: [["javascript.enabled", false]]}, () => { return deferred.resolve(); });
  await currentTask;

  info("Activating domainPolicy first, creating child process after");
  activate(false, testPolicy.exceptions, testPolicy.superExceptions);
  currentTask = initProcess();
  await currentTask;
  results = [];
  currentTask = runTest(testList(false, testPolicy.notExempt));
  results = results.concat(await currentTask);
  currentTask = runTest(testList(true, testPolicy.exempt));
  results = results.concat(await currentTask);
  checkAndCleanup(results);

  info("Creating child process first, activating domainPolicy after");
  currentTask = initProcess();
  await currentTask;
  activate(false, testPolicy.exceptions, testPolicy.superExceptions);
  results = [];
  currentTask = runTest(testList(false, testPolicy.notExempt));
  results = results.concat(await currentTask);
  currentTask = runTest(testList(true, testPolicy.exempt));
  results = results.concat(await currentTask);
  checkAndCleanup(results);
  finish();
}


add_task(test_domainPolicy);

registerCleanupFunction(() => {
  deactivateDomainPolicy();
});

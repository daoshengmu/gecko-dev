This sample refers from https://github.com/ChunMinChang/mozDevDoc/blob/master/Creating_a_WebIDL.md

## Setup firefox nightly in multiprocess mode

1. Open firefox nightly by ```$ ./mach run```
2. Enter *about:config* on URL
3. Click the button: *I'll be careful, I promise!*
4. Search *browser.tabs.remote.autostart* and set it to *true*

<pre>
$ cd MOZ_CEN
$ ./mach run ./practices/WebIDL/testHello.html
</pre>
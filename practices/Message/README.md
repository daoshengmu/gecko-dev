This sample refers from https://github.com/ChunMinChang/mozDevDoc/blob/master/Try_message_manager.md

Copy files from ./practices/Message to ./dom/events/test

<pre>
$ cd MOZ_CEN

$ ./mach mochitest-plain dom/events/test/test_dom_before_after_keyboard_event.html
…
…
-------------------------------
[Dad] Hi! Child! You say: Good night
-------------------------------
[child] get: Good Child
-------------------------------
…
…
</pre>
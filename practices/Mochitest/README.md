
This sample refers from https://github.com/ChunMinChang/mozDevDoc/blob/master/Creating_a_Test_Case.mdTry_message_manager.md

Copy files from ./practices/Mochitest to ./dom

## Add it to the build tree
<pre>
$ vim MOZ_CEN/dom/moz.build
</pre> 

<pre>
DIRS += [
    'animation',
    'apps',
...
...
    'manifest',
    'vr',
    'helloTest',
]
...
...
</pre> 

## Running the specific test

<pre>
$ cd MOZ_CEN
$ ./mach mochitest-plain dom/helloTest/test/test_bug123456.html
</pre>
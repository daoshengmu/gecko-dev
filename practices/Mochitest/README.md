
This sample refers from https://github.com/ChunMinChang/mozDevDoc/blob/master/Creating_a_Test_Case.mdTry_message_manager.md

First of all, Copy files from ```./practices/Mochitest``` to ```./dom```

### Note
If your test code that is generated from ```mochitest/gen_template.pl```, you have to add the below code to your ```test_bugxxxxxx.html```.
```
<script type="application/javascript" src="/tests/SimpleTest/SimpleTest.js"></script>
<link rel="stylesheet" type="text/css" href="/tests/SimpleTest/test.css"/>
```

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

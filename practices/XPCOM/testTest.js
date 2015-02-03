
function() testFunc() {
	const cTest = new Components.Constructor("@mozilla.org/test;1","nsITest");
	var test = new cTest();
	test.value
}

testFunc();

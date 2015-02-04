
function testFunc() {
	
	const cTest = new Components.Constructor("@mozilla.org/test;1","nsITest");
	var test = new cTest();
	test.value="Mama"
	test.writeValue("Call my")
	
	test.add(5,23)
	test.sub(5,23)
	test.mul(5,23)
	test.mul(test.add(5,23), test.sub(5,23));
}

testFunc();

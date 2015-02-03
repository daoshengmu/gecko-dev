
function testFunc() {

	const cSample = new Components.Constructor("@mozilla.org/sample;1", "nsISample");var 

	sample = new cSample();

	sample.writeValue("Hello")

	sample.writeValue("yo man!")

	sample.poke("high five");

	sample.writeValue("yo! man!");

}


testFunc();





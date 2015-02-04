
[Constructor,Constructor(DOMString str)]
interface Hello {
    attribute DOMString helloStr;
    readonly attribute DOMString readonlyStr;
    DOMString sayHello();
    long randNum();
    long add(long a, long b);
    long mul(long a, long b);
    long sub(long a, long b);
    [Throws]
    double div(long a, long b);
};
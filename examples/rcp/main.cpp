#include <stdio.h>
#include "Teuchos_RCP.hpp"

using Teuchos::RCP;
using Teuchos::rcp;
using Teuchos::null;


class A {
    int a;
public:
    A() {
        a = 0;
    }
    ~A() {}

    void test() {
        printf("A.test(), a=%d\n", this->a);
        this->a++;
    }
};


RCP<A> create_A() {
    RCP<A> a;
    a = rcp(new A());
    return a;
}


RCP<A> create_null() {
    return null;
}


int main()
{
    try {
        RCP<A> a = create_A();
        // works:
        a->test();
        RCP<A> b = create_null();
        // will raise an exception:
        b->test();
        return 0;
    }
    catch(const std::exception &except) {
        std::cout
            << "Caught exception of type '"<<typeid(except).name()<<"':\n"
            << except.what()
            << "\n";
    }
    return 1;
}

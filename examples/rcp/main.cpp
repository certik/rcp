#include <stdio.h>
#include "Teuchos_RCP.hpp"

using Teuchos::RCP;
using Teuchos::rcp;

class A {
public:
    A() {}
    ~A() {}

    void test() {
        printf("A.test()\n");
    }
};

RCP<A> create_A() {
    RCP<A> a;
    a = rcp(new A());
    return a;
}

int main()
{
    RCP<A> a = create_A();
    a->test();
    return 0;
}

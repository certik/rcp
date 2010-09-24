#include "Teuchos_stacktrace.hpp"

void g()
{
    Teuchos::get_backtrace();
}

void f()
{
    g();
}

int main()
{
    // Will keep calling get_backtrace() and you can observe in top if the
    // memory is rising or not:
    for (int i =0; i< 100000; i++)
        f();

    return 0;
}

#include "Teuchos_stacktrace.hpp"

void g()
{
    show_backtrace();
}

void f()
{
    g();
}

int main()
{
    print_stack_on_segfault();
    f();

    // This will segfault:
    char *p = NULL; *p = 0;

    return 0;
}

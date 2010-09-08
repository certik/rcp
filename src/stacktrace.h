#ifndef _STACKTRACE_H_
#define _STACKTRACE_H_

#include <string>

void show_backtrace();
void print_stack_on_segfault();
std::string get_backtrace();

#endif

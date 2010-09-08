How to compile
--------------
::

    $ cmake .
    $ make

How to test
-----------

Look into the
`main.cpp <http://github.com/certik/rcp/blob/master/examples/rcp/main.cpp>`_
first to see what it is doing, then compile it and run it::

    $ cd examples/rcp
    $ ./rcp
    A.test(), a=0
    terminate called after throwing an instance of 'Teuchos::NullReferenceError'
      what():  /home/ondrej/repos/rcp/src/Teuchos_RCPNode.cpp:707:

    Throw number = 1

    Throw test that evaluated to true: true

    Traceback (most recent call last):
      File "??", line 0, in _start()
      File "/build/buildd/eglibc-2.11.1/csu/libc-start.c", line 226, in __libc_start_main()
      File "/home/ondrej/repos/rcp/examples/rcp/main.cpp", line 39, in main()
            b->test();
      File "/home/ondrej/repos/rcp/src/Teuchos_RCP.hpp", line 259, in Teuchos::RCP<A>::operator->() const
          debug_assert_not_null();
      File "/home/ondrej/repos/rcp/src/Teuchos_RCPDecl.hpp", line 806, in Teuchos::RCP<A>::debug_assert_not_null() const
              assert_not_null();
      File "/home/ondrej/repos/rcp/src/Teuchos_RCP.hpp", line 428, in Teuchos::RCP<A>::assert_not_null() const
            throw_null_ptr_error(typeName(*this));
      File "/home/ondrej/repos/rcp/src/Teuchos_RCPNode.cpp", line 704, in Teuchos::throw_null_ptr_error(std::string const&)
          TEST_FOR_EXCEPTION(
      File "/home/ondrej/repos/rcp/src/stacktrace.cpp", line 322, in get_backtrace()
            size = backtrace(array, 100);

    Teuchos::RCP<A> : You can not call operator->() or operator*() if getRawPtr()==0!
    Aborted


TODO
----

Embed the stacktrace into the exception, polish the stacktrace.cpp so that it
doesn't leak.

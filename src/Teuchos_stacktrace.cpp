/*
Copyright (c) 2010, Ondrej Certik
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.
* Neither the name of the Sandia Corporation nor the names of its contributors
  may be used to endorse or promote products derived from this software without
  specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include <string>
#include <iostream>
#include <fstream>

// free() and abort() functions
#include <cstdlib>

// For handling variable number of arguments using va_start/va_end functions
#include <cstdarg>

// For registering SIGSEGV callbacks
#include <csignal>

#include "Teuchos_stacktrace.hpp"


// The following C headers are needed for some specific C functionality (see
// the comments), which is not available in C++:

// backtrace() function for retrieving the backtrace
#include <execinfo.h>

// For bfd_* family of functions for loading debugging symbols from the binary
#include <bfd.h>

// For dl_iterate_phdr() functionality
#include <link.h>

// For demangling function names
#include <cxxabi.h>


/* This struct is used to pass information between
   addr2str() and process_section().  */

struct line_data {
    asymbol **symbol_table;     /* Symbol table.  */

    bfd_vma addr;
    std::string filename;
    std::string function_name;
    unsigned int line;
    int line_found;
};

/*
   Reads the 'line_number'th line from the file filename.
*/
std::string read_line_from_file(std::string filename, unsigned int line_number)
{
    std::ifstream in(filename.c_str());
    if (!in.is_open())
        return "";
    if (line_number == 0)
        return "Line number must be positive";
    unsigned int n = 0;
    std::string line;
    while (n < line_number) {
        n += 1;
        if (in.eof())
            return "Line not found";
        getline(in, line);
    }
    return line;
}

/*
   Allows printf like formatting, but returns a std::string.
*/
std::string format(const char *fmt, ...)
{
    va_list argptr;
    va_start(argptr, fmt);

    char *str;
    vasprintf(&str, fmt, argptr);
    std::string s=str;
    free(str);

    va_end(argptr);
    return s;
}

/*
   Demangles the function name if needed (if the 'name' is coming from C, it
   doesn't have to be demangled, if it's coming from C++, it needs to be).

   Makes sure that it ends with (), which is automatic in C++, but it has to be
   added by hand in C.
   */
std::string demangle_function_name(std::string name)
{
    std::string s;

    if (name.length() == 0) {
        s = "??";
    } else {
        int status = 0;
        char *d = 0;
        d = abi::__cxa_demangle(name.c_str(), 0, 0, &status);
        if (d) {
            s = d;
            free(d);
        } else {
            s = name + "()";
        }
    }

    return s;
}

/* Look for an address in a section.  This is called via
   bfd_map_over_sections over all sections in abfd.

   If the correct line is found, store the result in 'data' and set
   data->line_found, so that subsequent calls to process_section exit
   immediately.
 */

static void process_section(bfd *abfd, asection *section, void *_data)
{
    line_data *data = (line_data*)_data;
    if (data->line_found)
        // If we already found the line, exit
        return;
    if ((bfd_get_section_flags(abfd, section) & SEC_ALLOC) == 0)
        return;

    bfd_vma vma = bfd_get_section_vma(abfd, section);
    if (data->addr < vma)
        // If the addr lies above the section, exit
        return;

    bfd_size_type size = bfd_section_size(abfd, section);
    if (data->addr >= vma + size)
        // If the addr lies below the section, exit
        return;

    // Calculate the correct offset of our line in the section
    bfd_vma offset = data->addr - vma - 1;

    // Finds the line corresponding to the offset
    const char *filename, *function_name;
    data->line_found = bfd_find_nearest_line(abfd, section, data->symbol_table,
            offset, &filename, &function_name, &data->line);
    if (filename == NULL)
        data->filename = "";
    else
        data->filename = filename;
    data->function_name = function_name;
}

/* Loads the symbol table into 'data->symbol_table'.  */

static int load_symbol_table(bfd *abfd, line_data *data)
{
    if ((bfd_get_file_flags(abfd) & HAS_SYMS) == 0)
        // If we don't have any symbols, return
        return 0;

    void **tmp = (void **) &(data->symbol_table);
    long n_symbols;
    unsigned int symbol_size;
    n_symbols = bfd_read_minisymbols(abfd, false, tmp, &symbol_size);
    if (n_symbols == 0)
        // dynamic
        n_symbols = bfd_read_minisymbols(abfd, true, tmp, &symbol_size);

    if (n_symbols < 0)
        // bfd_read_minisymbols() failed
        return 1;
    return 0;
}



/*
   Returns a string of 2 lines for the function with address 'addr' in the file
   'file_name'. Example:

     File "/home/ondrej/repos/rcp/src/Teuchos_RCP.hpp", line 428, in Teuchos::RCP<A>::assert_not_null() const
         throw_null_ptr_error(typeName(*this));

   */
static std::string addr2str(std::string file_name, bfd_vma addr)
{
    // Initialize 'abfd' and do some sanity checks
    bfd *abfd;
    abfd = bfd_openr(file_name.c_str(), NULL);
    if (abfd == NULL)
        return "Cannot open the binary file '" + file_name + "'\n";
    if (bfd_check_format(abfd, bfd_archive))
        return "Cannot get addresses from the archive '" + file_name + "'\n";
    char **matching;
    if (!bfd_check_format_matches(abfd, bfd_object, &matching))
        return "Unknown format of the binary file '" + file_name + "'\n";
    line_data data;
    data.addr = addr;
    data.symbol_table = NULL;
    data.line_found = false;
    // This allocates the symbol_table:
    if (load_symbol_table(abfd, &data) == 1)
        return "Failed to load the symbol table from '" + file_name + "'\n";
    // Loops over all sections and try to find the line
    bfd_map_over_sections(abfd, process_section, &data);
    // Deallocates the symbol table
    if (data.symbol_table != NULL) free(data.symbol_table);
    bfd_close(abfd);

    std::string s;
    // Do the printing --- print as much information as we were able to
    // find out
    if (!data.line_found) {
        // If we didn't find the line, at least print the address itself
        s = format("  File unknown, address: 0x%llx",
                (long long unsigned int) addr);
    } else {
        std::string name=demangle_function_name(data.function_name);
        if (data.filename.length() > 0) {
            // Nicely format the filename + function name + line
            s = format("  File \"%s\", line %u, in %s", data.filename.c_str(),
                    data.line, name.c_str());
            std::string line_text=read_line_from_file(data.filename,
                    data.line);
            if (line_text != "") {
                s += "\n    ";
                s += line_text;
            }
        } else {
            // The file is unknown (and data.line == 0 in this case), so the
            // only meaningful thing to print is the function name:
            s = format("  File unknown, in %s", name.c_str());
        }
    }
    s += "\n";
    return s;
}

struct match_data {
    bfd_vma addr;

    std::string filename;
    bfd_vma addr_in_file;
};

/*
   Tries to find the 'match.addr' in the current shared lib (as passed in
   'info'). If it succeeds, returns (in the 'data') the full path to the shared
   lib and the local address in the file.
*/
static int shared_lib_callback(struct dl_phdr_info *info,
        size_t size, void *data)
{
    struct match_data *match = (struct match_data *)data;
    for (int i=0; i < info->dlpi_phnum; i++) {
        if (info->dlpi_phdr[i].p_type == PT_LOAD) {
            ElfW(Addr) vaddr = info->dlpi_addr + info->dlpi_phdr[i].p_vaddr;
            if ((match->addr >= vaddr) &&
                        (match->addr < vaddr + info->dlpi_phdr[i].p_memsz)) {
                match->filename = info->dlpi_name;
                match->addr_in_file = match->addr - info->dlpi_addr;
                // We found a match, return a non-zero value
                return 1;
            }
        }
    }
    // We didn't find a match, return a zero value
    return 0;
}

/*
   Returns a std::string with the stacktrace corresponding to the
   list of addresses (of functions on the stack) in 'buffer'.

   It converts addresses to filenames, line numbers, function names and the
   line text.
   */
std::string backtrace2str(void *const *buffer, int size)
{
    int stack_depth = size - 1;

    std::string final;

    bfd_init();
    // Loop over the stack
    for (int i=stack_depth; i >= 0; i--) {
        // Iterate over all loaded shared libraries (see dl_iterate_phdr(3) -
        // Linux man page for more documentation)
        struct match_data match;
        match.addr = (bfd_vma) buffer[i];
        if (dl_iterate_phdr(shared_lib_callback, &match) == 0)
            return "dl_iterate_phdr() didn't find a match\n";

        if (match.filename.length() > 0)
            // This happens for shared libraries (like /lib/libc.so.6, or any
            // other shared library that the project uses). 'match.filename'
            // then contains the full path to the .so library.
            final += addr2str(match.filename, match.addr_in_file);
        else
            // The 'addr_in_file' is from the current executable binary, that
            // one can find at '/proc/self/exe'. So we'll use that.
            final += addr2str("/proc/self/exe", match.addr_in_file);
    }

    return final;
}


/* Returns the backtrace as a std::string. */
std::string Teuchos::get_backtrace()
{
    void *array[100];
    size_t size;
    std::string strings;

    // Obtain the list of addresses
    size = backtrace(array, 100);
    strings = backtrace2str(array, size);

    // Print it in a Python like fashion:
    std::string s("Traceback (most recent call last):\n");
    s += strings;
    return s;
}

/* Obtain a backtrace and print it to stdout. */
void Teuchos::show_backtrace()
{
    std::cout << Teuchos::get_backtrace();
}

void _segfault_callback_print_stack(int sig_num)
{
    std::cout << "\nSegfault caught. Printing stacktrace:\n\n";
    Teuchos::show_backtrace();
    std::cout << "\nDone. Exiting the program.\n";
    // Deregister our abort callback:
    signal(SIGABRT, SIG_DFL);
    abort();
}

void _abort_callback_print_stack(int sig_num)
{
    std::cout << "\nAbort caught. Printing stacktrace:\n\n";
    Teuchos::show_backtrace();
    std::cout << "\nDone.\n";
}

void Teuchos::print_stack_on_segfault()
{
    signal(SIGSEGV, _segfault_callback_print_stack);
    signal(SIGABRT, _abort_callback_print_stack);
}

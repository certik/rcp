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

#include <string.h>
#include <stdlib.h>
#include <execinfo.h>
#include <bfd.h>
#define HAVE_DECL_BASENAME 1
#include <libiberty.h>
#include <link.h>

#include <cxxabi.h>
#include <string>
#include <iostream>
#include <fstream>
#include <signal.h>

#define fatal(a) exit(1)

/* These class is used to pass information between
   translate_addresses_buf and find_address_in_section.  */

struct line_data {
    bfd_vma addr;
    const char *filename;
    const char *functionname;
    unsigned int line;
    int line_found;
    asymbol **symbol_table;     /* Symbol table.  */
};

/*
   Reads the 'line_number'th line from the file filename.
*/
std::string read_line_from_file(const char *filename, unsigned int line_number)
{
    std::ifstream in(filename);
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
std::string demangle_function_name(const char *name)
{
    std::string s;

    if (name == NULL || *name == '\0') {
        s = "??";
    } else {
        int status = 0;
        char *d = 0;
        d = abi::__cxa_demangle(name, 0, 0, &status);
        if (d) {
            s = d;
        } else {
            s = name;
            s += "()";
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
    data->line_found = bfd_find_nearest_line(abfd, section, data->symbol_table,
            offset, &data->filename, &data->functionname, &data->line);
}

/* Loads the symbol table into 'data->symbol_table'.  */

static void slurp_symtab(bfd * abfd, line_data *data)
{
    long symcount;
    unsigned int size;

    if ((bfd_get_file_flags(abfd) & HAS_SYMS) == 0)
        return;

    void **tmp = (void **) &(data->symbol_table);
    symcount = bfd_read_minisymbols(abfd, false, tmp, &size);
    if (symcount == 0)
        symcount = bfd_read_minisymbols(abfd, true /* dynamic */, tmp, &size);

    if (symcount < 0)
        fatal("bfd_read_minisymbols() failed");
}



/*
   Returns a string of 2 lines for the function with address 'addr'. Example:

     File "/home/ondrej/repos/rcp/src/Teuchos_RCP.hpp", line 428, in Teuchos::RCP<A>::assert_not_null() const
         throw_null_ptr_error(typeName(*this));

   */
static std::string translate_addresses_buf(bfd *abfd, bfd_vma *addr)
{
    std::string s;
    line_data data;
    // Read the symbols
    slurp_symtab(abfd, &data);
    data.addr = addr[0];
    data.line_found = false;
    bfd_map_over_sections(abfd, process_section, &data);
    if (!data.line_found) {
        s = format("[0x%llx] \?\?() \?\?:0", (long long unsigned int) addr[0]);
    } else {
        std::string name=demangle_function_name(data.functionname);
        s = format("  File \"%s\", line %u, in %s",
                data.filename ? data.filename : "??", data.line,
                name.c_str());
        if (data.filename) {
            std::string line_text=read_line_from_file(data.filename,
                    data.line);
            if (line_text != "") {
                s += "\n    ";
                s += line_text;
            }
        }
    }
    // cleanup
    if (data.symbol_table != NULL) {
        free(data.symbol_table);
        data.symbol_table = NULL;
    }
    s += "\n";
    return s;
}

struct file_match {
    const char *file;
    void *address;
    void *base;
    void *hdr;
};

static int find_matching_file(struct dl_phdr_info *info,
        size_t size, void *data)
{
    struct file_match *match = (struct file_match *)data;
    /* This code is modeled from Gfind_proc_info-lsb.c:callback() from libunwind */
    long n;
    const ElfW(Phdr) *phdr;
    ElfW(Addr) load_base = info->dlpi_addr;
    phdr = info->dlpi_phdr;
    for (n = info->dlpi_phnum; --n >= 0; phdr++) {
        if (phdr->p_type == PT_LOAD) {
            ElfW(Addr) vaddr = phdr->p_vaddr + load_base;
            if ((long unsigned)(match->address) >= vaddr && (long unsigned)(match->address) < vaddr + phdr->p_memsz) {
                /* we found a match */
                match->file = info->dlpi_name;
                match->base = (void*)(info->dlpi_addr);
            }
        }
    }
    return 0;
}


/* Process a file.  */

static std::string process_file(const char *file_name, bfd_vma *addr)
{
    // Initialize 'abfd' and do some sanity checks
    bfd *abfd;
    abfd = bfd_openr(file_name, NULL);
    if (abfd == NULL)
        fatal("bfd_openr() failed");
    if (bfd_check_format(abfd, bfd_archive))
        fatal("Cannot get addresses from archive");
    char **matching;
    if (!bfd_check_format_matches(abfd, bfd_object, &matching))
        fatal("bfd_check_format_matches() failed");


    // Get nice representation of each address
    std::string s = translate_addresses_buf(abfd, addr);

    bfd_close(abfd);
    return s;
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
    int x;

    std::string final;

    bfd_init();
    for(x=stack_depth; x>=0; x--) {
        struct file_match match;
        match.address = buffer[x];
        bfd_vma addr;
        dl_iterate_phdr(find_matching_file, &match);
        addr = (bfd_vma)((long unsigned)(buffer[x])
                - (long unsigned)(match.base));
        if (match.file && strlen(match.file))
            // This happens for shared libraries (like /lib/libc.so.6, or any
            // other shared library that the project uses). 'match.file' then
            // contains the full path to the .so library.
            final += process_file(match.file, &addr);
        else
            // The 'addr' is from the current executable binary, which one can
            // find at '/proc/self/exe'. So we'll use that.
            final += process_file("/proc/self/exe", &addr);
    }

    return final;
}


/* Returns the backtrace as a std::string. */
std::string get_backtrace()
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
void show_backtrace()
{
    std::cout << get_backtrace();
}

void _segfault_callback_print_stack(int sig_num)
{
    std::cout << "\nSegfault caught. Printing stacktrace:\n\n";
    show_backtrace();
    std::cout << "\nDone. Exiting the program.\n";
    // Deregister our abort callback:
    signal(SIGABRT, SIG_DFL);
    abort();
}

void _abort_callback_print_stack(int sig_num)
{
    std::cout << "\nAbort caught. Printing stacktrace:\n\n";
    show_backtrace();
    std::cout << "\nDone.\n";
}

void print_stack_on_segfault()
{
    signal(SIGSEGV, _segfault_callback_print_stack);
    signal(SIGABRT, _abort_callback_print_stack);
}

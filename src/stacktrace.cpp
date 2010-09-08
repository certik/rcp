#include <string.h>
#include <stdlib.h>
#include <execinfo.h>
#include <bfd.h>
#define HAVE_DECL_BASENAME 1
#include <libiberty.h>
#include <link.h>

#include <cxxabi.h>
#include <string>
#include <signal.h>

#define fatal(a) exit(1)

/* These class is used to pass information between
   translate_addresses_buf and find_address_in_section.  */

class Data {
public:
    bfd_vma pc;
    const char *filename;
    const char *functionname;
    unsigned int line;
    int found;
    asymbol **syms;		/* Symbol table.  */
};

/**
 * Read a line from the given file pointer, stripping the traling newline.
 * NULL is returned if an error or EOF is encountered.
 */
#define INIT_BUF_SIZE 64

char *read_line(FILE *fp) {
    int buf_size = INIT_BUF_SIZE;
    char *buf = (char*)calloc(buf_size, sizeof(char)); *buf = 0;
    int tail_size = buf_size;
    char *tail = buf;

    /* successively read portions of the line into the tail of the buffer
     * (the empty section of the buffer following the text that has already
     * been read) until the end of the line is encountered */
    while(!feof(fp)) {
        if(fgets(tail, tail_size, fp) == NULL) {
            /* EOF or read error */
            free(buf);
            return NULL;
        }
        if(tail[strlen(tail)-1] == '\n') {
            /* end of line reached */
            break;
        }
        /* double size of buffer */
        tail_size = buf_size + 1; /* size of new tail */
        buf_size *= 2; /* increase size of buffer to fit new tail */
        buf = (char*)realloc(buf, buf_size * sizeof(char));
        tail = buf + buf_size - tail_size; /* point tail at null-terminator */
    }
    tail[strlen(tail)-1] = 0; /* remove trailing newline */
    return buf;
}

/*
   Reads the 'line_number'th line from the file filename.
*/
std::string read_line_from_file(const char *filename, unsigned int line_number)
{
    FILE *in = fopen(filename, "r");
    if (in == NULL)
        return "";
    if (line_number == 0)
        return "Line number must be positive";
    unsigned int n = 0;
    char *line;
    std::string s;
    while (n < line_number) {
        n += 1;
        line = read_line(in);
        if (line == NULL) {
            return "Line not found";
        }
        s = line;
        free(line);
    }
    return s;
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

    if (name == NULL || *name == '\0')
        s = "??";
    else {
        int status = 0;
        char *d = 0;
        d = abi::__cxa_demangle(name, 0, 0, &status);
        if (d)
            s = d;
        else {
            s = name;
            s += "()";
        }
    }

    return s;
}

/* Look for an address in a section.  This is called via
   bfd_map_over_sections.  */

static void find_address_in_section(bfd *abfd, asection *section,
        void *_data)
{
    Data *data = (Data*)_data;
	bfd_vma vma;
	bfd_size_type size;

	if (data->found)
		return;

	if ((bfd_get_section_flags(abfd, section) & SEC_ALLOC) == 0)
		return;

	vma = bfd_get_section_vma(abfd, section);
	if (data->pc < vma)
		return;

	size = bfd_section_size(abfd, section);
	if (data->pc >= vma + size)
		return;

    // Originally there was "pc-vma", but sometimes the bfd_find_nearest_line
    // returns the next line after the correct one. "pc-vma-1" seems to produce
    // correct line numbers:
	data->found = bfd_find_nearest_line(abfd, section, data->syms, data->pc - vma - 1,
				      &data->filename, &data->functionname, &data->line);
}

/* Loads the symbol table into the global variable 'syms'.  */

static void slurp_symtab(bfd * abfd, Data *data)
{
	long symcount;
	unsigned int size;

	if ((bfd_get_file_flags(abfd) & HAS_SYMS) == 0)
		return;

    void **tmp = (void **) &(data->syms);
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
    Data *data = new Data();
    // Read the symbols
	slurp_symtab(abfd, data);
    data->pc = addr[0];
    data->found = false;
    bfd_map_over_sections(abfd, find_address_in_section, data);
    if (!data->found) {
        s = format("[0x%llx] \?\?() \?\?:0", (long long unsigned int) addr[0]);
    } else {
        std::string name=demangle_function_name(data->functionname);
        s = format("  File \"%s\", line %u, in %s",
                data->filename ? data->filename : "??", data->line,
                name.c_str());
        if (data->filename) {
            std::string line_text=read_line_from_file(data->filename,
                    data->line);
            if (line_text != "") {
                s += "\n    ";
                s += line_text;
            }
        }
    }
    // cleanup
	if (data->syms != NULL) {
		free(data->syms);
		data->syms = NULL;
	}
    delete data;
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
			final += process_file(match.file, &addr);
		else
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
void show_backtrace (void)
{
    std::string s=get_backtrace();
    printf("%s", s.c_str());
}

void _segfault_callback_print_stack(int sig_num)
{
    printf("\nSegfault caught. Printing stacktrace:\n\n");
    show_backtrace();
    printf("\nDone. Exiting the program.\n");
    // Deregister our abort callback:
    signal(SIGABRT, SIG_DFL);
    abort();
}

void _abort_callback_print_stack(int sig_num)
{
    printf("\nAbort caught. Printing stacktrace:\n\n");
    show_backtrace();
    printf("\nDone.\n");
}

void print_stack_on_segfault()
{
    signal(SIGSEGV, _segfault_callback_print_stack);
    signal(SIGABRT, _abort_callback_print_stack);
}

#include "vxworks.h"

int vx653_loader(const char *filename,
                 uint64_t    *elf_entry_out,
                 uint64_t    *elf_lowaddr_out)
{
    target_long  kernel_size = 0;
    uint64_t     elf_entry;
    uint64_t     elf_lowaddr;
    char        *dirname     = g_path_get_dirname(filename);
    GIOChannel  *channel     = g_io_channel_new_file(filename, "r", NULL);
    char        *line        = NULL;
    char        *path        = NULL;
    gsize        term;

    if (channel != NULL) {

        while (g_io_channel_read_line(channel, &line, NULL, &term, NULL)
               && line != NULL) {

            if (term != 0) {
                line[term] = '\0';
            }

            if (g_pattern_match_simple("pwd://*", line)) {

                path = g_build_filename(dirname, line + 6, NULL);

                printf("load: '%s'\n", path);
                kernel_size = load_elf(path, NULL, NULL, &elf_entry,
                                       &elf_lowaddr, NULL, 1, ELF_MACHINE, 0);
                if (kernel_size < 0) {
                    fprintf(stderr, "qemu: could not load kernel '%s'\n",
                            line + 6);
                    exit(1);
                }

                if (elf_entry != (uint64_t)-1) {
                    *elf_entry_out   = elf_entry;
                    *elf_lowaddr_out = elf_lowaddr;
                }

                g_free(path);
            }
            g_free(line);
        }
        g_io_channel_shutdown(channel, false, NULL);
        g_io_channel_unref(channel);
    } else {
        perror(filename);
    }

    g_free(dirname);
    return 0;
}

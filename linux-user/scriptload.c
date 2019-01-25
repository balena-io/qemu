/* Script loader.  Based on linux/fs/binfmt_script.c */

#include "qemu/osdep.h"

#include "qemu.h"

struct linux_binprm;

int load_script_file(const char *filename, struct linux_binprm *bprm)
{
    int retval, fd;
    char *i_arg = NULL, *i_name = NULL;
    char **new_argp;
    char *cp;
    char buf[BPRM_BUF_SIZE];

    /* Check if it is a script */
    fd = open(filename, O_RDONLY);
    if (fd == -1) {
        return -ENOEXEC;
    }

    retval = read(fd, buf, BPRM_BUF_SIZE);
    if (retval == -1) {
        close(fd);
        return -ENOEXEC;
    }

     /* if we have less than 2 bytes, we can guess it is not executable */
        if (retval < 2) {
            close(fd);
            return -ENOEXEC;
        }

    close(fd);
    /* adapted from the kernel
     * https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/fs/binfmt_script.c
     */
    if ((buf[0] == '#') && (buf[1] == '!')) {
        buf[BPRM_BUF_SIZE - 1] = '\0';
        cp = strchr(buf, '\n');
        if (cp == NULL) {
            cp = buf + BPRM_BUF_SIZE - 1;
        }
        *cp = '\0';
        while (cp > buf) {
            cp--;
            if ((*cp == ' ') || (*cp == '\t')) {
                *cp = '\0';
            } else {
                break;
            }
        }
        for (cp = buf + 2; (*cp == ' ') || (*cp == '\t'); cp++) {
            /* nothing */ ;
        }
        if (*cp == '\0') {
            return -ENOEXEC; /* No interpreter name found */
        }
        i_name = cp;
        i_arg = NULL;
        for ( ; *cp && (*cp != ' ') && (*cp != '\t'); cp++) {
            /* nothing */ ;
        }
        while ((*cp == ' ') || (*cp == '\t')) {
            *cp++ = '\0';
        }

        new_argp = NULL;
        if (*cp) {
            i_arg = cp;
        }

        if (i_arg) {
            new_argp = alloca(sizeof(void *));
            new_argp[0] = i_arg;
        }
        bprm->argv = new_argp;
        bprm->filename = i_name;
    } else {
        return 1;
    }
    return 0;
}

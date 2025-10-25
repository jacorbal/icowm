/**
 * @file utils/path.c
 *
 * @brief Implementation for path handling functions
 */

/* Utils includes */
#include <utils/safestr.h>

/* Local includes */
#include <utils/path.h>


/* Normalize a given file path by removing unnecessary components */
void path_simplify(char *restrict path)
{
    char *src = path, *dst = path;
    char *last_slash = NULL;

    while (*src) {
        if (*src == '/') {
            /* Avoid multiple slashes */
            if (dst != path && *(dst - 1) == '/') {
                src++;
                continue;
            }
            *dst++ = *src++;
            last_slash = dst - 1;   /* Save last slash position */
        } else if (safe_strncmp(src, "./", 2) == 0) {
            src += 2;   /* Jump over "./" */
        } else if (safe_strncmp(src, "../", 3) == 0 && last_slash) {
            // Retrocede al último nivel
            dst = last_slash;   /* Go back to last slash */
            src += 3;   /* Jump over "../" */
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';    /* End string */
}

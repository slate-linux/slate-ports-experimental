#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define VALUE_SIZE 4096
#define LINE_SIZE 8192
#define PATH_SIZE 4096

struct slateport {
    char category[VALUE_SIZE];
    char portname[VALUE_SIZE];
    char version[VALUE_SIZE];
    char url[VALUE_SIZE];
    char depends[VALUE_SIZE];
    char make[VALUE_SIZE];
    char makeopts[VALUE_SIZE];
    char pre[VALUE_SIZE];
};

/* Trim leading and trailing whitespace. */
static char *trim(char *s)
{
    char *end;

    while (isspace((unsigned char)*s))
        s++;

    end = s + strlen(s);

    while (end > s && isspace((unsigned char)end[-1]))
        end--;

    *end = '\0';

    return s;
}

/* Safely copy a configuration value. */
static int set_value(char *dest, size_t size, const char *src)
{
    if (strlen(src) >= size)
        return -1;

    strcpy(dest, src);
    return 0;
}

/* Validate category and package names. */
static int valid_name(const char *name)
{
    const unsigned char *p;

    if (!name[0] ||
        strcmp(name, ".") == 0 ||
        strcmp(name, "..") == 0)
        return 0;

    for (p = (const unsigned char *)name; *p; p++) {
        if (!isalnum(*p) &&
            *p != '-' &&
            *p != '_' &&
            *p != '+')
            return 0;
    }

    return 1;
}

/* Create a directory if it does not exist. */
static int make_directory(const char *path)
{
    struct stat st;

    if (mkdir(path, 0755) == 0)
        return 0;

    if (errno == EEXIST &&
        stat(path, &st) == 0 &&
        S_ISDIR(st.st_mode))
        return 0;

    perror(path);
    return -1;
}

/* Write a shell-safe single-quoted string. */
static void shell_quote(FILE *fp, const char *value)
{
    const char *p;

    fputc('\'', fp);

    for (p = value; *p; p++) {
        if (*p == '\'')
            fputs("'\\''", fp);
        else
            fputc(*p, fp);
    }

    fputc('\'', fp);
}

/* Parse one configuration assignment. */
static int parse_assignment(
    struct slateport *port,
    char *line)
{
    char *key;
    char *value;
    char *eq;
    size_t len;

    key = trim(line);

    if (!*key || *key == '#')
        return 0;

    eq = strchr(key, '=');

    if (!eq)
        return 0;

    *eq = '\0';

    key = trim(key);
    value = trim(eq + 1);

    /* Remove surrounding quotes. */
    len = strlen(value);

    if (len >= 2 &&
        (*value == '"' || *value == '\'') &&
        value[len - 1] == *value) {
        value[len - 1] = '\0';
        value++;
    }

    if (strcmp(key, "CATEGORY") == 0)
        return set_value(port->category,
            sizeof(port->category), value);

    if (strcmp(key, "PORTNAME") == 0)
        return set_value(port->portname,
            sizeof(port->portname), value);

    if (strcmp(key, "VERSION") == 0)
        return set_value(port->version,
            sizeof(port->version), value);

    if (strcmp(key, "URL") == 0)
        return set_value(port->url,
            sizeof(port->url), value);

    if (strcmp(key, "DEPENDS") == 0)
        return set_value(port->depends,
            sizeof(port->depends), value);

    if (strcmp(key, "MAKE") == 0)
        return set_value(port->make,
            sizeof(port->make), value);

    if (strcmp(key, "MAKEOPTS") == 0)
        return set_value(port->makeopts,
            sizeof(port->makeopts), value);

    if (strcmp(key, "PRE") == 0)
        return set_value(port->pre,
            sizeof(port->pre), value);

    return 0;
}

/* Generate meta.conf. */
static int generate_meta(
    const char *path,
    const struct slateport *port)
{
    FILE *fp;

    fp = fopen(path, "w");

    if (!fp) {
        perror(path);
        return -1;
    }

    fprintf(fp, "VERSION=%s\n", port->version);

    if (port->depends[0]) {
        fprintf(fp, "DEPENDS=");
        shell_quote(fp, port->depends);
        fputc('\n', fp);
    }

    if (fclose(fp) != 0) {
        perror(path);
        return -1;
    }

    return 0;
}

/* Generate port.sh. */
static int generate_port(
    const char *path,
    const struct slateport *port)
{
    FILE *fp;

    fp = fopen(path, "w");

    if (!fp) {
        perror(path);
        return -1;
    }

    fprintf(fp,
        "#!/bin/sh\n"
        "set -eu\n"
        "\n"
        "cd \"$PORTS_DIR/%s/%s\"\n"
        "\n",
        port->category,
        port->portname
    );

    fprintf(fp, "URL=");
    shell_quote(fp, port->url);

    fprintf(fp, "\nMAKEOPTS=");
    shell_quote(fp, port->makeopts);

    fprintf(fp,
        "\n\n"
        "# Download source\n"
        "curl -fL -o source.tar.gz \"$URL\"\n"
        "tar -xf source.tar.gz\n"
        "\n"
        "# Find extracted source directory\n"
        "SRCDIR=$(tar -tf source.tar.gz | sed -n '1{s,/.*,,;p;}')\n"
        "\n"
        "if [ -z \"$SRCDIR\" ]; then\n"
        "    echo \"Error: cannot determine source directory\" >&2\n"
        "    exit 1\n"
        "fi\n"
        "\n"
        "cd \"$SRCDIR\"\n"
        "\n"
    );

    if (port->pre[0]) {
        fprintf(fp,
            "# Prerequisites\n"
            "%s\n\n",
            port->pre
        );
    }

    fprintf(fp,
        "# Split MAKEOPTS into arguments\n"
        "set -f\n"
        "set -- $MAKEOPTS\n"
        "\n"
    );

    if (strcmp(port->make, "python") == 0) {
        fprintf(fp,
            "# Python setup.py install\n"
            "python3 setup.py install --root=\"$DESTDIR\" --prefix=/usr \"$@\"\n"
        );
    }
    else if (strcmp(port->make, "cmake") == 0) {
        fprintf(fp,
            "# CMake build\n"
            "cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr \"$@\"\n"
            "cmake --build build --parallel \"$MAKETHREADS\"\n"
            "DESTDIR=\"$DESTDIR\" cmake --install build\n"
        );
    }
    else if (strcmp(port->make, "meson") == 0) {
        fprintf(fp,
            "# Meson build\n"
            "meson setup build --prefix=/usr \"$@\"\n"
            "meson compile -C build -j \"$MAKETHREADS\"\n"
            "DESTDIR=\"$DESTDIR\" meson install -C build\n"
        );
    }
    else if (strcmp(port->make, "gnumake") == 0) {
        fprintf(fp,
            "# GNU Make build\n"
            "gmake -j\"$MAKETHREADS\" \"$@\"\n"
            "gmake install DESTDIR=\"$DESTDIR\"\n"
        );
    }
    else if (strcmp(port->make, "make") == 0) {
        fprintf(fp,
            "# Make build\n"
            "make -j\"$MAKETHREADS\" \"$@\"\n"
            "make install DESTDIR=\"$DESTDIR\"\n"
        );
    }
    else {
        fprintf(stderr,
            "Error: unsupported MAKE backend (use python, meson, gnumake, cmake, or make): %s\n",
            port->make);

        fclose(fp);
        return -1;
    }

    if (fclose(fp) != 0) {
        perror(path);
        return -1;
    }

    if (chmod(path, 0755) != 0) {
        perror(path);
        return -1;
    }

    return 0;
}

/* Convert one package. */
static int convert_package(
    const char *ports_dir,
    const struct slateport *port)
{
    char category_dir[PATH_SIZE];
    char package_dir[PATH_SIZE];
    char meta_path[PATH_SIZE];
    char script_path[PATH_SIZE];

    if (!port->category[0] ||
        !port->portname[0] ||
        !port->version[0] ||
        !port->url[0] ||
        !port->make[0]) {
        fprintf(stderr,
            "Error: CATEGORY, PORTNAME, VERSION, URL and MAKE are required\n");
        return -1;
    }

    if (!valid_name(port->category) ||
        !valid_name(port->portname)) {
        fprintf(stderr,
            "Error: invalid CATEGORY or PORTNAME\n");
        return -1;
    }

    if (snprintf(category_dir, sizeof(category_dir),
        "%s/%s", ports_dir, port->category) >=
        (int)sizeof(category_dir)) {
        fprintf(stderr, "Error: category path too long\n");
        return -1;
    }

    if (snprintf(package_dir, sizeof(package_dir),
        "%s/%s", category_dir, port->portname) >=
        (int)sizeof(package_dir)) {
        fprintf(stderr, "Error: package path too long\n");
        return -1;
    }

    if (snprintf(meta_path, sizeof(meta_path),
        "%s/meta.conf", package_dir) >=
        (int)sizeof(meta_path)) {
        fprintf(stderr, "Error: metadata path too long\n");
        return -1;
    }

    if (snprintf(script_path, sizeof(script_path),
        "%s/port.sh", package_dir) >=
        (int)sizeof(script_path)) {
        fprintf(stderr, "Error: script path too long\n");
        return -1;
    }

    if (make_directory(category_dir) != 0)
        return -1;

    if (make_directory(package_dir) != 0)
        return -1;

    if (generate_meta(meta_path, port) != 0)
        return -1;

    if (generate_port(script_path, port) != 0)
        return -1;

    printf("Generated: %s/%s\n",
        port->category,
        port->portname);

    return 0;
}

/* Convert all package sections in a file. */
static int convert(
    const char *filename,
    const char *ports_dir)
{
    FILE *fp;
    struct slateport port;
    char line[LINE_SIZE];
    char *current;
    int active = 0;
    int count = 0;
    int result;

    fp = fopen(filename, "r");

    if (!fp) {
        perror(filename);
        return -1;
    }

    memset(&port, 0, sizeof(port));

    while (fgets(line, sizeof(line), fp)) {
        current = trim(line);

        /* Start of a new package. */
        if (strcmp(current, "--(PACKAGE)--") == 0) {

            /* Finish the previous package. */
            if (active) {
                result = convert_package(ports_dir, &port);

                if (result != 0) {
                    fclose(fp);
                    return -1;
                }

                count++;
            }

            memset(&port, 0, sizeof(port));
            active = 1;

            continue;
        }

        /* Ignore anything before the first package marker. */
        if (!active)
            continue;

        result = parse_assignment(&port, current);

        if (result != 0) {
            fprintf(stderr,
                "Error: configuration value too long\n");

            fclose(fp);
            return -1;
        }
    }

    if (ferror(fp)) {
        perror("Reading package file");
        fclose(fp);
        return -1;
    }

    /* Finish the final package. */
    if (active) {
        result = convert_package(ports_dir, &port);

        if (result != 0) {
            fclose(fp);
            return -1;
        }

        count++;
    }

    fclose(fp);

    if (count == 0) {
        fprintf(stderr,
            "Error: no packages found\n");
        return -1;
    }

    printf("Converted %d package(s).\n", count);

    return 0;
}

/* Main entry point. */
int main(int argc, char **argv)
{
    const char *ports_dir;

    if (argc < 2 || argc > 3) {
        fprintf(stderr,
            "Usage: %s <file.slateport> [ports-directory]\n",
            argv[0]);

        return 1;
    }

    ports_dir = argc == 3
        ? argv[2]
        : getenv("PORTS_DIR");

    if (!ports_dir || !ports_dir[0])
        ports_dir = "/usr/ports";

    if (convert(argv[1], ports_dir) != 0)
        return 1;

    return 0;
}

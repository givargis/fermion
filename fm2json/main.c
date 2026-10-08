/**
 * Copyright (c) 2025-2026 Tony Givargis
 * University of California, Irvine
 * main.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../libfm/fm.h"

static char *
read_file(const char *pathname, const char **error)
{
	char *program;
	FILE *file;
	long n;

	(*error) = "unable to read";
	if (!(file = fopen(pathname, "rb"))) {
		return NULL;
	}
	if (fseek(file, 0, SEEK_END) ||
	    (0 > (n = ftell(file))) ||
	    fseek(file, 0, SEEK_SET)) {
		fclose(file);
		return NULL;
	}
	if (!(program = malloc(n + 1))) {
		fclose(file);
		return NULL;
	}
	if ((size_t)n != fread(program, 1, (size_t)n, file)) {
		fclose(file);
		free(program);
		return NULL;
	}
	fclose(file);
	if (memchr(program, '\0', (size_t)n)) {
		(*error) = "NUL byte in";
		free(program);
		return NULL;
	}
	program[n] = '\0';
	return program;
}

int
main(int argc, char *argv[])
{
	char *output, *program, errstr[FM_ERRSTR_LEN];
	const char *error;
	fm_t fm;

	if (2 != argc) {
		fprintf(stderr, "error: missing input file argument\n");
		return EXIT_FAILURE;
	}
	if (!(program = read_file(argv[1], &error))) {
		fprintf(stderr, "error: %s '%s'\n", error, argv[1]);
		return EXIT_FAILURE;
	}
	if (!(output = malloc(strlen(argv[1]) + 6))) {
		fprintf(stderr, "error: out of memory");
		free(program);
		return EXIT_FAILURE;
	}
	snprintf(output, strlen(argv[1]) + 6, "%s.json", argv[1]);
	if (!(fm = fm_open(program, errstr))) {
		fprintf(stderr, "error: %s:%s\n", argv[1], errstr);
		free(output);
		free(program);
		return EXIT_FAILURE;
	}
	free(program);
	if (fm_json(fm, output)) {
		fprintf(stderr, "error: unable to write '%s'\n", output);
		free(output);
		fm_close(fm);
		return EXIT_FAILURE;
	}
	free(output);
	fm_close(fm);
	return 0;
}

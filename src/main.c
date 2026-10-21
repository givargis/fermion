#include "fm_lexer.h"
#include "fm_parser.h"

int
main()
{
	const char *s = "1+2-3*4/5";
	fm__parser_t parser;

	fm__lexer_init();
	parser = fm__parser_open(s);
	fm__parser_close(parser);
	return 0;
}

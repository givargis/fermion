#include <stdio.h>
#include <assert.h>

#include "fm_lexer.h"

int
main()
{
	const struct fm__lexer_token *token;
	size_t lineno, column;
	const char *errstr;
	fm__lexer_t lexer;

	fm__lexer_init();
	lexer = fm__lexer_open("1..+2");
	assert( lexer );

	do {
		if (!(token = fm__lexer_next(lexer))) {
			errstr = fm__lexer_error(lexer, &lineno, &column);
			printf("error: %lu:%lu: %s\n",
			       (unsigned long)lineno,
			       (unsigned long)column,
			       errstr);
			break;
		}
		printf("op=%d u.s=[%s]\n",
		       token->op,
		       token->op == FM__LEXER_OP_IDENTIFIER ? token->u.s : "");
	}
	while (FM__LEXER_OP_EOF != token->op);

	fm__lexer_close(lexer);
	return 0;
}

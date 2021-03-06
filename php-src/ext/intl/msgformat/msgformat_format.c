/*
   +----------------------------------------------------------------------+
   | PHP Version 5                                                        |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Stanislav Malyshev <stas@zend.com>                          |
   +----------------------------------------------------------------------+
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unicode/ustring.h>

#include "php_intl.h"
#include "msgformat_class.h"
#include "msgformat_format.h"
#include "msgformat_data.h"
#include "msgformat_helpers.h"
#include "intl_convert.h"

#ifndef Z_ADDREF_P
#define Z_ADDREF_P(z) ((z)->refcount++)
#endif

/* {{{ */
static void msgfmt_do_format(MessageFormatter_object *mfo, zval *args, zval *return_value TSRMLS_DC) 
{
	zval **fargs;
	int count;
	UChar* formatted = NULL;
	int formatted_len = 0;
	HashPosition pos;
	int i;

	count = zend_hash_num_elements(Z_ARRVAL_P(args));

	if(count < umsg_format_arg_count(MSG_FORMAT_OBJECT(mfo))) {
		/* Not enough aguments for format! */
		intl_error_set( INTL_DATA_ERROR_P(mfo), U_ILLEGAL_ARGUMENT_ERROR,
			"msgfmt_format: not enough parameters", 0 TSRMLS_CC );
		RETVAL_FALSE;
		return;
	}

	fargs = safe_emalloc(count, sizeof(zval *), 0);

	zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(args), &pos);
	for(i=0;i<count;i++) {
		zval **val;
		zend_hash_get_current_data_ex(Z_ARRVAL_P(args), (void **)&val, &pos);
		fargs[i] = *val;
		Z_ADDREF_P(fargs[i]);
		/* TODO: needs refcount increase here? */
		zend_hash_move_forward_ex(Z_ARRVAL_P(args), &pos);
	}

	umsg_format_helper(MSG_FORMAT_OBJECT(mfo), count, fargs, &formatted, &formatted_len, &INTL_DATA_ERROR_CODE(mfo) TSRMLS_CC);

	for(i=0;i<count;i++) {
		zval_ptr_dtor(&fargs[i]);
	}

	efree(fargs);

	if (formatted && U_FAILURE( INTL_DATA_ERROR_CODE(mfo) ) ) {
			efree(formatted);
	}

	INTL_METHOD_CHECK_STATUS( mfo, "Number formatting failed" );
	INTL_METHOD_RETVAL_UTF8( mfo, formatted, formatted_len, 1 );
}
/* }}} */

/* {{{ proto mixed MessageFormatter::format( array $args )
 * Format a message. }}} */
/* {{{ proto mixed msgfmt_format( MessageFormatter $nf, array $args )
 * Format a message.
 */
PHP_FUNCTION( msgfmt_format )
{
	zval *args;
	MSG_FORMAT_METHOD_INIT_VARS;


	/* Parse parameters. */
	if( zend_parse_method_parameters( ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Oa",
		&object, MessageFormatter_ce_ptr,  &args ) == FAILURE )
	{
		intl_error_set( NULL, U_ILLEGAL_ARGUMENT_ERROR,
			"msgfmt_format: unable to parse input params", 0 TSRMLS_CC );

		RETURN_FALSE;
	}

	/* Fetch the object. */
	MSG_FORMAT_METHOD_FETCH_OBJECT;

	msgfmt_do_format(mfo, args, return_value TSRMLS_CC);
}
/* }}} */

/* {{{ proto mixed MessageFormatter::formatMessage( string $locale, string $pattern, array $args )
 * Format a message. }}} */
/* {{{ proto mixed msgfmt_format_message( string $locale, string $pattern, array $args )
 * Format a message.
 */
PHP_FUNCTION( msgfmt_format_message )
{
	zval       *args;
	UChar      *spattern = NULL;
	int         spattern_len = 0;
	char       *pattern = NULL;
	int         pattern_len = 0;
	char       *slocale = NULL;
	int         slocale_len = 0;
	MessageFormatter_object mf = {0};
	MessageFormatter_object *mfo = &mf;

	/* Parse parameters. */
	if( zend_parse_method_parameters( ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "ssa",
		  &slocale, &slocale_len, &pattern, &pattern_len, &args ) == FAILURE )
	{
		intl_error_set( NULL, U_ILLEGAL_ARGUMENT_ERROR,
			"msgfmt_format_message: unable to parse input params", 0 TSRMLS_CC );

		RETURN_FALSE;
	}

	msgformat_data_init(&mfo->mf_data TSRMLS_CC);

	if(pattern && pattern_len) {
		intl_convert_utf8_to_utf16(&spattern, &spattern_len, pattern, pattern_len, &INTL_DATA_ERROR_CODE(mfo));
		if( U_FAILURE(INTL_DATA_ERROR_CODE((mfo))) )
		{
			intl_error_set( NULL, U_ILLEGAL_ARGUMENT_ERROR,
				"msgfmt_format_message: error converting pattern to UTF-16", 0 TSRMLS_CC );
			RETURN_FALSE;
		}
	} else {
		spattern_len = 0;
		spattern = NULL;
	}

	if(slocale_len == 0) {
		slocale = INTL_G(default_locale);
	}

#ifdef MSG_FORMAT_QUOTE_APOS
	if(msgformat_fix_quotes(&spattern, &spattern_len, &INTL_DATA_ERROR_CODE(mfo)) != SUCCESS) {
		intl_error_set( NULL, U_INVALID_FORMAT_ERROR,
			"msgfmt_format_message: error converting pattern to quote-friendly format", 0 TSRMLS_CC );
		RETURN_FALSE;
	}
#endif

	/* Create an ICU message formatter. */
	MSG_FORMAT_OBJECT(mfo) = umsg_open(spattern, spattern_len, slocale, NULL, &INTL_DATA_ERROR_CODE(mfo));
	if(spattern && spattern_len) {
		efree(spattern);
	}
	INTL_METHOD_CHECK_STATUS(mfo, "Creating message formatter failed");

	msgfmt_do_format(mfo, args, return_value TSRMLS_CC);

	/* drop the temporary formatter */
	msgformat_data_free(&mfo->mf_data TSRMLS_CC);
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

#include <brutal/alloc.h>
#include <bs/bs.h>

static BsExpr parse_expression(Scan *scan, Alloc *alloc);

static bool skip_keyword(Scan *scan, const char *expected)
{
    return scan_skip_word(scan, str_cast(expected));
}

static bool skip_space(Scan *scan)
{
    return scan_eat(scan, isspace);
}

static void skip_nested_comment(Scan *scan)
{
    while (!skip_keyword(scan, "|#") && !scan_end(scan))
    {
        if (skip_keyword(scan, "#|"))
        {
            skip_nested_comment(scan);
        }
    }
}

static bool skip_comment(Scan *scan)
{
    if (scan_skip(scan, ';'))
    {
        while (scan_next(scan) != '\n' && !scan_end(scan))
        {
            if (skip_keyword(scan, "#|"))
            {
                skip_nested_comment(scan);
            }
        }

        return true;
    }
    else if (skip_keyword(scan, "#|"))
    {
        skip_nested_comment(scan);
        return true;
    }
    else if (skip_keyword(scan, "#|"))
    {
        skip_space(scan);

        struct alloc_heap heap;
        alloc_heap_init(&heap);
        parse_expression(scan, base_cast(&heap));
        alloc_heap_deinit(&heap);

        return true;
    }
    else
    {
        return false;
    }
}

static void skip_atmosphere(Scan *scan)
{
    while (skip_space(scan) || skip_comment(scan))
        ;
}

static int isany(int c, char const *what)
{
    for (size_t i = 0; what[i]; i++)
    {
        if (what[i] == c)
        {
            return true;
        }
    }

    return false;
}

static int isatom_start(int c)
{
    return isalnum(c) | isany(c, "!$%&*/:<=>?^_~");
}

static int isatom(int c)
{
    return isatom_start(c) || isdigit(c) || isany(c, "+-.@");
}

static BsExpr parse_atom(Scan *scan)
{
    scan_begin_token(scan);
    scan_next(scan);
    scan_skip_until(scan, isatom);
    return bs_atom(scan_end_token(scan));
}

static BsExpr parse_rune(Scan *scan)
{
    if (skip_keyword(scan, "alarm"))
    {
        return bs_rune('\a');
    }
    else if (skip_keyword(scan, "backspace"))
    {
        return bs_rune('\xb');
    }
    else if (skip_keyword(scan, "delete"))
    {
        return bs_rune('\xff');
    }
    else if (skip_keyword(scan, "escape"))
    {
        return bs_rune('\e');
    }
    else if (skip_keyword(scan, "newline"))
    {
        return bs_rune('\n');
    }
    else if (skip_keyword(scan, "null"))
    {
        return bs_rune('\0');
    }
    else if (skip_keyword(scan, "return"))
    {
        return bs_rune('\r');
    }
    else if (skip_keyword(scan, "space"))
    {
        return bs_rune(' ');
    }
    else if (skip_keyword(scan, "tab"))
    {
        return bs_rune('\t');
    }

    if (scan_curr(scan) == 'x' && isxdigit(scan_peek(scan, 1)))
    {
        Rune rune = 0;

        while (isxdigit(scan_curr(scan)) && !scan_end(scan))
        {
            rune *= 16;
            rune += scan_next_digit(scan);
        }

        return bs_rune(rune);
    }

    return bs_rune(scan_next(scan));
}

static BsExpr parse_pair(Scan *scan, Alloc *alloc)
{
    if (scan_skip(scan, ')'))
    {
        return bs_nil();
    }

    BsExpr car = parse_expression(scan, alloc);
    skip_atmosphere(scan);
    BsExpr cdr = parse_pair(scan, alloc);

    return bs_pair(alloc, car, cdr);
}

static BsExpr parse_expression(Scan *scan, Alloc *alloc)
{
    if (skip_keyword(scan, "#true") || skip_keyword(scan, "#t"))
    {
        return bs_bool(true);
    }
    else if (skip_keyword(scan, "#false") || skip_keyword(scan, "#f"))
    {
        return bs_bool(false);
    }
    else if (skip_keyword(scan, "#\\"))
    {
        return parse_rune(scan);
    }
    else if (scan_skip(scan, '('))
    {
        skip_atmosphere(scan);
        return parse_pair(scan, alloc);
    }
    else if (isatom_start(scan_curr(scan)))
    {
        return parse_atom(scan);
    }
    else
    {
        char c = scan_curr(scan);
        scan_throw(scan, str_cast("unexpected-character"), str_cast_n(1, &c));
        return bs_nil();
    }
}

BsExpr bs_parse(Scan *scan, Alloc *alloc)
{
    skip_atmosphere(scan);
    return parse_expression(scan, alloc);
}

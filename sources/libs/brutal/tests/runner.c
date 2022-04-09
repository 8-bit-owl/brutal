#include <brutal/alloc.h>
#include <brutal/cli.h>
#include <brutal/debug.h>
#include <brutal/io.h>
#include <brutal/math.h>
#include <brutal/tests/runner.h>
#include <brutal/text.h>
#include <brutal/time.h>
#include <unistd.h>

static Vec(Test) _tests = {};
static bool _tests_init = false;
static TestCtx *_self = nullptr;

void test_register(Test test)
{
    if (!_tests_init)
    {
        vec_init(&_tests, alloc_global());
        _tests_init = true;
    }
    vec_push(&_tests, test);
}

TestCtx *test_self(void)
{
    assert_not_null(_self);
    return _self;
}

void test_init(TestCtx *self, Alloc *alloc)
{
    *self = (TestCtx){};
    vec_init(&self->hooks, alloc);
    _self = self;
}

void test_deinit(TestCtx *self)
{
    vec_deinit(&self->hooks);
}

void test_begin_suite(TestCtx *self)
{
    self->seed = m_entropy64();
}

void test_end_suite(TestCtx *self)
{
    cli_clear_display_after_cursor(io_chan_err());

    if (self->counter_passed == self->counter_total)
    {
        cli_fmt$(io_chan_err(), "\n 🤘 all {} tests passed - [fg-light-black]{}\n\n", self->counter_passed, txt_nice(tick_now()));
    }
    else
    {
        cli_fmt$(io_chan_err(), "\n ❌ [fg-red]{} fail [fg-green]{} pass {} total - [fg-light-black]{}\n\n", self->counter_failed, self->counter_passed, self->counter_total, txt_witty(tick_now()));
    }
}

void test_passed(TestCtx *self, Test *test)
{
    UNUSED(self);
    cli_fmt$(io_chan_err(), "[fg-white bg-green bold]{} [fg-white]{}\n", " PASS ", test->name);
}

void test_failed(TestCtx *self, Test *test)
{
    UNUSED(self);
    if (test->flags & TEST_EXPECTED_TO_FAIL)
    {
        test->state = TEST_PASS;
    }
    else if (test->state == TEST_PASS)
    {
        cli_fmt$(io_chan_err(), "[fg-white bg-red bold]{} [fg-red]{}\n", " FAIL ", test->name);
        test->state = TEST_FAIL;
    }
}

void test_run_test(TestCtx *self, Test *test)
{
    self->current_test = test;
    self->current_case = nullptr;

    self->counter_total++;

    test_progress(self);
    test->state = (test->flags & TEST_EXPECTED_TO_FAIL) ? TEST_FAIL : TEST_PASS;
    test->fn();

    if (test->state == TEST_PASS)
    {
        test_passed(self, test);
        self->counter_passed++;
    }
    else
    {
        self->counter_failed++;
    }

    vec_foreach(r, &self->hooks)
    {
        r->dtor(r->data);
    }

    vec_clear(&self->hooks);
}

void test_run_all(TestCtx *self)
{
    cli_fmt$(io_chan_err(), "Running {} tests...\n", vec_len(&_tests));

    vec_foreach(t, &_tests)
    {
        test_run_test(self, t);
    }
}

void test_run_sel(TestCtx *self, Str pattern)
{
    vec_foreach(t, &_tests)
    {
        if (glob_match_str(pattern, t->name))
        {
            test_run_test(self, t);
        }
    }
}

bool test_begin_case(TestCtx *self, TestCase *test_case)
{
    self->current_case = test_case;
    test_progress(self);
    return true;
}

void test_end_case(TestCtx *self)
{
    self->current_case = nullptr;
}

void test_progress(TestCtx *self)
{
    cli_clear_display_after_cursor(io_chan_err());
    cli_fmt$(io_chan_err(), "\n[fg-cyan]{} ", cli_spinner(CLI_SPINNER_DOTS));

    if (self->current_test)
    {
        if (self->current_case)
        {
            cli_fmt$(io_chan_err(), "Running {} - {}...", self->current_test->name, self->current_case->name);
        }
        else
        {
            cli_fmt$(io_chan_err(), "Running {}...", self->current_test->name);
        }
    }
    else
    {
        cli_fmt$(io_chan_err(), "Ideling...");
    }

    cli_fmt$(io_chan_err(), "\n");
    cli_cursor_horizontal(io_chan_err(), 0);
    cli_cursor_prevline(io_chan_err(), 2);

    usleep(10000);
}

void test_fail(MAYBE_UNUSED TestCtx *self, Loc loc, Str msg, AnyVa args)
{
    cli_fmt$(io_chan_err(), "{}:{}: ", loc.file, loc.line);
    cli_fmt(io_chan_err(), msg, args);
    io_write_byte$(io_chan_err(), '\n');
}

void test_expect(MAYBE_UNUSED TestCtx *self, Loc loc, Any lhs, Any rhs, char const *op)
{
    test_failed(self, self->current_test);
    cli_fmt$(io_chan_err(), "{}:{}: Expected {} {} {}\n", loc.file, loc.line, lhs, op, rhs);
}

void *test_use(TestCtx *self, uint64_t id, void *args, TestHookCtor *ctor, TestHookDtor *dtor)
{
    vec_foreach(r, &self->hooks)
    {
        if (r->id == id)
        {
            return r->data;
        }
    }

    TestHook hook = {
        .id = id,
        .dtor = dtor,
        .data = ctor(args),
    };

    vec_push(&self->hooks, hook);

    return hook.data;
}
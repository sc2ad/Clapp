tests = [
    'test_simple',
]
ex_fail = []
suites = {
    'test_simple': ['simple'],
}

foreach t : tests + ex_fail
    e = executable(t, [t + '.cpp'], dependencies: [gtest_dep, gtest_main_dep, clapp_dep])
    test(t, e, suite: suites[t], should_fail: t in ex_fail)
endforeach

tests = [
    'test_simple',
]
suites = {
    'test_simple': ['simple']
}

foreach t : tests
    e = executable(t, [t + '.cpp'], dependencies: [gtest_dep, gtest_main_dep, clapp_dep])
    test(t, e, suite: suites[t])
endforeach

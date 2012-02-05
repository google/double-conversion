double_conversion_sources = ['src/' + x for x in SConscript('src/SConscript')]
double_conversion_test_sources = ['test/cctest/' + x for x in SConscript('test/cctest/SConscript')]
test = double_conversion_sources + double_conversion_test_sources
print(test)
env = Environment(CPPPATH='#/src')
debug = ARGUMENTS.get('debug', 0)
if int(debug):
  env.Append(CCFLAGS = '-g')
print double_conversion_sources
print double_conversion_test_sources
env.Program('run_tests', double_conversion_sources + double_conversion_test_sources)

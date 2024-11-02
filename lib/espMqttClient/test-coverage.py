import os

Import("env", "projenv")

# Dump build environment (for debug purpose)
print(env.Dump())

# access to global build environment
print(env)

# access to the project build environment
# (used for source files located in the "src" folder)
print(projenv)

def generateCoverageInfo(source, target, env):
    for file in os.listdir("test"):
        os.system(".pio/build/native/program test/"+file)
    os.system("lcov -d .pio/build/native/ -c -o lcov.info")
    os.system("lcov --remove lcov.info '*Unity*' '*unity*' '/usr/include/*' '*/test/*' -o filtered_lcov.info")
    os.system("genhtml -o cov/ --demangle-cpp filtered_lcov.info")

env.AddPostAction(".pio/build/native/program", generateCoverageInfo)
./build.sh && gdb          \
    -ex "b RobinHT_Set"    \
    -ex "b RobinHT_Get"    \
    -ex "b RobinHT_Rem"    \
    -ex "run"              \
    a.out

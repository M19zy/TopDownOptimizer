# Generic Join Optimizer 运行说明

## Dependency

1. OS: CentOS 7 (Other operating system should be ok)
2. Compiler：clang-15 (Support C++20 at least)
3. Or-tools，download proper version for your os from（[https://developers.google.com/optimization/install/cpp/linux](https://developers.google.com/optimization/install/cpp/linux). Put Or-tools into the project folder, and input in terminal
   ```
   export  LD_LIBRARY_PATH=$LD_LIBRARY_PATH:or-tools/lib/libortools.so.9```
   ```

## Compile

Input in terminal

```
make
```

## Run

After compiled successfully, run 'main' like the format of `./main dataDir queryDir`. For example, you can run `./main test test.sql`。

## Others

Please contact the author if have any problem.

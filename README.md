# Generic Join Optimizer 运行说明

## 依赖环境

1. 代码应该和OS无关，本实验可在CentOS 7 环境下正常运行，但其它环境下并未测试过。
2. 编译器：clang-15。代码需要使用c++部分新特性，因此编译器应该支持c++20特性（编译器至少需要支持c++17特性，但不保证可以通过编译）。
3. 其它：Or-tools，谷歌开源的优化工具库。因为开发环境问题，请下载Or-tools适合的编译版本文件（[https://developers.google.com/optimization/install/cpp/linux](https://developers.google.com/optimization/install/cpp/linux)）放入项目文件夹GJP/下，并在目录`GJP/`下输入
   ```
   export  LD_LIBRARY_PATH=$LD_LIBRARY_PATH:or-tools/lib/libortools.so.9```
   ```

## 编译

在目录`GJP/`下输入

```
make
```

## 运行

编译成功后，得到`main`文件，按照格式`./main dataDir queryDir`执行即可。例如使用样例中的查询：`./main test test.sql`。

## 其它

有其它问题欢迎联系作者~

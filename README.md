# Compiler
Compiler principles lab

# 依赖关系
cmake, gcc, g++, flex, bison, spim
(若提示cmake版本低, 通常修改CMakeLists.txt文件第一行为你的版本即可)
```
sudo apt install cmake gcc g++ flex bison spim
```

# 编译
```
cd build
cmake ..
make
```
将产生 CompilerLab1, CompilerLab2, CompilerLab3, CompilerLab4.
CompilerLab1~3 是阶段性成果. CompilerLab4 包含了其他是三个的所有功能.

# 运行
```
./CompilerLab4 ../testcase/lab4_more/E-2.3.cmm output.s
spim -file output.s
```
第一条命令将输出 output.s, output.s.ir, output.s.syntax 文件. 
分别为目标代码, 中间代码和抽象语法树. 若发生错误则不会生成.
第二条命令运行生成的结果.

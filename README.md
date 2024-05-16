# 一、简介
Hoshino是一个基于llvm-14的解释性语言，目前类型不完善。frontend词法分析与语法分析基于c++17使用递归下降分析构建AST；表达式分析使用运算符优先级分析法。
# 二、安装
本语言基于llvm-14（理论上版本大于14均可），因此需要安装llvm-14，单独安装llvm-14可以到[llvm的github官方](https://github.com/llvm/llvm-project)查看相关步骤，build_support的脚本会直接安装clang-14（clang-14的安装自带llvm-14）：
```sh
# 进入build_support目录
cd ./build_support
# 更改执行权限
chmod +x ./install_support.sh
# 安装llvm依赖
./install_support.sh
```
llvm-14安装完成后即可构建该项目：
```sh
# 创建构建目录
mkdir build
cd ./build
# 构建项目
cmake ..
make -j4 # make使用CPU核数可自行设定
```
# 三、语法演示
## 3.1 函数定义
```txt
# 声明函数，以下声明为builtin库的C函数，jit会在运行时寻找外部链接调用此函数
extern printNum();
extern endl();
def printOneToTen(){
  for i=1;i<11;i+=1  {
    printNum(i);
    endl();
  }
}
# 打印1到10
printOneToTen();
```
## 3.2 自定义运算符
自定义运算符结构如下：
```txt
# operator为要定义的运算符
# 其中unary运算符只支持单字符，binary运算符支持单字符、双字符以及三字符
# precedence为运算符优先级
# args为参数，各参数间用空格隔开
def [unary|binary]@[operator] [precedence] (args) {
    ......
}
```
下面演示一些基本运算符的定义
```txt
# 定义单目运算符!
def unary@!(v) {
  if v {
    0;
  }
  else {
    1;
  }
}
# 定义双目运算符||
def binary@|| 5 (LHS RHS){
  if LHS {
    1;
  }
  else if RHS{
    1;
  }
  else{
    0;
  }
}
# 定义双目运算符>
def binary@> 10 (LHS RHS) {
  RHS < LHS;
}
# 定义双目运算符==
def binary@== 9 (LHS RHS) {
  !(LHS < RHS || LHS > RHS);
}
# 判断两个数是否相等
def isSame(x y) {
    x == y;
}
{
  var b = isSame(5, 5);
  printNum(b);
}
```
## 3.3 if语句
```txt
# if语句的条件接受一个表达式，为0时条件为假，非0时条件为真
{
  var a = 10;
  if a==10 {
    printNum(a);
  } else if a < 5 {
    printNum(1);
  } else {
    printNum(0);
  }
}
```
## 3.4 for循环
```txt
extern putchard(char);
extern printNum(char);
extern tab();
extern endl();
# 打印九九乘法表
for i=1;i<10;i+=1{
    for j=1;j<i;j+=1 {
      tab(); 
    }
    # print i*j=?
    # putchard接收ascii码打印字符
    # putchard(42)打印'*'
    # putchard(61)打印'='
    for j=i;j<10;j+=1 {
        printNum(i) , putchard(42) , printNum(j) , putchard(61) , 
        printNum(i*j) , 
        tab(); 
    }
    endl();
}
```
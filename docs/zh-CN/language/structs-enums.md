# 结构体与枚举



## Struct（结构体）



结构体是**值类型**，分配在栈上，赋值时按值复制（不同于类的引用语义）。



### 声明



```kinal

Struct Point
{

    int X;

    int Y;

}

```



结构体字段默认是公开的（与 `Class` 略有不同）。可以显式加访问控制，但结构体通常用于轻量数据容器。



### 使用



```kinal

Point p;

p.X = 3;

p.Y = 4;



int dist = p.X * p.X + p.Y * p.Y;

```



结构体不使用 `New` 关键字创建，直接声明变量即可（字段默认为零值）：



```kinal

Struct Rectangle
{

    int Width;

    int Height;

}



Rectangle r;

r.Width  = 100;

r.Height = 50;

int area = r.Width * r.Height;

```



### Packed 结构体



`By Packed` 属性使结构体字段紧密排列（无填充）：



```kinal

Struct Header By Packed
{

    u16 Magic;

    u8  Version;

    u8  Flags;

}

```



### 内存对齐



`By Align(n)` 指定字节对齐：



```kinal

Struct AlignedData By Align(8)
{

    int a;

    int b;

}

```



也可以同时使用多个属性：



```kinal

Struct Pair By Align(8)
{

    int a;

    int b;

}

```



### 结构体与 FFI



结构体常用于与 C 的 `struct` 互操作：



```kinal

Struct sockaddr_in By Packed
{

    u16 sin_family;

    u16 sin_port;

    u32 sin_addr;

    byte[8] sin_zero;

}

```



详见 [FFI](ffi.md)。



---



## Enum（枚举）



枚举定义一组命名的整数常量。



### 声明



```kinal

Enum Direction
{

    North,

    South,

    East,

    West

}

```



默认底层类型为 `int`，值从 0 开始自动递增。



### 显式指定值



```kinal

Enum HttpStatus
{

    Ok = 200,

    NotFound = 404,

    ServerError = 500

}

```



### 指定底层类型



使用 `By 类型` 指定底层整数类型：



```kinal

Enum Irq By u8
{

    Timer    = 0,

    Keyboard = 1,

    Serial   = 2

}



Enum BigFlag By u64
{

    None  = 0,

    Read  = 1,

    Write = 2,

    Exec  = 4

}

```



支持的底层类型：`i8`、`u8`、`i16`、`u16`、`i32`、`u32`、`i64`、`u64`、`int`、`byte`。



### 使用枚举



```kinal

Direction d = Direction.North;



Switch (d)
{

    Case (Direction.North) { IO.Console.PrintLine("北"); }

    Case (Direction.South) { IO.Console.PrintLine("南"); }

    Case (Direction.East)  { IO.Console.PrintLine("东"); }

    Case (Direction.West)  { IO.Console.PrintLine("西"); }

}

```



比较：



```kinal

If (d == Direction.North)
{
    IO.Console.PrintLine("朝北");

}

```



枚举转整数：



```kinal

int val = [int](HttpStatus.Ok);    // 200

u8  irq = [u8](Irq.Timer);        // 0

```



整数转枚举：



```kinal

HttpStatus status = [HttpStatus](404);

```



### 枚举成员作为静态常量



枚举成员可以通过类型名访问，也支持静态成员调用语法：



```kinal

// 以下两种方式等价

Direction d1 = Direction.North;

```



### 嵌套枚举（类内部）



```kinal

Class Network
{

    Enum Protocol
    {

        TCP,

        UDP,

        ICMP

    }

}



Network.Protocol p = Network.Protocol.TCP;

```



---



## 综合示例



```kinal

Unit App.HardwareDemo;



Get IO.Console;



// 中断号枚举

Enum IrqKind By u8
{

    Timer    = 0,

    Keyboard = 1,

    Network  = 2

}



// 中断描述符结构体（紧凑排列）

Struct IrqDescriptor By Packed
{

    u8  irq;

    u16 handler;

    u8  flags;

}



Static Function void HandleIrq(IrqKind kind)
{

    Switch (kind)
    {

        Case (IrqKind.Timer)
        {

            IO.Console.PrintLine("定时器中断");

        }

        Case (IrqKind.Keyboard)
        {

            IO.Console.PrintLine("键盘中断");

        }

        Case (default)
        {

            IO.Console.PrintLine("未知中断");

        }

    }

}



Static Function int Main()
{

    IrqDescriptor desc;

    desc.irq     = [u8](IrqKind.Keyboard);

    desc.handler = 0x1000;

    desc.flags   = 0x01;



    HandleIrq([IrqKind](desc.irq));

    Return 0;

}

```



## 下一步



- [模块系统](modules.md)

- [FFI 外部函数接口](ffi.md)

- [安全等级](safety.md)


# 异常处理



Kinal 通过 `Try`/`Catch`/`Throw` 实现异常处理，异常对象可以是任意类型。



## 基本语法



```kinal

Try
{

    // 可能抛出异常的代码

}
Catch (类型 变量名)
{

    // 处理异常

}

```



## Throw



`Throw` 抛出一个异常，值可以是任意类型：



```kinal

Throw "出错了";                    // 抛出字符串

Throw New IO.Error("标题", "详情"); // 抛出 IO.Error 对象

Throw 42;                          // 抛出整数（不推荐但合法）

```



## Try/Catch



捕获特定类型的异常：



```kinal

Try
{

    Throw "boom";

}
Catch (string e)
{

    IO.Console.PrintLine("捕获到字符串异常: " + e);

}

```



捕获 `IO.Error` 对象：



```kinal

Try
{

    Throw New IO.Error("文件错误", "文件不存在");

}
Catch (IO.Error err)
{

    IO.Console.PrintLine(err.Title);

    IO.Console.PrintLine(err.Message);

}

```



## IO.Error — 标准异常类



`IO.Error` 是 Kinal 内置的标准异常类，字段如下：



| 字段 | 类型 | 说明 |

|------|------|------|

| `Title` | `string` | 异常标题（简短描述） |

| `Message` | `string` | 详细信息 |

| `Inner` | `IO.Error` | 内嵌的根因异常（可为 `null`） |

| `Trace` | `string` | 调用栈轨迹（自动填充） |



```kinal

IO.Error err = New IO.Error("网络错误", "连接超时");

IO.Console.PrintLine(err.Title);    // 网络错误

IO.Console.PrintLine(err.Message);  // 连接超时

```



### 异常链



使用 `Inner` 字段链接根因：



```kinal

Try
{

    // 底层操作

    IO.Error inner = New IO.Error("Socket 错误", "连接被拒绝");

    Throw New IO.Error("HTTP 请求失败", "无法建立连接", inner);

}
Catch (IO.Error err)
{

    IO.Console.PrintLine(err.Title);

    IO.Console.PrintLine(err.Message);

    If (err.Inner != null)
    {
        IO.Console.PrintLine("根因: " + err.Inner.Title);

    }

}

```



完整构造函数：



```kinal

// IO.Error(title, message) — 两参数版本

IO.Error e1 = New IO.Error("标题", "消息");



// 通过字段设置 Inner

IO.Error e2 = New IO.Error("外层", "外层消息");

// 注意：Inner 字段需要在构造后单独设置（见下面的链式异常测试）

```



## 不带参数的 Catch



若不需要异常信息，可以省略 `Catch` 参数（捕获所有类型）：



```kinal

Try
{

    RiskyOperation();

} Catch
{

    IO.Console.PrintLine("发生了异常");

}

```



## Return via Throw



在需要从函数通过异常返回时：



```kinal

Function int SafeDivide(int a, int b)
{

    If (b == 0)
    {
        Throw New IO.Error("除法错误", "除数不能为零");

    }

    Return a / b;

}



Try
{

    int result = SafeDivide(10, 0);

}
Catch (IO.Error err)
{

    IO.Console.PrintLine(err.Title);  // 除法错误

}

```



## 完整示例



```kinal

Unit App.ExceptionDemo;



Get IO.Console;



Function int Divide(int a, int b)
{

    If (b == 0)
    {
        Throw New IO.Error("ArithmeticError", "Division by zero");

    }

    Return a / b;

}



Function void Risky()
{

    // 嵌套 Try

    Try
    {

        int r = Divide(10, 0);

        IO.Console.PrintLine(r);

    }
    Catch (IO.Error inner)
    {

        Throw New IO.Error("Operation Failed", "计算失败", inner);

    }

}



Static Function int Main()
{

    Try
    {

        Risky();

    }
    Catch (IO.Error err)
    {

        IO.Console.PrintLine("错误: " + err.Title);

        IO.Console.PrintLine("消息: " + err.Message);

        If (err.Inner != null)
        {
            IO.Console.PrintLine("根因: " + err.Inner.Title + " - " + err.Inner.Message);

        }

    }
    Catch (string msg)
    {

        IO.Console.PrintLine("字符串异常: " + msg);

    }

    Return 0;

}

```



## 注意事项



- `Catch` 只捕获类型**完全匹配**的异常，Kinal 目前不支持多态捕获（即 `Catch (IO.Error e)` 不捕获 `IO.Error` 的子类）

- 未被捕获的异常会将程序终止

- 在 `Safe` 函数中，异常处理同样可用



## 下一步



- [类与继承](classes.md)

- [安全等级](safety.md)


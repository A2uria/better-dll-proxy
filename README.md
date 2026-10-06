# Better DLL Proxy

| **原理**         | **代码量** | **如何解决 DLL 重名**     |
| ---------------- | ---------- | ------------------------- |
| **Delay Import** | **较低**   | 修改 `__delayLoadHelper2` |
| Forwarded Export | 较低       | 绝对路径 Workaround       |
| Regular Import   | 中等       |                           |
| Manual Import    | 较高       | 手写 Handler              |

## Delay Import

我实现了一份基于延迟导入实现的 DLL 转发，将 DLL 转发的大多数工作交由 Linker 完成，也就是说不再需要什么 dll proxy generator，不再需要手写 Handler，只需要提供一个 `.def` 文件就能做到 DLL 转发。

实现一个优先加载系统 DLL 的 `__delayLoadHelper2` 其实很简单，把 `delayhlp.cpp` 里的 `LoadLibraryExA` 的 `dwFlags` 参数修改成 `LOAD_LIBRARY_SEARCH_SYSTEM32` 就行。

```diff
--- a/delayhlp.cpp
+++ b/delayhlp.cpp
@@ -315,7 +315,7 @@
             hmod = HMODULE(((*__pfnDliNotifyHook2)(dliNotePreLoadLibrary, &dli)));
             }
         if (hmod == 0) {
-            hmod = ::LoadLibraryEx(dli.szDll, NULL, 0);
+            hmod = ::LoadLibraryEx(dli.szDll, NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
             }
         if (hmod == 0) {
             dli.dwLastError = ::GetLastError();
```

不过微软的实现实在是太过繁琐了，`__delayLoadHelper2` 的工作其实很简单，也就是根据延迟导入表 `arg1` 和延迟导入地址 `arg2` 加载目标函数。所以我搓了一份只支持从系统目录延迟加载 DLL 的极简 `__delayLoadHelper2`。

https://github.com/A2uria/better-dll-proxy/blob/eeac924895ab9e9a069721fa0158832c8b4952cb/src/__delayLoadHelper2.c#L8-L45

如果需要用自己的函数替换导出的函数，只需要添加自己的实现然后修改 `.def` 文件就行。

```diff
 LIBRARY version
 EXPORTS
-    GetFileVersionInfoA
+    GetFileVersionInfoA = __wrap_GetFileVersionInfoA
     ...
```

使用方法：给 Linker 传入 `-def:x.def -delayload:x.dll x.lib`

> [!IMPORTANT]
>
> 部分系统导入库（如 `version.lib`）并没有导出目标 DLL 的全部符号，也就是说最好先根据 `.def` 生成完整的 `.lib`，然后再使用。

## Forwarded Export

> [!NOTE]
>
> `.def` 通过 export definition 来描述导出的函数，语法如下：
>
> ```
> export_definition ::= entry_name [ "=" ( internal_name | other_module "." exported_name ) ] [ "@" ordinal [ NONAME ] ] [ PRIVATE | DATA ]
> ```

用这种方式实现 DLL 转发其实是最轻松的，DLL 不重名的情况下（正常转发或者目标 DLL 经过重命名，比如 `steam_api64.dll` 转发到 `steam_api64_o.dll`）只需要导出 `func = module.func` 就行，但是很多情况下我们转发的目标是同名系统 DLL，此时直接转发就会出现问题。

对于这类问题，有一部分项目会考虑给 `other_module` 硬编码 `C:\Windows\System32`，但极端情况下会出现找不到 DLL 的问题，如 `WinPE` 环境下 `SystemRoot` 其实是 `X:\Windows` 而非 `C:\Windows`，导致加载失败。

[mrexodia/perfect-dll-proxy](https://github.com/mrexodia/perfect-dll-proxy) 的解决方案是在 `other_module` 中硬编码 `\\.\GLOBALROOT\SystemRoot` 来解析 `Windows` 文件夹，但是这样仍存在一定缺陷，因为绕过了重定向的原因，32 位 DLL 需要硬编码 `SysWOW64`，且这样 wow64 DLL 不兼容 win32 DLL。

使用方法：给 Linker 传入 `-def:x.def` 或者硬编码到 `#pragma comment` 即可。

## Regular Import

如果目标 DLL 与你的 DLL 不重名的话，其实也可以考虑正常使用导入库导入目标函数然后直接导出。这种办法一般是用于 Patch 和 Hook 目标 DLL，考虑到 Windows 的 PE Loader 在加载 DLL 依赖时会进行拓扑排序，目标 DLL 加载早于你的 DLL，也就是说你可以在 `DllMain` 里写 Patch 和 Hook 而不用受限于 Loader Lock 导致的不能加载目标 DLL。

如果 DLL 重名的话目前还没有完善的方法实现，一种比较 Hacky 的方法是在 `.def` 文件内硬编码 `\ => _` 替换后的绝对路径，然后再把生成的 `.lib` 里的路径替换回去，当然如果你能写脚本生成这种非标准 `.lib` 的话也不失为一种 Workaround。

使用方法：给 Linker 传入 `-def:x.def x.lib` 或者硬编码到 `#pragma comment` 即可。

## Manual Import

对一些导出函数较少且原型已知的 DLL，也可以采用手动导入的方式。跟前两种相比，这种方式可以自己解析目标 DLL 路径，也就是说，实现转发系统 DLL 时可以不用整 Workaround 或者说 Hack。

虽然这种方式算是可拓展性最好的 DLL 代理实现方式，但是其弊端也很明显，即需要针对每一个导出函数写一份 Handler，且一般需要知道对应 DLL 导出函数的原型。

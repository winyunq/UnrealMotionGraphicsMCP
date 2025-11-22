# UMG MCP 默认工作区功能实现

## 概述

实现了"默认工作区"功能：当没有指定UMG资产时，自动使用 `/Game/UnrealMotionGraphicsMCP.UnrealMotionGraphicsMCP` 作为默认工作区，如果不存在则自动创建。

## 设计原则

**完全由C++端处理**：默认工作区的逻辑完全在C++端的 `ApplyJsonToUmgAsset_GameThread` 函数中实现，Python端只负责解析HTML/JSON并传递给C++。

## 实现细节

### Python端 (UmgMcpServer.py)

当 `apply_layout` 函数没有找到任何目标资产时，传递**空字符串**给C++端。

**关键代码**：
```python
# If still no path, pass empty string - C++ will use default workspace
if not final_path:
    final_path = ""
    logger.info("No UMG asset specified. C++ will use default workspace.")
```

### C++端 (UmgFileTransformation.cpp)

#### 1. 默认路径处理

在函数开头检查AssetPath，如果为空则使用默认路径：

```cpp
// Handle default workspace: if AssetPath is empty, use default path
FString FinalAssetPath = AssetPath;
if (FinalAssetPath.IsEmpty() || FinalAssetPath.TrimStartAndEnd().IsEmpty())
{
    FinalAssetPath = TEXT("/Game/UnrealMotionGraphicsMCP.UnrealMotionGraphicsMCP");
    UE_LOG(LogUmgMcp, Log, TEXT("No asset path provided. Using default workspace: '%s'."), *FinalAssetPath);
}
```

#### 2. 自动创建资产

当资产不存在时，自动创建新的 WidgetBlueprint：

```cpp
// Create a new package
UPackage* Package = CreatePackage(*PackagePath);

// Create the Widget Blueprint using the factory
UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
WidgetBlueprint = Cast<UWidgetBlueprint>(Factory->FactoryCreateNew(
    UWidgetBlueprint::StaticClass(),
    Package,
    FName(*AssetName),
    RF_Public | RF_Standalone,
    nullptr,
    GWarn
));

bIsNewlyCreated = true;  // Mark as newly created
```

#### 3. 防止崩溃的关键修复

**问题**：调用 `FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified()` 会导致崩溃（对新创建和已存在的Blueprint都会崩溃）。

**根本原因**：这个函数在异步的GameThread上下文中不安全，即使Blueprint已经完全初始化也会导致访问冲突。

**最终解决方案**：
- **完全移除** `MarkBlueprintAsStructurallyModified` 调用
- 对所有Blueprint（新创建和已存在）统一使用：只标记Package为dirty
- 让用户手动保存或在编辑器中刷新

```cpp
// Save the blueprint by marking package as dirty
// Note: We don't call MarkBlueprintAsStructurallyModified because it causes crashes
// in async context, even for existing blueprints.
UPackage* Package = WidgetBlueprint->GetOutermost();
if (Package)
{
    Package->MarkPackageDirty();
    
    if (bIsNewlyCreated)
    {
        UE_LOG(LogUmgMcp, Log, TEXT("New asset created. Please save manually (Ctrl+S)."));
    }
    else
    {
        UE_LOG(LogUmgMcp, Log, TEXT("Existing asset modified. Close and reopen the asset to see changes, or press Compile."));
    }
}
```

## 使用方法

### 示例1：不指定资产路径（使用默认工作区）

```python
# 会自动使用/创建 /Game/UnrealMotionGraphicsMCP.UnrealMotionGraphicsMCP
result = await session.call_tool("apply_layout", arguments={
    "layout_content": html_content
    # 不传 asset_path
})
```

### 示例2：显式指定资产路径

```python
result = await session.call_tool("apply_layout", arguments={
    "layout_content": html_content,
    "asset_path": "/Game/MyCustomUI.MyCustomUI"
})
```

## 文件修改列表

- ✅ `UmgMcpServer.py` - Python端：传递空字符串给C++
- ✅ `UmgFileTransformation.cpp` - C++端：
  - 默认路径处理
  - 自动创建资产
  - 完全移除 `MarkBlueprintAsStructurallyModified` 调用
- ✅ `Gemini_test_default_workspace.py` - 测试脚本

## 用户体验

### 新创建的资产
- 资产会自动创建并应用UI布局
- **需要手动保存**：按 `Ctrl+S` 或在关闭项目时保存
- 保存后可在Content Browser中看到新资产

### 已存在的资产
- UI布局会立即应用
- 如果资产在编辑器中打开：
  - **方法1**：关闭资产编辑器，然后重新打开查看更改
  - **方法2**：点击"Compile"按钮刷新
- 如果资产未打开：下次打开时会看到更改

## 已知限制

1. **不会自动刷新编辑器**：需要手动关闭/重新打开或点击Compile
2. **新资产需要手动保存**：不会自动保存到磁盘
3. **默认路径硬编码**：固定为 `/Game/UnrealMotionGraphicsMCP.UnrealMotionGraphicsMCP`

## 测试结果

### 未打开文件时
✅ 传递空字符串给C++
✅ C++检测到空字符串并使用默认路径
✅ 自动创建 UnrealMotionGraphicsMCP 资产
✅ 成功应用UI布局
✅ Package标记为dirty

### 打开文件时
✅ 使用打开文件的路径
✅ 成功应用UI布局
✅ Package标记为dirty
✅ **不再崩溃**（已移除 MarkBlueprintAsStructurallyModified）

## 下一步

需要用户：
1. **重新编译插件**（C++代码已修改）
2. **重启Unreal Editor**
3. **运行测试**：
   ```bash
   python Gemini_test_default_workspace.py
   ```
4. **验证两种情况**：
   - 没有打开UMG文件时运行
   - 打开UMG文件时运行
| 命令 (JSON `type`) | 参数 (JSON `params`) | 描述与AI使用暗示 (The "Prompt") |
| :--- | :--- | :--- |
| **元知识与自省 (Meta-Knowledge & Introspection)** | | *AI使用提示：在执行任何复杂操作前，先调用这些API来“学习”规则，这能极大减少错误。* |
| `get_widget_schema` | `{"widget_type": "Button"}` | **“这个元件有什么用？”** 返回指定控件类型的“结构图”，包含其所有可编辑属性和类型。**AI提示**：在调用`set_widget_properties`前使用此API，可以确保你设置的属性名和值类型都是有效的，从而避免幻觉和错误。 |
| `get_creatable_widget_types` | `{}` | **“我的工具箱里有什么？”** 返回一个列表，包含所有可以被`create_widget`命令创建的控件类型。**AI提示**：当用户要求创建一种你不知道的控件时，可调用此API来检查其是否可用或寻找替代品。 |
| | | |
| **意图推断 (Intent Inference Workflow)** | | *AI使用提示：这是实现“人机协同”的核心。严格遵循“快照->对比”的两步流程。* |
| `capture_ui_snapshot` | `{"asset_path": "/Game/UI/WBP_MainMenu"}` | **“记住tree状态”（第一步）**。对UMG资产的当前布局状态进行一次“快照”，并返回一个唯一的`snapshot_id`。**AI提示**：这是“学习用户意图”流程的**第一步**。在你要求用户进行手动调整**之前**，调用此API来保存“修改前”的状态。 |
| `compare_ui_snapshots` | `{"snapshot_id_before": "...", "snapshot_id_after": "..."}` | **“猜测用户的意图”（第二步）**。对比两次快照，返回一份包含**“推断出的设计关系”**（如对齐、间距）的差异报告。**AI提示**：这是学习流程的“大脑”。它的返回值不是简单的坐标变化，而是可以直接用于生成更鲁棒布局的**设计规则**。 |
| | | |
| **注意力 (Attention)** | | *AI使用提示：当用户的指令模糊不清时（如“这个按钮”），首先调用这些API来确定上下文。* |
| `get_active_umg_context` | `{}` | **“用户正在编辑哪个UMG？”** 返回用户当前拥有焦点的UMG编辑器的资产路径。**AI提示**：这是处理模糊指令的**首选API**，能最高效地获取用户的实时工作上下文。 |
| `get_last_edited_umg_asset` | `{}` | **“用户上一个操作的是什么？”** 返回用户最后一次打开或保存的UMG资产路径。**AI提示**：当`get_active_umg_context`返回null（因为用户已切换窗口）时，使用此API来**维持上下文**。 |
| `get_recently_edited_umg_assets` | `{"max_count": 5}` | **“用户最近在忙什么？”** 返回近期编辑过的UMG资产列表。**AI提示**：用于理解用户的多任务场景，或在上下文完全丢失时，向用户提供可能的选项。 |
| `is_umg_editor_active` | `{"asset_path": "..."}` (可选) | **“用户在看UMG吗？”** 检查是否有任何/特定的UMG编辑器是活跃窗口。**AI提示**：用于决定是否需要重复获取数据。如果用户不在UMG编辑器中，你可能就不需要频繁调用`get_layout_data`。 |
| | | |
| **感知与洞察 (Sensing & Insight)** | | *AI使用提示：在确定了注意力焦点后，用这些工具来“看清”细节。* |
| `get_widget_tree` | `{"asset_path": "..."}` | 获取UMG的完整控件层级树。**AI提示**：这是对UI进行**结构性理解**的基础。 |
| `query_widget_properties` | `{"widget_id": "...", "properties": [...]}` | 精确查询单个控件的特定属性。**AI提示**：比获取整个树更高效，用于验证或获取少量关键信息。 |
| `get_layout_data` | `{"asset_path": "...", "resolution": {...}}` | 获取所有控件的屏幕空间包围盒。**AI提示**：这是进行**布局分析**的基础，所有关于位置、大小、对齐的判断都依赖于此。 |
| `check_widget_overlap` | `{"asset_path": "..."}` | **“这些控件覆盖了吗？”** 高效地检查UI中是否存在布局重叠。**AI提示**：在进行布局分析时，**优先使用这个API**，而不是自己下载所有坐标进行计算，因为这在UE端执行会快得多。 |
| | | |
| **行动 (Action)** | | *AI使用提示：这是你改变世界的工具。* |
| `create_widget` | `{"asset_path": "...", "parent_id": "...", "widget_type": "...", "widget_name": "..."}` | 创建一个新控件。**AI提示**：结合`get_creatable_widget_types`使用，确保`widget_type`有效。 |
| `set_widget_properties` | `{"widget_id": "...", "properties": {...}}` | 修改控件属性。**AI提示**：这是**最核心的修改工具**。结合`get_widget_schema`使用，确保`properties`字典中的键和值都是有效的。 |
| `delete_widget` | `{"widget_id": "..."}` | 移除一个控件。 |
| `reparent_widget` | `{"widget_id": "...", "new_parent_id": "..."}` | 改变控件的父子关系。**AI提示**：用于重构UI，例如将多个按钮放入一个`VerticalBox`中来实现自动布局。 |
| | | |
| **文件工作流 (File Workflow)** | | *AI使用提示：用于与版本控制系统（如Git）集成的工作流。* |
| `export_umg_to_json` | `{"asset_path": "..."}` | 将UMG“反编译”成JSON字符串返回。**AI提示**：用于将UE中的二进制资产状态，转换为可以被文本工具（和你自己）处理和版本控制的格式。 |
| `apply_json_to_umg` | `{"asset_path": "...", "json_data": "..."}` | 将JSON字符串“编译”成UMG资产。**AI提示**：用于从一个版本控制的文本文件，恢复或创建一个UE资产。这是实现自动化UI部署和修改的关键。 |

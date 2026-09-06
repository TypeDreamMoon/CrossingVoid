# ZDBridge

ZDBridge 是 CrossingVoid 工程的 Unreal Editor 桥接插件，用于把外部工具生成的动画同步计划安全地落地到 Paper2D 与 PaperZD 资产中。

## 解决的问题

Unreal Python 对部分 Paper2D 编辑器字段的反射支持不完整，例如 `UPaperSpriteFactory::InitialTexture` 在当前引擎版本中不是可写的 UPROPERTY；同时，Content Browser 的多选菜单操作没有稳定的公开 Python 命令接口。ZDBridge 在编辑器 C++ 层调用 Paper2D/PaperZD 类型和 AssetTools，避免依赖猜测菜单命令或模拟鼠标操作。

## 当前能力

- 从 `UTexture2D` 创建并保存 `UPaperSprite`；
- 按帧顺序和 `FrameRun` 批量创建/更新 `UPaperFlipbook`；
- 创建或更新 `UPaperZDAnimSequence_Flipbook`；
- 将 Flipbook 写入 PaperZD 序列的 `AnimData.Animation`；
- 将 PaperZD 序列绑定到 AnimMaps 的 Animation Source，使其出现在编辑器的 Supported Animations 列表中；
- 为后续动画通知、语音通知、攻击判定和特效通知扩展提供统一 C++ 入口。

## 暴露接口

`UZDBridgeLibrary` 位于 `Source/ZDBridge/Public/ZDBridgeLibrary.h`：

- `CreatePaperSpriteFromTexture`
- `CreatePaperFlipbookFromSprites`
- `CreatePaperZDSequence`
- `AddOrUpdateSupportedAnimation`

`AddOrUpdateSupportedAnimation` 会检查序列的 `AnimSource`；若未绑定到目标 AnimMaps，则设置为目标 `PaperZDAnimationSource` 并标记资产为脏。这对应 PaperZD 编辑器中将序列加入目标 Animation Source 的操作。PaperZD 2.2 的 `Supported Animations` 是编辑器根据该关联显示的列表，不是 AnimMaps 资产上的普通可写数组。

这些函数均带有 `BlueprintCallable` 和 `CallInEditor`，可由编辑器蓝图、Editor Utility 或后续 Python 调用。

## 安装与编译

插件目录：

```text
C:\CrossingVoid\Plugins\ZDBridge
```

工程已在 `C:\CrossingVoid\CrossingVoid.uproject` 中启用插件。使用匹配的 Unreal Engine 版本编译 `CrossingVoidEditor` 目标即可。

构建前必须关闭 Unreal Editor，或关闭 Live Coding；否则 UnrealBuildTool 会拒绝替换正在使用的模块文件。

示例命令：

```text
D:\UnrealEngine-5.8.2\Engine\Build\BatchFiles\Build.bat CrossingVoidEditor Win64 Development -Project=C:\CrossingVoid\CrossingVoid.uproject -WaitMutex
```

## 设计原则

1. 资产创建优先使用 Unreal `AssetTools`；
2. 资产字段写入优先使用公开 API，必要时在编辑器模块中使用受控反射；
3. 每次修改资产后标记 Package Dirty，由调用方统一保存；
4. 不依赖不稳定的 Content Browser 菜单命令 ID；
5. 对外部同步失败保留明确错误文本，避免静默生成错误资产。

## 后续扩展

动画通知建议继续放在 ZDBridge 中实现，由桥接层负责：

- 创建 PaperZD AnimNotify 数据；
- 设置通知所在帧和持续时间；
- 绑定通知名称与参数；
- 批量复制旧序列通知；
- 保存序列并触发 AnimMaps/蓝图刷新。

这样外部 C# 工具只需要传递结构化 JSON，Unreal 编辑器侧负责调用 PaperZD 的真实数据结构，后续扩展通知系统时无需再次依赖 Python 私有字段或 UI 自动化。

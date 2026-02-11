#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "UmgMcpMaterialSubsystem.generated.h"

/**
 * @brief       UMGMCP 材质操控子系统
 * 
 * 本子系统作为 MCP 5 Pillars 战略的核心状态机和 API 提供者，
 * 负责管理当前编辑上下文 (Target Material) 并提供节点操作、连接和属性设置的原子操作。
 */
UCLASS()
class UMGMCP_API UUmgMcpMaterialSubsystem : public UEditorSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /**
     * @brief       设置当前编辑的目标材质
     * 
     * 加载指定的材质资产。如果不存在，根据配置可自动创建。
     * 此操作是所有后续编辑操作的锚点。
     * 
     * @param       参数名称: AssetPath                     数据类型:        FString
     * @param       参数名称: bCreateIfNotFound             数据类型:        bool
     * 
     * @return      状态描述信息                            数据类型:        FString
     **/
    FString SetTargetMaterial(const FString& AssetPath, bool bCreateIfNotFound = true);

    /**
     * @brief       获取当前目标材质
     * 
     * @return      材质指针                                数据类型:        UMaterial*
     **/
    UMaterial* GetTargetMaterial() const;

    /**
     * @brief       定义外部参数 (Scalar/Vector/Texture)
     * 
     * 确保材质中存在指定名称和类型的参数。如果存在则返回句柄，不存在则创建。
     * 
     * @param       参数名称: ParamName                     数据类型:        FString
     * @param       参数名称: ParamType                     数据类型:        FString
     * 
     * @return      参数节点句柄                            数据类型:        FString
     **/
    FString DefineVariable(const FString& ParamName, const FString& ParamType);

    /**
     * @brief       向图表中添加材质表达式节点
     * 
     * @param       参数名称: NodeClass                     数据类型:        FString
     * @param       参数名称: NodeName                      数据类型:        FString
     * 
     * @return      新节点句柄                              数据类型:        FString
     **/
    FString AddNode(const FString& NodeClass, const FString& NodeName = TEXT(""));

    /**
     * @brief       通过句柄删除节点
     * 
     * @param       参数名称: NodeHandle                    数据类型:        FString
     * 
     * @return      是否删除成功                            数据类型:        bool
     **/
    bool DeleteNode(const FString& NodeHandle);

    /**
     * @brief       使用可靠的“自然”逻辑连接两个节点
     * 
     * 自动确定源节点的主输出和目标节点的主输入进行连接。
     * 
     * @param       参数名称: FromHandle                    数据类型:        FString
     * @param       参数名称: ToHandle                      数据类型:        FString
     * 
     * @return      是否连接成功                            数据类型:        bool
     **/
    bool ConnectNodes(const FString& FromHandle, const FString& ToHandle);

    /**
     * @brief       连接两个节点之间的特定引脚 (“外科手术式”)
     * 
     * @param       参数名称: FromHandle                    数据类型:        FString
     * @param       参数名称: FromPin                       数据类型:        FString
     * @param       参数名称: ToHandle                      数据类型:        FString
     * @param       参数名称: ToPin                         数据类型:        FString
     * 
     * @return      是否连接成功                            数据类型:        bool
     **/
    bool ConnectPins(const FString& FromHandle, const FString& FromPin, const FString& ToHandle, const FString& ToPin);

    /**
     * @brief       向 Custom 节点注入 HLSL 代码
     * 
     * @param       参数名称: NodeHandle                    数据类型:        FString
     * @param       参数名称: HlslCode                      数据类型:        FString
     * @param       参数名称: InputNames                    数据类型:        TArray<FString>
     * 
     * @return      是否应用成功                            数据类型:        bool
     **/
    bool SetCustomNodeHLSL(const FString& NodeHandle, const FString& HlslCode, const TArray<FString>& InputNames);

    /**
     * @brief       设置节点的一般属性
     * 
     * 使用反射机制设置属性，如 "Constant", "Texture" 等。
     * 
     * @param       参数名称: NodeHandle                    数据类型:        FString
     * @param       参数名称: Properties                    数据类型:        TSharedPtr<FJsonObject>
     * 
     * @return      是否设置成功                            数据类型:        bool
     **/
    bool SetNodeProperties(const FString& NodeHandle, const TSharedPtr<FJsonObject>& Properties);

    /**
     * @brief       获取节点的详细信息（引脚、连接、类名）
     * 
     * @param       参数名称: NodeHandle                    数据类型:        FString
     * 
     * @return      JSON 格式的详细信息                     数据类型:        FString
     **/
    UFUNCTION(BlueprintCallable, Category = "Mcp|Material")
    FString GetNodeInfo(const FString& NodeHandle);

    /**
     * @brief       设置材质的最终输出节点
     * 
     * 将指定节点的输出连接 to 材质的主输出属性 (EmissiveColor/Opacity or MaterialAttributes).
     * 
     * @param       参数名称: NodeHandle                    数据类型:        FString
     * 
     * @return      是否设置成功                            数据类型:        bool
     **/
    bool SetOutputNode(const FString& NodeHandle);

    /**
     * @brief       编译并保存材质
     * 
     * @return      编译状态/错误信息                       数据类型:        FString
     **/
    FString CompileAsset();

private:
     /**
     * @brief       当前编辑的材质引用（直接从MaterialEditor获取）
     */
    TWeakObjectPtr<UMaterial> TargetMaterial;

    /**
     * @brief       通过生成的唯一名称 (Handle) 查找表达式
     * 
     * @param       参数名称: Handle                        数据类型:        FString
     * 
     * @return      表达式指针                              数据类型:        UMaterialExpression*
     * @brief       根据句柄查找节点表达式
     */
    UMaterialExpression* FindExpressionByHandle(const FString& Handle);

    /**
     * @brief       强制刷新材质编辑器 UI
     */
    void ForceRefreshMaterialEditor();
};

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * @brief       材质命令处理类 (JSON-RPC)
 * 
 * 路由 "material_*" 开头的 JSON 命令至 UUmgMcpMaterialSubsystem。
 * 负责参数提取、验证以及 JSON 响应格式化。
 */
class FUmgMcpMaterialCommands
{
public:
    FUmgMcpMaterialCommands();
    ~FUmgMcpMaterialCommands();

    /**
     * @brief       命令处理主入口
     * 
     * @param       参数名称: CommandType                   数据类型:        FString
     * @param       参数名称: Params                        数据类型:        TSharedPtr<FJsonObject>
     * 
     * @return      JSON 响应对象                           数据类型:        TSharedPtr<FJsonObject>
     **/
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    /**
     * @brief       安全获取子系统实例
     * 
     * @return      子系统指针                              数据类型:        class UUmgMcpMaterialSubsystem*
     **/
    class UUmgMcpMaterialSubsystem* GetSubsystem() const;
};

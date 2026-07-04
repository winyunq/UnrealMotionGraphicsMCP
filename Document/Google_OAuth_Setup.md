# Google OAuth 2.0 配置指南 (Winyunq Style)

为了启用插件的身份验证功能（如用户头像、个性化欢迎语等），您需要配置自己的 Google OAuth 凭据。

## 步骤 1：创建 Google Cloud 项目
1. 访问 [Google Cloud Console](https://console.cloud.google.com/)。
2. 创建一个新项目，命名为 `FabUmgMcp` 或您喜欢的名称。

## 步骤 2：启用 API 服务
1. 在左侧菜单中选择 **API 和服务** > **库**。
2. 搜索并启用 **Generative Language API** (供 Gemini 使用)。

## 步骤 3：配置 OAuth 同意屏幕
1. 在左侧菜单中选择 **API 和服务** > **OAuth 同意屏幕**。
2. 选择 **User Type**: **外部 (External)** (如果您有 Google Workspace，也可以选择内部)。
3. 填写必要应用信息（名称、支持邮箱）。
4. 在 **范围 (Scopes)** 中添加：`.../auth/generative-language` 和 `userinfo.profile`。
5. 将您的测试账号 (Gmail) 添加到 **测试用户** 列表中。

## 步骤 4：创建凭据 (Credentials)
1. 在左侧菜单中选择 **凭据**。
2. 点击 **创建凭据** > **OAuth 客户端 ID**。
3. **应用类型** 选择 **桌面应用 (Desktop App)**。
4. 创建后，您将获得 **客户端 ID (Client ID)** 和 **客户端密钥 (Client Secret)**。

## 步骤 5：在代码中填入凭据
1. 打开文件：`Source/UmgMcp/Private/FabServer/Authentication/UmgMcpGoogleAuthenticationProvider.h`。
2. 将获取到的 `ClientID` 和 `ClientSecret` 填入对应的变量中。
   - `ClientID`: `YOUR_CLIENT_ID.apps.googleusercontent.com`
   - `ClientSecret`: `YOUR_CLIENT_SECRET`
3. 重新编译 Unreal Engine 项目。

---
*注：由于 Winyunq 战略强调安全性与开发者掌控，我们目前不建议将 Secret 存储在 Editor Config 文件中。*

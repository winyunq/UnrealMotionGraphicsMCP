/**
 * @file UmgMcpConfig.h
 * @brief Defines default configuration values for the MCP server.
 *
 * This header contains default constants for the server's host address and port.
 * These values are used as fallbacks if a configuration file is not found or
 * is missing the required fields.
 */
#pragma once

#include "CoreMinimal.h"

// Default settings for MCP Server
#define MCP_SERVER_HOST_DEFAULT "127.0.0.1"
#define MCP_SERVER_PORT_DEFAULT 55557
#define MCP_SOCKET_TIMEOUT_DEFAULT 0.1f
#define MCP_GAME_THREAD_TIMEOUT_DEFAULT 3.0f
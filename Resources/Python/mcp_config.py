# mcp_config.py
# This file contains the configuration for the UmgMcp server.
# It can be modified by installer scripts or by the user directly.

# The IP address of the Unreal Engine editor running the UmgMcp plugin.
# '127.0.0.1' is the standard for local machine communication.
UNREAL_HOST = "127.0.0.1"

# Port 0 means "discover an active UE editor". Each editor binds its own
# OS-assigned ephemeral port and publishes that actual port in the shared
# UmgMcp instance registry.
UNREAL_PORT = 0

# Socket timeout in seconds.
# Increase this if you experience timeouts during heavy operations or over slower networks (VPN).
SOCKET_TIMEOUT = 3

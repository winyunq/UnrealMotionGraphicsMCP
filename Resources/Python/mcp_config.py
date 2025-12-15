# mcp_config.py
# This file contains the configuration for the UmgMcp server.
# It can be modified by installer scripts or by the user directly.

# The IP address of the Unreal Engine editor running the UmgMcp plugin.
# '127.0.0.1' is the standard for local machine communication.
UNREAL_HOST = "127.0.0.1"

# The port that the UmgMcp plugin is listening on.
# Ensure this matches the port configured in the Unreal Engine plugin settings.
UNREAL_PORT = 55557 # Changed to a more unique port for our project

# Socket timeout in seconds.
# Increase this if you experience timeouts during heavy operations or over slower networks (VPN).
SOCKET_TIMEOUT = 3
import asyncio
import json
import logging
import sys

# Configure basic logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - [MOCK UE5] - %(levelname)s - %(message)s'
)
logger = logging.getLogger("MockUE5")

HOST = "127.0.0.1"
PORT = 55557  # Standard port as per mcp_config.py logic

async def handle_client(reader, writer):
    addr = writer.get_extra_info('peername')
    logger.info(f"✅ Connection accepted from {addr}")

    buffer = b""
    try:
        while True:
            # Read in small chunks to see arrival immediately
            chunk = await reader.read(4096)
            if not chunk:
                logger.info(f"❌ Connection closed/EOF by {addr}")
                break
            
            logger.info(f"📥 Received data chunk: {len(chunk)} bytes")
            buffer += chunk
            
            if b'\0' in buffer:
                while b'\0' in buffer:
                    message_bytes, buffer = buffer.split(b'\0', 1)
                    message_str = message_bytes.decode('utf-8')
                    logger.info(f"📜 Full Message Received:\n{message_str}")
                    
                    # Simulate processing delay slightly?
                    # await asyncio.sleep(0.1)

                    # Prepare mock response
                    try:
                        req = json.loads(message_str)
                        cmd = req.get("command", "unknown")
                    except:
                        cmd = "invalid_json"

                    response = {
                        "status": "success",
                        "success": True, 
                        "command": cmd,
                        "result": {
                            "data": {
                                "message": "Hello from Mock Unreal Engine!",
                                "asset_path": "/Game/Mock/TestAsset"
                            }
                        }
                    }
                    
                    response_json = json.dumps(response)
                    logger.info(f"📤 Sending Response: {response_json}")
                    
                    writer.write(response_json.encode('utf-8') + b'\0')
                    await writer.drain()
                    logger.info("✅ Response flushed.")
    
    except ConnectionResetError:
        logger.warning(f"⚠️ Connection reset by {addr}")
    except Exception as e:
        logger.error(f"🔥 Error: {e}")
    finally:
        writer.close()
        await writer.wait_closed()
        logger.info(f"🔒 Socket closed for {addr}")

async def main():
    logger.info(f"🚀 Mock Unreal Engine Server starting on {HOST}:{PORT}")
    logger.info("Waiting for incoming connections from UmgMcpServer...")
    
    server = await asyncio.start_server(handle_client, HOST, PORT)
    
    async with server:
        await server.serve_forever()

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        logger.info("Stopped by user.")

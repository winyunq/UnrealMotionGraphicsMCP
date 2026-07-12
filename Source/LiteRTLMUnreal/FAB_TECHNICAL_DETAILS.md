# Technical Details

## Features
1. **High-performance local LLM inference**: Powered by Google's LiteRT (TensorFlow Lite) optimized for UE5.
2. **Instant Session Switching**: Pointer-based KV-cache mapping allows switching between multiple agents with <1ms latency.
3. **Constrained Decoding**: Force model outputs to follow specific Regex patterns or valid JSON structures during generation.
4. **Real-time VRAM Guard**: Intelligent memory management using DXGI to query available VRAM and prevent D3D device loss.
5. **Incremental Message Sync**: Optimized synchronization protocol that only sends new messages to the inference engine.
6. **Zero-Dependency Bridge**: C-style ABI firewall encapsulates heavy dependencies (Abseil/Protobuf) for maximum stability.
7. **Blueprint & C++ Friendly**: Includes both a high-level Actor Component and a low-level static C++ API.

## Code Modules
- `LiteRTLMUnreal` (Runtime)

## Technical Specifications
- **Number of Blueprints**: 0
- **Number of C++ Classes**: 4 (Core)
- **Network Replicated**: No
- **Supported Development Platforms**: Windows, macOS
- **Supported Target Build Platforms**: Win64 (DX12/Vulkan), macOS arm64 (Metal/CPU)

## Documentation & Support
- **Documentation Link**: [https://winyunq.github.io/LiteRT-LM-Unreal/index.html]
- **Example Project**: N/A
- **Important/Additional Notes**: Requires compatible LiteRT runtime libraries for inference.

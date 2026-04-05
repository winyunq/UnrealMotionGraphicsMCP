import os

def check_copyright(directory):
    # 定义需要检查的版权声明字符串
    copyright_string = "// Copyright (c) 2025-2026 Winyunq. All rights reserved."
    
    # 定义需要检查的文件后缀
    extensions = {'.cpp', '.h', '.hpp', '.c'}
    
    missing_files = []

    # 遍历目录
    for root, dirs, files in os.walk(directory):
        for file in files:
            if os.path.splitext(file)[1].lower() in extensions:
                file_path = os.path.join(root, file)
                
                try:
                    with open(file_path, 'r', encoding='utf-8') as f:
                        # 读取前几行进行检查（通常版权信息在第一行）
                        content = f.read(200) # 读取前200个字符足够覆盖头部
                        if copyright_string not in content:
                            missing_files.append(file_path)
                except Exception as e:
                    print(f"无法读取文件 {file_path}: {e}")

    # 输出结果
    if missing_files:
        print(f"以下文件缺少版权声明 ({len(missing_files)} 个):")
        for path in missing_files:
            print(f"  - {path}")
    else:
        print("所有文件均已包含正确的版权声明！")

if __name__ == "__main__":
    # 将 '.' 替换为你想要检查的代码目录路径
    target_dir = input("请输入要检查的代码目录路径 (直接回车检查当前目录): ").strip() or "."
    check_copyright(target_dir)
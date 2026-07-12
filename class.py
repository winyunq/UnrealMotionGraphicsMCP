import os
import re

def count_classes_in_folder(target_dir):
    # 匹配 class 关键字，排除掉前向声明（即排除以分号结尾的 class A;）
    # 逻辑：匹配 class + 类名 + (可能有的继承) + 左大括号或换行
    class_regex = re.compile(r'\bclass\s+([A-Za-z_]\w*)[\s\w_:]*(?=\{|\n|$)')
    
    unique_classes = set()
    total_class_definitions = 0
    
    for root, _, files in os.walk(target_dir):
        for file in files:
            # 限制文件类型，通常为 .h, .cpp, .py 等，可根据需要调整
            if file.endswith(('.h', '.cpp', '.cs', '.py')):
                file_path = os.path.join(root, file)
                try:
                    with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                        lines = f.readlines()
                        
                        for line in lines:
                            clean_line = line.strip()
                            
                            # 1. 过滤掉注释行
                            if clean_line.startswith(("//", "/*", "*", "#")):
                                continue
                            
                            # 2. 匹配 class 定义
                            match = class_regex.search(clean_line)
                            if match:
                                # 进一步排除掉前向声明（判断行尾是否有分号）
                                if not clean_line.endswith(';'):
                                    class_name = match.group(1)
                                    unique_classes.add(class_name)
                                    total_class_definitions += 1
                                    
                except Exception as e:
                    print(f"无法读取文件 {file_path}: {e}")

    return len(unique_classes), total_class_definitions

# 执行统计
if __name__ == "__main__":
    folder_to_scan = r'Source'
    unique_count, total_count = count_classes_in_folder(folder_to_scan)
    
    print(f"统计报告：")
    print(f"--- 文件夹: {folder_to_scan}")
    print(f"--- 发现的不重复类 (Class) 总数: {unique_count}")
    print(f"--- 累计出现的类定义次数: {total_count}")
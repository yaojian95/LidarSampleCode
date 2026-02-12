import os
import cv2
import numpy as np
from pathlib import Path

def stitch_images_horizontal_by_timestamp(input_folder, output_path=None):
    """
    将文件夹中的图片按时间戳顺序左右拼接
    
    Args:
        input_folder: 输入文件夹路径
        output_path: 输出文件路径，如果为None则保存在输入文件夹
    """
    # 获取所有图片文件（修复重复查找问题）
    image_extensions = ('.png', '.jpg', '.jpeg', '.bmp', '.tiff', '.tif')
    image_files = []
    
    for ext in image_extensions:
        # 只查找小写扩展名，避免重复
        image_files.extend(Path(input_folder).glob(f'*{ext}'))
    
    # 如果没找到，再查找大写扩展名
    if not image_files:
        for ext in image_extensions:
            image_files.extend(Path(input_folder).glob(f'*{ext.upper()}'))
    
    if not image_files:
        print(f"在 {input_folder} 中没有找到图片文件")
        return
    
    # 按文件名（时间戳）排序
    image_files.sort(key=lambda x: str(x.name))
    
    print(f"找到 {len(image_files)} 张图片（按时间戳顺序）:")
    for i, img_file in enumerate(image_files[:10]):  # 只显示前10张
        print(f"  {i+1:3d}. {img_file.name}")
    if len(image_files) > 10:
        print(f"  ... 共 {len(image_files)} 张")
    
    # 读取第一张图片获取尺寸信息
    first_img = cv2.imread(str(image_files[0]))
    if first_img is None:
        print(f"无法读取图片: {image_files[0]}")
        return
    
    img_height, img_width = first_img.shape[:2]
    
    # 计算总宽度
    total_width = img_width * len(image_files)
    
    print(f"\n图片尺寸: {img_width} x {img_height}")
    print(f"拼接后尺寸: {total_width} x {img_height}")
    print(f"拼接图片数量: {len(image_files)}")
    
    # 创建拼接画布
    stitched_image = np.zeros((img_height, total_width, 3), dtype=np.uint8)
    
    # 逐个拼接图片
    success_count = 0
    for idx, img_path in enumerate(image_files):
        img = cv2.imread(str(img_path))
        if img is None:
            print(f"警告: 无法读取图片 {img_path.name}，跳过")
            continue
        
        # 调整图片尺寸，确保高度一致
        if img.shape[0] != img_height:
            aspect_ratio = img.shape[1] / img.shape[0]
            new_width = int(img_height * aspect_ratio)
            img = cv2.resize(img, (new_width, img_height))
        
        # 计算放置位置
        x_start = idx * img_width
        x_end = x_start + img.shape[1]
        
        # 确保不超出边界
        if x_end <= total_width:
            stitched_image[:, x_start:x_end] = img[:, :img.shape[1]]
            success_count += 1
        
        # 显示进度
        if (idx + 1) % 50 == 0 or idx == len(image_files) - 1:
            print(f"  进度: {idx + 1}/{len(image_files)}")
    
    print(f"\n成功拼接 {success_count} 张图片")
    
    # 生成输出文件名
    if output_path is None:
        first_timestamp = image_files[0].stem
        last_timestamp = image_files[-1].stem
        output_filename = f"stitched_{first_timestamp}_to_{last_timestamp}.png"
        output_path = Path(input_folder) / output_filename
    
    # 保存拼接后的图片
    cv2.imwrite(str(output_path), stitched_image)
    print(f"\n拼接完成！")
    print(f"保存路径: {output_path}")
    
    return stitched_image

def main():
    # 设置图片文件夹路径
    input_folder = r"E:\yaojian\lidar_profiling_SDK\Demo\SampleCode\SampleCode\build\Release\DepthMaps"
    
    # 检查文件夹是否存在
    if not os.path.exists(input_folder):
        print(f"错误：文件夹不存在 - {input_folder}")
        return
    
    # 执行拼接
    result = stitch_images_horizontal_by_timestamp(input_folder)
    
    if result is not None:
        print("\n拼接成功！")
        
        # 可选：显示拼接结果（2秒后自动关闭）
        try:
            # 如果图片太大，可以缩小显示
            display_img = result
            if result.shape[1] > 1920:  # 如果宽度超过1920
                scale = 1920 / result.shape[1]
                new_width = 1920
                new_height = int(result.shape[0] * scale)
                display_img = cv2.resize(result, (new_width, new_height))
            
            cv2.imshow('Stitched Image', display_img)
            print("预览窗口将在2秒后自动关闭...")
            cv2.waitKey(2000)
            cv2.destroyAllWindows()
        except:
            print("无法显示图片（可能没有图形界面）")
    else:
        print("拼接失败！")
    
    print("程序执行完毕！")

if __name__ == "__main__":
    main()
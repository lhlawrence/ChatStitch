import os
import csv
from PIL import Image, ImageDraw, ImageFont
from datetime import datetime
import openai

# import matplotlib.pyplot as plt
class BackgroundStitchingAgent:
    def __init__(self, config):
        self.config = config

    def llm_background_stitching(self, scene, message):
        try:
            q0 = "I will provide you with an operation statement to showe a occupied vehicle, and I need you to determine in which image and scene the background stitching should be performed. "
            q1 = "You need to return a JSON dictionary with 2 keys, including "
            q2 = "(1) 'scene_num', representing the scene number where the background stitching should be performed. If the scene number is not mentioned, the value is just '0'."
            q3 = "(2) 'image_num', representing the image number where the background stitching should be performed. If the image number is not mentioned, the value is just '0'."
            q4 = "An example: Given operation statement 'Stitch the background images for scene 1 and image 2', you should return: {'scene_num': '1', 'image_num': '2'}"
            q5 = "Note that you should not return any code or explanations, only provide a JSON dictionary."
            q6 = "The operation statement is:" + message
            prompt_list = [q0, q1, q2, q3, q4, q5, q6]
            result = openai.chat.completions.create(
                model="gpt-4o",
                messages=[{"role": "system", "content": "You are an assistant helping me to determine the scene and image number for background stitching."}] + \
                        [{"role": "user", "content": q} for q in prompt_list]
            )

            answer = result.choices[0].message.content
            print(f"LLM answer: {answer}")
            start = answer.index("{")
            answer = answer[start:]
            end = answer.rfind("}")
            answer = answer[:end+1]

        except Exception as e:
            print(e)
            return "[Background Stitching Agent LLM] determining scene and image number fails."
        
        



    def func_stitch_background(self, scene):
        """
        Stitch the background images for the scene, store results in scene.current_images,and add timestamp to the stitched image.
        """
        print(f"Stitching background images for the scene.")
        # if have all stitched
        #定义路径
        base_path = "/home/lianghao/project/CrossView_ws/data/SurroundView/VID_20231020_172334"
        scene_path = os.path.join(base_path, f"scene{scene.scene_num}")
        csv_file = os.path.join(scene_path, f"scene{scene.scene_num}.csv")
        img_dir = os.path.join(scene_path, "imgs")

        # 读取 CSV 文件
        frame_timestamps = {}
        if not os.path.exists(csv_file):
            print(f"Error: CSV file not found: {csv_file}")
            return

        with open(csv_file, mode='r') as file:
            csv_reader = csv.reader(file)
            for row in csv_reader:
                frame_number = int(row[0])
                timestamp = float(row[1])
                frame_timestamps[frame_number] = timestamp

        # 处理图像
        stitched_images = []
        for frame_number in sorted(frame_timestamps.keys()):
            img_file = os.path.join(img_dir, f"{frame_number}.jpg")
            if not os.path.exists(img_file):
                print(f"Warning: Image file not found: {img_file}")
                continue

            # 打开图像
            image = Image.open(img_file)

            # 添加时间戳
            draw = ImageDraw.Draw(image)
            # font = ImageFont.load_default()
            # 加载一个 TrueType 字体文件并指定大小
            # 这里使用 Arial 字体作为示例，确保在你的系统上有这个字体文件
            font_path = "/usr/share/fonts/truetype/abyssinica/AbyssinicaSIL-Regular.ttf"  # 替换为你的字体文件路径
            font_size = 30  # 设置字体大小
            font = ImageFont.truetype(font_path, font_size)
            timestamp_str = datetime.fromtimestamp(frame_timestamps[frame_number]).strftime('%Y-%m-%d %H:%M:%S.%f')
            draw.text((10, 10), timestamp_str, fill="white", font=font)

            # 将处理后的图像添加到列表中
            stitched_images.append(image)

        # 将拼接的图像存储到 scene.current_images
        scene.current_images = stitched_images
        # 更新时间戳
        scene.current_timestamps = list(frame_timestamps.values())

        print("Background stitching completed.")
        # print(scene.current_timestamps)
        # # 播放 current_images 中的图像
        # if scene.current_images:
        #     plt.ion()  # 开启交互模式
        #     fig, ax = plt.subplots()

        #     for image in scene.current_images:
        #         ax.imshow(image)
        #         plt.pause(0.1)  # 暂停0.5秒以显示每张图像
        #         ax.clear()  # 清除当前图像以显示下一张

        #     plt.ioff()  # 关闭交互模式
        #     plt.show()  # 显示最后一帧
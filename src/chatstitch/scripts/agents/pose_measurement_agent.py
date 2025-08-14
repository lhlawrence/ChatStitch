import openai
from termcolor import colored
import traceback
import os
import csv
import math
# from geometry_msgs.msg import TransformStamped
# import tf_conversions

        
def deg2rad(degrees):
    return degrees * math.pi / 180.0

def gps_to_mercator(lon, lat, alt):
    R = 6378137  # WGS-84半径
    x = R * lon * math.pi / 180.0
    y = R * math.log(math.tan(math.pi / 4.0 + lat * (math.pi / 180.0) / 2.0))
    z = alt
    return x, y, z

class NavigationData:
    def __init__(self, timestamp, latitude, longitude, altitude, eightVel, northVel, upVel, roll, pitch, yaw):
        self.timestamp = timestamp
        self.latitude = latitude
        self.longitude = longitude
        self.altitude = altitude
        self.eightVel = eightVel
        self.northVel = northVel
        self.upVel = upVel
        self.roll = roll
        self.pitch = pitch
        self.yaw = yaw

class PoseMeasurementAgent:
    def __init__(self, config):
        self.config = config

    def llm_pose_measurement(self, scene, message):
        try:
            q0 = "I will provide you with an operation statement to showe a occupied vehicle, and I need you to determine the car's pose should be shown or not. "  

            q1 = "You need to return a JSON dictionary with 1 keys, including "

            q2 = "(1) 'pose_status', representing determine the car's pose should be shown or not .if shown is 1.If the status is not mentioned, the value is just '0'."

            q3 = "An example: Given operation statement 'Show the E300 behind the building', you should return: {'pose_status':'1'}"

            q4 = "Note that you should not return any code or explanations, only provide a JSON dictionary."

            q5 = "The operation statement is:" + message

            prompt_list = [q0,q1,q2,q3,q4,q5]

            result = openai.chat.completions.create(
            model="gpt-4o",
            messages=[{"role": "system", "content": "You are an assistant helping me to determine a car's color and type."}] + \
                    [{"role": "user", "content": q} for q in prompt_list]
            )
            answer = result.choices[0].message.content

            print(f"{colored('[Pose Agent LLM] deciding pose used or not', color='magenta', attrs=['bold'])} \
                    \n{colored('[Raw Response>>>]', attrs=['bold'])} {answer}")

            start = answer.index("{")
            answer = answer[start:]
            end = answer.rfind("}")
            answer = answer[:end+1]
            shown_status = eval(answer)
            print(f"{colored('[Extracted Response>>>]', attrs=['bold'])} {shown_status} \n")

        except Exception as e:
            print(e)
            traceback.print_exc()
            return "[Pose Agent LLM] deciding asset shown or not fails."

        return shown_status
'''
    def func_readgps(self, scene):
        try:
            scene.cars_gps_datas.clear()
            localization_csv_paths = self.config.get('localization_csv_paths', [])
            dataset_path = self.config.get('localization_dataset_path', '')

            for path in localization_csv_paths:
                filename = os.path.join(dataset_path, path)
                print(f"Reading CSV file: {filename}")

                if not os.path.isfile(filename):
                    print(f"Error opening file: {filename}")
                    return False

                gps_data = {}
                with open(filename, newline='') as csvfile:
                    reader = csv.reader(csvfile)
                    for row in reader:
                        if len(row) < 10:
                            continue  # Skip invalid lines
                        timestamp = float(row[0])
                        latitude = float(row[1])
                        longitude = float(row[2])
                        altitude = float(row[3])
                        eightVel = float(row[4])
                        northVel = float(row[5])
                        upVel = float(row[6])
                        roll = float(row[7])
                        pitch = float(row[8])
                        yaw = float(row[9])
                        nav_data = NavigationData(timestamp, latitude, longitude, altitude, eightVel, northVel, upVel, roll, pitch, yaw)
                        gps_data[timestamp] = nav_data

                scene.cars_gps_datas.append(gps_data)

            print(f"Successfully read all {len(localization_csv_paths)} localization CSV files!")
            # print (f"scene.cars_gps_datas: {scene.cars_gps_datas}")
            return True

        except Exception as e:
            print(e)
            traceback.print_exc()
            return "[Pose Agent] Failed to read GPS data."

    def func_align_gps_data(self, scene):
        scene.current_cars_gps_datas.clear()

        for frame_timestamp in scene.current_timestamps:
            frame_nav_datas = []

            for car_gps_data in scene.cars_gps_datas:
                # 找到第一个不小于 frame_timestamp 的导航数据条目
                timestamps = sorted(car_gps_data.keys())
                nav_data = None
                for timestamp in timestamps:
                    if timestamp >= frame_timestamp:
                        nav_data = car_gps_data[timestamp]
                        break

                if nav_data is None:
                    print(f"Error: No navigation data found for timestamp {frame_timestamp}")
                    frame_nav_datas.append(None)
                else:
                    frame_nav_datas.append(nav_data)

            scene.current_cars_gps_datas.append(frame_nav_datas)
        # print("Successfully aligned GPS data with timestamps!")
        # print(scene.current_cars_gps_datas[0])
        # for i, frame_nav_data in enumerate(scene.current_cars_gps_datas):
        #     car_a_data, car_b_data = frame_nav_data  # 分别获取两个车辆的数据
        #     print(f"Frame {i}:")
        #     if car_a_data:
        #         print(f"  Car A: {car_a_data}")
        #     else:
        #         print("  Car A: No data")
        #     if car_b_data:
        #         print(f"  Car B: {car_b_data}")
        #     else:
        #         print("  Car B: No data")


    def func_gpstotf(self, scene):
        init_x, init_y, init_z = 0.0, 0.0, 0.0  # 初始化参考点

        for frame_nav_datas in scene.current_cars_gps_datas:
            level_msgs = []
            body_msgs = []
            camera_msgs = []

            for carIdx, nav_data in enumerate(frame_nav_datas):
                if nav_data is None:
                    continue  # 如果没有导航数据，跳过这个车辆

                x, y, z = gps_to_mercator(nav_data.longitude, nav_data.latitude, nav_data.altitude)

                # 车辆水平坐标系到世界坐标系
                level_msg = TransformStamped()
                level_msg.header.frame_id = "world"
                level_msg.child_frame_id = f"level_{carIdx}"
                level_msg.transform.translation.x = x - init_x
                level_msg.transform.translation.y = y - init_y
                level_msg.transform.translation.z = z - init_z
                level_msg.transform.rotation.x = 0.0
                level_msg.transform.rotation.y = 0.0
                level_msg.transform.rotation.z = 0.0
                level_msg.transform.rotation.w = 1.0
                level_msgs.append(level_msg)

                # 打印 level_msg 的详细信息
                # print(f"Level Msg for Car {carIdx}:")
                # print(f"  Frame ID: {level_msg.header.frame_id}")
                # print(f"  Child Frame ID: {level_msg.child_frame_id}")
                # print(f"  Translation: ({level_msg.transform.translation.x}, "
                #     f"{level_msg.transform.translation.y}, {level_msg.transform.translation.z})")
                # print(f"  Rotation: ({level_msg.transform.rotation.x}, "
                #   f"{level_msg.transform.rotation.y}, {level_msg.transform.rotation.z}, "
                #   f"{level_msg.transform.rotation.w})")

                # 车辆载体坐标系到车辆水平坐标系
                body_msg = TransformStamped()
                body_msg.header.frame_id = f"level_{carIdx}"
                body_msg.child_frame_id = f"body_{carIdx}"
                body_msg.transform.translation.x = 0.0
                body_msg.transform.translation.y = 0.0
                body_msg.transform.translation.z = 0.0
                q = tf_conversions.transformations.quaternion_from_euler(0.0, 0.0, deg2rad(-nav_data.yaw))
                body_msg.transform.rotation.x = q[0]
                body_msg.transform.rotation.y = q[1]
                body_msg.transform.rotation.z = q[2]
                body_msg.transform.rotation.w = q[3]
                body_msgs.append(body_msg)

                # 全景相机坐标系到车辆载体坐标系
                if carIdx == 0:  # 只有第一辆车上装有全景相机
                    camera_msg = TransformStamped()
                    camera_msg.header.frame_id = f"body_{carIdx}"
                    camera_msg.child_frame_id = f"camera_{carIdx}"
                    camera_msg.transform.translation.x = 0.0
                    camera_msg.transform.translation.y = 0.5
                    camera_msg.transform.translation.z = 0.1
                    camera_msg.transform.rotation.x = 0.0
                    camera_msg.transform.rotation.y = 0.0
                    camera_msg.transform.rotation.z = 0.0
                    camera_msg.transform.rotation.w = 1.0
                    camera_msgs.append(camera_msg)

            scene.level_msgs.append(level_msgs)
            scene.body_msgs.append(body_msgs)
            scene.camera_msgs.append(camera_msgs)
            print("Successfully converted GPS data to TF messages!")
            # print(scene.level_msgs[0])
            # print(scene.body_msgs[0])
            # print(scene.camera_msgs[0])

        return True
'''

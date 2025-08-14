import cv2
import numpy as np
import rospy
from geometry_msgs.msg import TransformStamped
from cv_bridge import CvBridge
from sensor_msgs.msg import Image
import openai
from termcolor import colored
import traceback
# import tf_conversions

class BBox:
    def __init__(self, *args):
        self.bbox_vertices_3d = []
        self.bbox_vertices_2d = []

        if len(args) == 3:
            # 初始化使用长度、宽度和高度
            length, width, height = args
            self.bbox_vertices_3d = [
                tf2_geometry_msgs.Point(0, 0, 0),                          # 0. 中心
                tf2_geometry_msgs.Point(-width/2, -length/2, -height),   # 1. 后左下
                tf2_geometry_msgs.Point(width/2, -length/2, -height),    # 2. 后右下
                tf2_geometry_msgs.Point(-width/2, length/2, -height),    # 3. 前左下
                tf2_geometry_msgs.Point(width/2, length/2, -height),     # 4. 前右下
                tf2_geometry_msgs.Point(-width/2, -length/2, 0),         # 5. 后左上
                tf2_geometry_msgs.Point(width/2, -length/2, 0),          # 6. 后右上
                tf2_geometry_msgs.Point(-width/2, length/2, 0),          # 7. 前左上
                tf2_geometry_msgs.Point(width/2, length/2, 0)            # 8. 前右上
            ]
        elif len(args) == 6:
            # 初始化使用前、后、左、右、上、下距离
            front, back, left, right, up, down = args
            self.bbox_vertices_3d = [
                tf2_geometry_msgs.Point(0, 0, 0),                # 0. 中心
                tf2_geometry_msgs.Point(-left, -back, -down),    # 1. 左后下
                tf2_geometry_msgs.Point(right, -back, -down),    # 2. 右后下
                tf2_geometry_msgs.Point(-left, front, -down),    # 3. 左前下
                tf2_geometry_msgs.Point(right, front, -down),    # 4. 右前下
                tf2_geometry_msgs.Point(-left, -back, up),       # 5. 左后上
                tf2_geometry_msgs.Point(right, -back, up),       # 6. 右后上
                tf2_geometry_msgs.Point(-left, front, up),       # 7. 左前上
                tf2_geometry_msgs.Point(right, front, up)        # 8. 右前上
            ]

        self.bbox_vertices_2d = [cv2.Point2d(0, 0) for _ in range(len(self.bbox_vertices_3d))]

    def draw_3d_bbox(self, image):
        if len(self.bbox_vertices_2d) != len(self.bbox_vertices_3d):
            rospy.logerr("bbox_vertices_2d.size() != bbox_vertices_3d.size()")
            return False

        # 绘制顶点
        for point in self.bbox_vertices_2d:
            cv2.circle(image, (int(point.x), int(point.y)), 4, (0, 0, 255), -1)

        # 绘制底面
        cv2.line(image, self.bbox_vertices_2d[1], self.bbox_vertices_2d[2], (255, 0, 0), 2)
        cv2.line(image, self.bbox_vertices_2d[2], self.bbox_vertices_2d[4], (255, 0, 0), 2)
        cv2.line(image, self.bbox_vertices_2d[4], self.bbox_vertices_2d[3], (255, 0, 0), 2)
        cv2.line(image, self.bbox_vertices_2d[3], self.bbox_vertices_2d[1], (255, 0, 0), 2)

        # 绘制顶面
        cv2.line(image, self.bbox_vertices_2d[5], self.bbox_vertices_2d[6], (255, 0, 0), 2)
        cv2.line(image, self.bbox_vertices_2d[6], self.bbox_vertices_2d[8], (255, 0, 0), 2)
        cv2.line(image, self.bbox_vertices_2d[8], self.bbox_vertices_2d[7], (255, 0, 0), 2)
        cv2.line(image, self.bbox_vertices_2d[7], self.bbox_vertices_2d[5], (255, 0, 0), 2)

        # 绘制侧面
        cv2.line(image, self.bbox_vertices_2d[1], self.bbox_vertices_2d[5], (255, 0, 0), 2)
        cv2.line(image, self.bbox_vertices_2d[2], self.bbox_vertices_2d[6], (255, 0, 0), 2)
        cv2.line(image, self.bbox_vertices_2d[3], self.bbox_vertices_2d[7], (255, 0, 0), 2)
        cv2.line(image, self.bbox_vertices_2d[4], self.bbox_vertices_2d[8], (255, 0, 0), 2)

        return True

    def print(self):
        print("BBox3D:")
        for vertex in self.bbox_vertices_3d:
            print(f"({vertex.x}, {vertex.y}, {vertex.z})")

class PerspectiveMeasurementAgent:
    def __init__(self, config):
        self.config = config

    def llm_perspective_measurement(self, scene, message):
        try:
            q0 = "I will provide you with an operation statement to show a vehicle, and I need you to determine whether the vehicle should be boxed or not."
            
            q1 = "You need to return a JSON dictionary with 1 key 'pers_status', representing whether the vehicle should be boxed or not. If it should be boxed, the value is '1'. If it should not be boxed, the value is '0'. If the status is not mentioned, the value defaults to '0'."
            
            q2 = "An example: Given operation statement 'Show the E300 behind the building', you should return: {'pers_status':'0'}"
            
            q3 = "An example: Given operation statement 'Show the back right view of the E300 behind the building with full perspective measurement.', you should return: {'pers_status':'1'}"

            q4 = "An example: Given operation statement 'Show the vehicle in front of us, do not box it', you should return: {'pers_status':'0'}"

            q5 = "Note that you should not return any code or explanations, only provide a JSON dictionary."

            q6 = "The operation statement is:" + message

            prompt_list = [q0, q1, q2, q3, q4, q5, q6]

            result = openai.chat.completions.create(
                model="gpt-4o",
                messages=[{"role": "system", "content": "You are an assistant helping me to determine whether a vehicle should be boxed."}] + \
                        [{"role": "user", "content": q} for q in prompt_list]
            )
            answer = result.choices[0].message.content

            print(f"{colored('[Perspective Measurement Agent LLM] deciding 3DBBox or not', color='magenta', attrs=['bold'])} \
                    \n{colored('[Raw Response>>>]', attrs=['bold'])} {answer}")
            start = answer.index("{")
            answer = answer[start:]
            end = answer.rfind("}")
            answer = answer[:end+1]
            status = eval(answer)
            print(f"{colored('[Extracted Response>>>]', attrs=['bold'])} {status} \n")
        except Exception as e:
            print(e)
            traceback.print_exc()
            return "[Perspective Measurement Agent LLM] deciding 3DBBox or not fails."
        
        return status
        

'''
    def func_perspective_measurement(self, scene):
        """Perspective measurement agent.
        """
        print("Perspective measurement agent is working.")
        #进行透视测量

        for frame_idx, (timestamp, image) in enumerate(zip(scene.current_timestamps, scene.current_images)):
            # 假设每个车辆都有一组level_msgs, body_msgs, camera_msgs
            try:
                # 这里假设你有两个车辆（0和1），并且你需要从这些消息中提取相应的变换
                T_Body1_Camera0 = self.find_transform(scene.camera_msgs[0], "camera_0", "body_1")
                T_Body1_Body0 = self.find_transform(scene.body_msgs[1], "body_0", "body_1")
                T_Body1_Nerf1 = self.find_transform(scene.level_msgs[1], "nerf_1", "body_1")
            except ValueError as ex:
                rospy.logerr(f"Transform error: {ex}")
                continue

            # 计算两车距离与姿态角
            distance = np.sqrt(
                T_Body1_Body0.transform.translation.x**2 +
                T_Body1_Body0.transform.translation.y**2 +
                T_Body1_Body0.transform.translation.z**2
            )
            q = T_Body1_Body0.transform.rotation
            roll, pitch, yaw = tf_conversions.transformations.euler_from_quaternion([q.x, q.y, q.z, q.w])
            yaw = np.degrees(yaw)
            rospy.loginfo(f"Distance: {distance} m, Yaw: {yaw} degrees")

            # 坐标映射
            bbox = BBox(2,2,1,1,0,2)  # 需要定义 BBox 类并初始化
            for i in range(len(bbox.bbox_vertices_3d)):
                self.target_body_to_panorama(image, T_Body1_Camera0, bbox.bbox_vertices_3d[i], bbox.bbox_vertices_2d[i])

            # 获取 Nerf 渲染图像
            # ngp_img = self.call_render_service(T_Body1_Camera0, T_Body1_Nerf1)

            # 映射 Nerf 渲染结果到全景图像
            # pano_ngp_img, pano_ngp_mask = self.pinhole_nerf_to_panorama(image, ngp_img, bbox.bbox_vertices_2d)

            # 融合图像
            # blended_result = self.blend_images(image, pano_ngp_img, pano_ngp_mask)

            # 绘制 3D BBox
            bbox.draw_3d_bbox(image)

            return scene

            
    def find_transform(self, transforms, target_frame, source_frame):
        for transform in transforms:
            if transform.header.frame_id == target_frame and transform.child_frame_id == source_frame:
                return transform
        raise ValueError(f"Transform from {source_frame} to {target_frame} not found.")

    def target_body_to_panorama(self, panorama, transform, vertex_3d, vertex_2d):
        # 实现将3D点映射到2D图像上的逻辑
        pass

    def call_render_service(self, transform_camera, transform_nerf):
        # 调用渲染服务并返回渲染图像
        pass

    def pinhole_nerf_to_panorama(self, panorama, ngp_img, vertices_2d):
        # 将针孔图像的 Nerf 渲染结果映射到全景图像上
        pass

    def blend_images(self, panorama, ngp_img, ngp_mask):
        # 融合全景图像与 Nerf 渲染图像
        pass
    
'''

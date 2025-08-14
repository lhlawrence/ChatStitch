import os
import numpy as np
from agents.utils import (check_and_mkdirs, generate_vertices)
import datetime
import torch.nn as nn

class Scene(nn.Module):
    def __init__(self, config):
        self.data_root = config['data_root']
        self.scene_name = config['scene_name']
        self.scene_num = config['scene_num']

        self.ext_int_path = os.path.join(self.data_root, self.scene_name, config['ext_int_file'])
        self.bbox_path = os.path.join(self.data_root, self.scene_name, config['bbox_file'])
        self.init_img_path = os.path.join(self.data_root, self.scene_name, config['init_img_file'])

        self.fps = config.get('fps', 20)
        self.frames = config['frames']
        self.multi_process_num = config.get('multi_process_num', 1)

        # 初始化 current_timestamps 以存储每帧的时间戳
        self.current_timestamps = []
        self.current_images = []
        # 初始化 cars_gps_datas 以存储每辆车的 GPS 数据
        self.cars_gps_datas = []
        self.current_cars_gps_datas = []
        # 初始化 tf变换阵
        self.level_msgs = []
        self.body_msgs = []
        self.camera_msgs = []
        """
        [static scene data]  static指的是background不变
        """
        #下面暂时注释掉，因为还不符合要求
        self.bbox_data = np.load(
            self.bbox_path, allow_pickle=True
        ).item()

        # all_current_vertices = []
        # for k in self.bbox_data.keys():
        #     current_vertices = generate_vertices(self.bbox_data[k])
        #     all_current_vertices.append(current_vertices)
        # self.all_current_vertices = np.array(all_current_vertices)

        # if self.all_current_vertices.shape[0] > 0:
        #     self.all_current_vertices_coord = np.mean(self.all_current_vertices,axis=1)[:,:2]
        # else:
        #     self.all_current_vertices_coord = np.zeros((0,2))

        # # read extrinsics from cams_meta.npy. NeRF (RUB) convention.
        # extrinsics = np.load(self.ext_int_path)[:, :12].reshape(-1, 3, 4)
        # extrinsics = extrinsics[:, :3, :4]

        # self.nerf_motion_extrinsics = extrinsics # [N, 3, 4]

        # # read intrinsics from cams_meta.npy 
        # self.intrinsics = np.load(self.ext_int_path)[:, 12:21].reshape(-1, 3, 3)[0]
        # self.focal = self.intrinsics[0, 0]
        # self.height = 1280
        # self.width = 1920

        # if self.is_wide_angle:
        #     self.intrinsics[0, 2] += 1920 # shift the principal point to the right.
        #     self.width = 1920 * 3

        #这里是动态的，保存信息
        """
        [dynamic scene data], will be updated during parsing. 
        ---
        current_extrinsics : np.npdarray [N, 3, 4] 
            N=#frames, correspond to current_images. NeRF (RUB) convention

        current_images : list of np.ndarray [H, W, 3] with len=frames
            Show to users. NeRF's output: current_images

        current_inpainted_images: list of np.ndarray [H, W, 3] with len=frames
            Show to users. NeRF + inpaint's output: current_inpainted_images

        """
        # self.is_ego_motion = False
        # self.add_car_all_static = True # check every time before blender rendering

        # self.current_extrinsics = self.nerf_motion_extrinsics[3:4]  # use the second frame because it is in the training set. Better visualization
        # self.current_extrinsics = self.current_extrinsics.repeat(self.frames, axis=0)

        # self.removed_cars = []  # keys of cars which are removed
        self.shown_cars_dict = {} 
        self.shown_cars_count = 0
        
        # self.past_operations = []

        # self.all_trajectories = []

        # # use current time as cache
        # current_time = datetime.datetime.now()
        # short_scene_name = self.scene_name.lstrip('segment-')[:4]

        # self.logging_name = current_time.strftime(f"{short_scene_name}_%Y_%m_%d_%H_%M_%S")

        # self.save_cache = config['save_cache']
        # self.cache_dir = os.path.join(config["cache_dir"], self.logging_name)
        # self.output_dir = config["output_dir"]

        # check_and_mkdirs(self.cache_dir)
        # check_and_mkdirs(self.output_dir)
    '''
    def setup_cars(self):
        """
        Call at the beginning of each interaction. 
        calculate the information of cars from original scene based on current extrinsic
        """
        # get the information of u, v, depth, mask of each car with current extrinsic
        original_cars_dict = {}
        name_to_bbox_car_id = {}
        bbox_car_id_to_name = {}
        
        mask_list = []
        mask_corners_list = []
        depth_list = []
        u_v_depth_list = []
        car_id_list = []

        for car_id in self.bbox_data.keys():
            extrinsic_for_project = transform_nerf2opencv_convention(
                self.current_extrinsics[0]
            )
            u_v_depth = get_attributes_for_one_car(
                self.bbox_data[car_id], extrinsic_for_project, self.intrinsics
            )
            if (
                u_v_depth["u"] < 0
                or u_v_depth["u"] > self.width
                or u_v_depth["v"] < 0
                or u_v_depth["v"] > self.height
            ):
                continue
            corners = generate_vertices(self.bbox_data[car_id])
            mask, mask_corners = get_outlines(
                corners,
                extrinsic_for_project,
                self.intrinsics,
                self.height,
                self.width,
            )

            mask_list.append(mask)
            mask_corners_list.append(mask_corners)
            depth_list.append(u_v_depth["depth"])
            u_v_depth_list.append(u_v_depth)
            car_id_list.append(car_id)

        # add color information
        color_dict = getColorList()
        for idx_in_list, car_id in enumerate(car_id_list):
            car_name = f"original_car_{car_id}"
            name_to_bbox_car_id[car_name] = car_id
            bbox_car_id_to_name[car_id] = car_name

            original_cars_dict[car_name] = u_v_depth_list[idx_in_list]
            current_mask_corner = mask_corners_list[idx_in_list]

            color = get_color(
                self.current_images[0][
                    current_mask_corner[0] + 50 : current_mask_corner[1] - 50,
                    current_mask_corner[2] + 50 : current_mask_corner[3] - 50,
                ]
            )  

            color_vector = (color_dict[color][0] + color_dict[color][1]) / 2
            color_vector = np.uint8(color_vector.reshape(1, 1, 3))
            original_cars_dict[car_name]["rgb"] = cv2.cvtColor(
                color_vector, cv2.COLOR_HSV2RGB
            )
            original_cars_dict[car_name]["x"] = self.bbox_data[car_id]["cx"]
            original_cars_dict[car_name]["y"] = self.bbox_data[car_id]["cy"]

        self.original_cars_dict = original_cars_dict
        self.name_to_bbox_car_id = name_to_bbox_car_id
        self.bbox_car_id_to_name = bbox_car_id_to_name
    '''
    def show_car(self, shown_car_info):
        """
        Show a single car to self.shown_cars_dict dictionary.
        shown_car_id is the number of cars shown so far.
        """
        # shown_car_dict: {'type': str, 'position': temporal [xyz], 'direction': temporal [theta], ...}
        shown_car_info['need_shown'] = True
        shown_car_id = str(self.shown_cars_count)
        car_name = f'shown_car_{shown_car_id}'

        self.shown_cars_dict[car_name] = shown_car_info
        self.shown_cars_count += 1

        return car_name
import argparse
from agents.utils import read_yaml
from agents.task_managemet import TaskManagementAgent
from agents.asset_select_agent import AssetSelectAgent
from agents.view_change_agent import ViewChangeAgent
from agents.foreground_rendering_agent import ForegroundRenderingAgent
from agents.background_stitch_agent import BackgroundStitchingAgent
from agents.pose_measurement_agent import PoseMeasurementAgent
from agents.perspective_measurement_agent import PerspectiveMeasurementAgent
from scene import Scene
import os
from termcolor import colored
import imageio.v2 as imageio
# import matplotlib.pyplot as plt
class ChatStitch:
    def __init__(self, config):
        self.config = config

        self.scene = Scene(config["scene"])  # agents share and maintain the same scene

        agents_config = config['agents']
        self.project_manager = TaskManagementAgent(agents_config["task_management_agent"])
        self.asset_select_agent = AssetSelectAgent(agents_config["asset_select_agent"])
        self.view_change_agent = ViewChangeAgent(agents_config["view_change_agent"])
        self.foreground_rendering_agent = ForegroundRenderingAgent(agents_config["foreground_rendering_agent"])
        self.background_stitch_agent = BackgroundStitchingAgent(agents_config["background_stitch_agent"])
        self.pose_measurement_agent = PoseMeasurementAgent(agents_config["pose_measurement_agent"])
        self.perspective_measurement_agent = PerspectiveMeasurementAgent(agents_config["perspective_measurement_agent"])

        self.tech_agents = {
            "asset_select_agent": self.asset_select_agent,
            "view_change_agent": self.view_change_agent,
            "foreground_rendering_agent": self.foreground_rendering_agent,
            "background_stitch_agent": self.background_stitch_agent,
            "pose_measurement_agent": self.pose_measurement_agent,
            "perspective_measurement_agent": self.perspective_measurement_agent,
        }

        self.current_prompt = (
            "An empty prompt"  # initialization place holder for debugging
        )

    def setup_init_frame(self):
        """Setup initial frame for ChatStitch's reasoning and stitching.
        """
        if not os.path.exists(self.scene.init_img_path):
            print(f"{colored('[Note]', color='red', attrs=['bold'])} ",
                  f"{colored('can not find init image, stitching it for the first time')}\n")
            # it will stitch scene.current_images
            self.background_stitch_agent.func_stitch_background(self.scene)
            # save the initial image
            imageio.imwrite(self.scene.init_img_path, self.scene.current_images[0])
        else:
            self.scene.current_images = [imageio.imread(self.scene.init_img_path)] * self.scene.frames
            
            # 可视化图像
            # plt.imshow(self.scene.current_images[0])
            # plt.axis('off')  # 隐藏坐标轴
            # plt.show()

    def execute_llms(self, prompt):
        print(f"Executing LLMS with prompt: {prompt}")
        """Entry of ChatStitch's reasoning.
        We perform multi-LLM reasoning for the user's prompt

        Input:
            prompt : str
                language prompt to ChatSim.
        """
        # self.scene.setup_cars()
        self.current_prompt = prompt
        tasks = self.project_manager.decompose_prompt(self.scene, prompt)
        for task in tasks.values():
            print(
                f"{colored('[Performing Single Prompt]', on_color='on_blue', attrs=['bold'])} {colored(task, attrs=['bold'])}\n"
            )
            self.project_manager.dispatch_task(self.scene, task, self.tech_agents)

        print(colored("multi agents end\n", color="red", attrs=["bold"]), end=' ')
        self.project_manager.one_prompt(self.scene, prompt)
        
        # print(colored("scene.shown_cars_dict", color="red", attrs=["bold"]), end=' ')
        # pprint.pprint(self.scene.added_cars_dict.keys())
        # print(colored("scene.unshown_cars", color="red", attrs=["bold"]), end=' ')
        # pprint.pprint(self.scene.removed_cars)

    def execute_funcs(self):
        """Entry of Chatstitch's stitching functions
        We perform agent's functions following the self.scene's configuration.
        self.scene's configuration are updated in self.execute_llms()
        """
        # use scene.current_extrinsics, stitch (novel) view images
        self.background_stitch_agent.func_stitch_background(self.scene)
        self.pose_measurement_agent.func_readgps(self.scene)
        self.pose_measurement_agent.func_align_gps_data(self.scene)
        self.pose_measurement_agent.func_gpstotf(self.scene)
        self.perspective_measurement_agent.func_perspective_measurement(self.scene)

        # Retrieve blender file from asset bank
        self.asset_select_agent.func_retrieve_blender_file(self.scene)

        # Blender add car. If no addition, just return
        self.foreground_rendering_agent.func_blender_add_cars(self.scene)

        # Generate Video
        # generate_video(self.scene, self.current_prompt)



def get_parser():
    parser = argparse.ArgumentParser(description="ChatStitch argrument parser.")
    parser.add_argument(
        "--config_yaml", "-y", type=str, default="/media/lh/D2/BEVProjects/ChatStitch/src/chatstitch/config/config.yaml", help="path to config file")
    parser.add_argument(
        "--prompt", "-p", type=str, default="Display the front-left view of a black tesla_cybertruck measuring 5.8m in length, 2.2m in width, 1.9m in height and enable 3D bbox for it.", help="language prompt to ChatStitch.")
    args, unknown = parser.parse_known_args()
    # args = parser.parse_args()
    return args

if __name__ == "__main__":
    args = get_parser()
    config = read_yaml(args.config_yaml)

    chatstitch = ChatStitch(config)
    chatstitch.setup_init_frame()
    chatstitch.execute_llms(args.prompt)
    chatstitch.execute_funcs()
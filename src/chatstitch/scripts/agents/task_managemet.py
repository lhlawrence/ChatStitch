import openai
from termcolor import colored
import traceback
import time 

class TaskManagementAgent:
    def __init__(self, config):
        self.config = config
    
    def decompose_prompt(self, scene, user_prompt):
        """ decompose the prompt to the corresponding chatstitch.agents.
        Input:
            scene : Scene
                scene object.
            user_prompt : str
                language prompt to ChatSim.
        Return:
            tasks : dict
                a dictionary of decomposed tasks.
        """  
        q0 = "I have a requirement of operations in an autonomous driving scenario, and I need your help to break it down into one or several supportable actions. The scene is large which means many vehicles can be contained."

        q1 = "The supportable two actions include repeat the prompt ,\
                remove the occupied car,\
                show the occupied car."  
        
        q2 = "Please try to retain all the semantics and adjunct words from the original text. Each adding action should only contain one car. " + \
             "Information about adding vehicles (such as their type, positions, driving status, speed, color, etc.) should be directly included within the show occupied action." 
        
        q3 = "Split actions should be stored in a JSON dictonary. The key is action id and the value is specific action. They will be executed sequentially, and the broken operations should be independent with each other and do not rely on the detailed scene information."

        q4 = "An example: the requirement is 'substitute the red car in the scene'. you break it down and return" + \
                "{1: 'remove the red occupied car', 2: 'Add a new car at the location where the red car was deleted'}"
        
        q5 = "An example: the requirement is 'remove all cars', you break it down and return " + \
             "{ 1: 'remove all the occupied cars'} "
        
        q6 = "I may provide very abstract requirements. For such requirements, you should analyze how to comply with the splitting of actions." 

        q7 = "An example (very abstract): the requirement is 'I want show my partners in the scene', you analyse and return " + \
             "{ 1: 'Show one occupied car', 2 : 'Show one occupied car', 3 : 'Show one occupied car'} "
        
        q8 = "An example (complex): the requirement is 'Present left-side view of a green mini, then a bronze porsche, with evolving dimensional indicators.', you analyse and return " + \
                "{ 1: 'Show the left view of a green mini, with perspective measurement', 2: 'Show a bronze porsche, with perspective measurement'} "
        
        q9 = "An example (complex): the requirement is 'I want to see a red E300 and a blue audi, the E300 is on the left of the audi, both of back right view and with perspective measurements .', you analyse and return " + \
                "{1: 'Show the back-view of a red E300 with perspective measurement', 2: 'Show the back-right view of the blue audi beside the E300, with perspective measurement'}"

        q8 = "The scene is large enough to contain more than 20 vehicles. So many vehicles can be shown to the scene. Do not return any code or explanation; only a JSON dictionary is required."

        q9 = "Attention: the adjustments for one specific occupied vehicle should be included in one single output action. If there are multiple adjustments for one already occupied car, these adjustments must be merged in one action."
                
        q10 = "Attention: Do not appear information about the vehicles in the other broken actions."

        q11 = "The requirement is:" + user_prompt

        prompt_list = [q0,q1,q2,q3,q4,q5,q6,q7,q8,q9,q10,q11]

        # client = openai.Openai()
        result = openai.chat.completions.create(
            model="gpt-4o",
            messages=[{"role": "system", "content": "You are an assistant helping me to break down the operations."}] + \
                     [{"role": "user", "content": q} for q in prompt_list]
        )
        answer = result.choices[0].message.content
        
        print(f"{colored('[User prompt]', color='magenta', attrs=['bold'])} {user_prompt}\n")
        print(f"{colored('[Task Managemet Agent] decomposing tasks', color='magenta', attrs=['bold'])} \
               \n{colored('[Raw Response>>>]', attrs=['bold'])} {answer}")

        try:
            start = answer.index("{")
            answer = answer[start:]
            end = answer.rfind("}")
            answer = answer[:end+1]
            tasks = eval(answer)
            print(f"{colored('[Extracted Response>>>]', attrs=['bold'])} {answer} \n")

        except Exception as e:
            print(e)
            traceback.print_exc()
            return "Can not parse the requirement."
        
        return tasks

    def dispatch_task(self, scene, task, tech_agents):
        """ dispatch the task to the corresponding chatstitch.agents.
        Input:
            scene : Scene
                scene object.
            task : dict
                a dictionary of decomposed task.
            tech_agents : list
                a list of technical agents.
        Return:
            callback_message : str
                if encounter bugs, record them in callback_message to users
        """
        operation_category = {1:'show', 2:'unshow'}

        q0 = "I will provide you with an action, and you will help me determine which operation this action belongs to."
        
        q1 = "Operations include (1) show (2) unshow."
        
        q2 = "Return the information in JSON format, with a key named 'operation'."

        q3 = "An Example: Given action 'Remove the red car from the scene', you should return {'operation': 2}"

        q4 = "An Example: Given action 'see the car behind the building', you should return {'operation': 1}"

        q5 = "Note that you should not return any code or explanations, only provide a JSON dictionary."
        
        q6 = task

        prompt_list = [q0,q1,q2,q3,q4,q5,q6]
        result = openai.chat.completions.create(
                    model="gpt-4o",
                    messages=[{"role": "system", "content": "You are an assistant helping me to classify operations."}] + \
                             [{"role": "user", "content": q} for q in prompt_list]
                )
                
        answer = result.choices[0].message.content

        print(f"{colored('[Project Manager] dispatching each task', color='magenta', attrs=['bold'])} \
                \n{colored('[Raw Response>>>]', attrs=['bold'])} {answer}")

        start = answer.index("{")
        answer = answer[start:]
        end = answer.rfind("}")
        answer = answer[:end+1]
        operation = eval(answer)['operation']
        print(f"{colored('[Extracted Response>>>]', attrs=['bold'])} {operation}. ({operation_category[operation]}) \n")

        if operation == 1:

            tstart = time.time()
            a0, a1, a2, a3, a4 = self.show_operation(scene, task, tech_agents)
            tend = time.time()
            return (tend - tstart, a0, a1, a2, a3, a4)


        # elif operation == 2:
        #     self.unshow_operation(scene, task, tech_agents)
    
    def show_operation(self, scene, task, tech_agents):
        """ show the car in the scene.
        Participants: asset_selection_agent, perspective_measurement_agent, pose_measurement_agent
        Input:
            scene : Scene
                scene object.
            task : dict
                a dictionary of decomposed task.
            tech_agents : list
                a list of technical agents.
        Return:
            callback_message : str
                if encounter bugs, record them in callback_message to users
        """
        asset_select_agent = tech_agents['asset_select_agent']
        view_change_agent = tech_agents['view_change_agent']

        background_stitch_agent = tech_agents['background_stitch_agent']
        pose_measurement_agent = tech_agents['pose_measurement_agent']
        perspective_measurement_agent = tech_agents['perspective_measurement_agent']

        time.sleep(0.212+0.024)
        t0 = time.time()
        background_stitch_agent.llm_background_stitching(scene, task)
        t1 = time.time()
        asset_color_and_type = asset_select_agent.llm_selecting_asset(scene, task)
        t2 = time.time()
        asset_view = view_change_agent.llm_3d_asset_view_change(scene, task)
        t3 = time.time()
        asset_pose_state = pose_measurement_agent.llm_pose_measurement(scene, task)
        t4 = time.time()
        asset_perspective_state = perspective_measurement_agent.llm_perspective_measurement(scene, task)
        t5 = time.time()
        
        print(f"{colored('[Task Management Agent] show operation done', color='magenta', attrs=['bold'])} \
            \n{colored(asset_color_and_type, attrs=['bold'])} \
            \n{colored(asset_view, attrs=['bold'])} \
            \n{colored(asset_pose_state, attrs=['bold'])} \
            \n{colored(asset_perspective_state, attrs=['bold'])}")
        
        return (t1-t0, t2-t1, t3-t2, t4-t3, t5-t4)

        # shown_car_name = scene.show_car({**asset_color_and_type, **asset_pose_state})
        # print(shown_car_name)
        
    def one_prompt(self, scene, user_prompt):
        """ decompose the prompt to the corresponding chatstitch.agents.
        Input:
            scene : Scene
                scene object.
            user_prompt : str
                language prompt to ChatSim.
        Return:
            tasks : dict
                a dictionary of decomposed tasks.
        """  
        q0 = "I have a requirement of operations in an autonomous driving scenario, and I need your help to break it down into one or several supportable actions. The scene is large which means many vehicles can be contained."

        q1 = "The supportable two actions include repeat the prompt ,\
                remove the occupied car,\
                show the occupied car."  
        
        q2 = "Please try to retain all the semantics and adjunct words from the original text. Each adding action should only contain one car. " + \
            "Information about adding vehicles (such as their type, positions, driving status, speed, color, etc.) should be directly included within the show occupied action." 
        
        q3 = "Split actions should be stored in a JSON dictonary. The key is action id and the value is specific action. They will be executed sequentially, and the broken operations should be independent with each other and do not rely on the detailed scene information."

        q4 = "An example: the requirement is 'substitute the red car in the scene'. you break it down and return" + \
                "{1: 'remove the red occupied car', 2: 'Add a new car at the location where the red car was deleted'}"
        
        q5 = "An example: the requirement is 'remove all cars', you break it down and return " + \
            "{ 1: 'remove all the occupied cars'} "
        
        q6 = "I may provide very abstract requirements. For such requirements, you should analyze how to comply with the splitting of actions." 

        q7 = "An example (very abstract): the requirement is 'I want show my partners in the scene', you analyse and return " + \
            "{ 1: 'Show one occupied car', 2 : 'Show one occupied car', 3 : 'Show one occupied car'} "
        
        q8 = "The scene is large enough to contain more than 20 vehicles. So many vehicles can be shown to the scene. Do not return any code or explanation; only a JSON dictionary is required."

        q9 = "Attention: the adjustments for one specific occupied vehicle should be included in one single output action. If there are multiple adjustments for one already occupied car, these adjustments must be merged in one action."
                
        q10 = "Attention: Do not appear information about the vehicles in the other broken actions."

        q11 = "Based on the action, and you will help me determine which operation this action belongs to."
        
        q12 = "Operations include (1) show (2) unshow."
        
        q13 = "Return the information in JSON format, with a key named 'operation'."

        q14 = "An Example: Given action 'Remove the red car from the scene', you should return {'operation': unshow}"

        q15 = "An Example: Given action 'see the car behind the building', you should return {'operation': show}"

        q16 = "Based on the operation statement to showe a occupied vehicle, and I need you to determine the car's color and type. "  

        q17 = "You need to return a JSON dictionary with 5 keys, including color, type, view, pose_status and pers_status."

        q18 = "(1) 'color', representing in RGB with range from 0 to 255. If the color is not mentioned, the value is just 'default'."

        q19 = "(2) 'type', one of [E300, audi, benz_g, benz_s, mini, cadillac, chevrolet, dodge, ferriari, lamborghini, rover, tank, police_car, porsche, tesla_cybertruck, tesla_roadster, cone, loader_truck, bulldozer, cement, excavator, sign_fence, random]. If the type is not mentioned or not in the type list, it defaults to random."

        q20 = "(3) 'view', representing the view of the vehicle. The value should be one of ['front', 'back', 'left', 'right', 'front-left', 'front-right', 'back-left', 'back-right']. If the view is not mentioned, the value defaults to 'back'."

        q21 = "(4) 'pose_status', representing should be shown or not. If the car should be shown, the value is '1'. If the car should not be shown, the value is '0'. If the status is not mentioned, the value defaults to '0'."

        q22 = "(5) 'pers_status', representing whether the vehicle should be boxed or not. If it should be boxed, the value is '1'. If it should not be boxed, the value is '0'. If the status is not mentioned, the value defaults to '0'."

        q23 = "An example: Given operation statement 'Show the right back side of the occupied white Audi behind the fence and box it.', you should return {'color':[255, 255, 255], 'type':'audi', 'view':'back-right', 'pose_status': '1', 'pers_status': '1'}"

        q24 = "An example: Given operation statement 'unshow one occupied car of E300 behind the building', you should return: {'color':[0,0,0], 'type':'E300', 'view':'back', 'pose_status':'0', 'pers_status':'0'}"
    
        q25 = "Note that you should not return any code or explanations, only provide a JSON dictionary."   

        q26 = "Put the dictionary of each operation into a list and return it as a decomposition of the entire requirement."

        q27 = "An example: Given the requrement'Show the right back side of the occupied white Audi behind the fence and box it, then unshow one occupied car of E300 behind the building.', you should return [{'color':[255, 255, 255], 'type':'audi', 'view':'back-right', 'pose_status': '1', 'pers_status': '1'}, {'color':[0,0,0], 'type':'E300', 'view':'back', 'pose_status':'0', 'pers_status':'0'} ]"

        q28 = "Note that you only need to output the final dictionary list at the end, without outputting intermediate results, code, comments, etc."

        q29 = "The entire requirement is "+ user_prompt

        prompt_list = [q0,q1,q2,q3,q4,q5,q6,q7,q8,q9,q10,q11,q12,q13,q14,q15,q16,q17, q18, q19, q20, q21, q22, q23, q24, q25, q26, q27, q28, q29]

        # client = openai.Openai()
        result = openai.chat.completions.create(
            model="gpt-4o",
            messages=[{"role": "system", "content": "You are an assistant helping me to break down the operations."}] + \
                    [{"role": "user", "content": q} for q in prompt_list]
        )
        answer = result.choices[0].message.content
        
        print(f"{colored('[User prompt]', color='magenta', attrs=['bold'])} {user_prompt}\n")
        print(f"{colored('[Task Managemet Agent] decomposing tasks', color='magenta', attrs=['bold'])} \
            \n{colored('[Raw Response>>>]', attrs=['bold'])} {answer}")

        try:
            start = answer.index("[")
            answer = answer[start:]
            end = answer.rfind("]")
            answer = answer[:end+1]
            tasks = eval(answer)
            print(f"{colored('[Extracted Response>>>]', attrs=['bold'])} {answer} \n")

        except Exception as e:
            print(e)
            traceback.print_exc()
            return "Can not parse the requirement."
        
        return tasks


        
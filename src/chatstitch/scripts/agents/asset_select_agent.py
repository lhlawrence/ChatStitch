import openai
from termcolor import colored
import traceback
import random

class AssetSelectAgent:
    def __init__(self, config):
        self.config = config
        self.asset_bank = {
            #需更新
            'E300' : "E300.in",
            'audi' : "Audi_Q3_2023.blend",
            'benz_g' : "Benz_G.blend",
            'benz_s' : "Benz_S.blend",
            'mini' : "BMW_mini.blend",
            'cadillac' : "Cadillac_CT6.blend",
            'chevrolet' : "Chevrolet.blend",
            'dodge' : "Dodge_SRT_Hellcat.blend",
            'ferriari' : "Ferriari_f150.blend",
            'lamborghini' : "Lamborghini.blend",
            'rover' : "Land_Rover_range_rover.blend",
            'tank' : "M1A2_tank.blend",
            'police_car' : "Police_car.blend",
            'porsche' : "Porsche-911-4s-final.blend",
            'tesla_cybertruck' : "Tesla_cybertruck.blend",
            'tesla_roadster' : "Tesla_roadster.blend",
            'loader_truck' : "obstacles/Loader_truck.blend",
            'bulldozer' : "obstacles/Bulldozer.blend",
            'cement' : "obstacles/Cement_isolation_pier.blend",
            'excavator' : "obstacles/Excavator.blend",
            'sign_fence' : "obstacles/Sign_fence.blend",
            'cone' : "obstacles/Traffic_cone.blend"
        }
        self.assets_dir = config['assets_dir']

    def llm_selecting_asset(self, scene, message):
        try:
            q0 = "I will provide you with an operation statement to show a occupied vehicle, and I need you to determine the car's color and type. "  

            q1 = "You need to return a JSON dictionary with 2 keys, including "

            q2 = "(1) 'color', representing in RGB with range from 0 to 255. If the color is not mentioned, the value is just 'default'."

            q3 = "(2) 'type', one of [E300, audi, benz_g, benz_s, mini, cadillac, chevrolet, dodge, ferriari, lamborghini, rover, tank, police_car, porsche, tesla_cybertruck, tesla_roadster, cone, loader_truck, bulldozer, cement, excavator, sign_fence, random]. If the type is not mentioned or not in the type list, it defaults to random."

            q4 = "An example: Given operation statement 'Show one occupied car of E300 behind the build', you should return: {'color':[0,0,0], 'type':'E300'}"

            q5 = "Note that you should not return any code or explanations, only provide a JSON dictionary."

            q6 = "The operation statement is:" + message

            prompt_list = [q0,q1,q2,q3,q4,q5,q6]

            result = openai.chat.completions.create(
            model="gpt-4o",
            messages=[{"role": "system", "content": "You are an assistant helping me to determine a car's color and type."}] + \
                    [{"role": "user", "content": q} for q in prompt_list]
            )
            answer = result.choices[0].message.content

            print(f"{colored('[Asset Agent LLM] deciding asset type and color', color='magenta', attrs=['bold'])} \
                    \n{colored('[Raw Response>>>]', attrs=['bold'])} {answer}")

            start = answer.index("{")
            answer = answer[start:]
            end = answer.rfind("}")
            answer = answer[:end+1]
            color_and_type = eval(answer)
            color_and_type['type'] = color_and_type['type'] if color_and_type['type'] != 'random' else random.choice(list(self.asset_bank.keys()))
            print(f"{colored('[Extracted Response>>>]', attrs=['bold'])} {color_and_type} \n")

        except Exception as e:
            print(e)
            traceback.print_exc()
            return "[Asset Agent LLM] deciding asset type and color fails."

        return color_and_type
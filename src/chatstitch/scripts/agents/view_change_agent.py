import openai
from termcolor import colored
import traceback
import random

class ViewChangeAgent:
    def __init__(self, config):
        self.config = config


    def llm_3d_asset_view_change(self, scene, message):
        try:
            q0 = "I will provide you with an operation statement to show a vehicle, and I need you to determine the view of the vehicle."

            q1 = "You need to return a JSON dictionary with 1 key 'view', representing the view of the vehicle. The value should be one of ['front', 'back', 'left', 'right', 'front-left', 'front-right', 'back-left', 'back-right']. If the view is not mentioned, the value defaults to 'back'."

            q2 = "An example: Given operation statement 'Show the E300 behind the building', you should return: {'view':'back'}"

            q3 = "An example: Given operation statement 'Show the back right view of the E300 behind the building', you should return: {'view':'back-right'}"

            q4 = "An example: Given operation statement 'Show the vehicle in front of us', you should return: {'view':'back'}"

            q5 = "Note that you should not return any code or explanations, only provide a JSON dictionary."

            q6 = "The operation statement is:" + message

            prompt_list = [q0, q1, q2, q3, q4, q5, q6]

            result = openai.chat.completions.create(
                model="gpt-4o",
                messages=[{"role": "system", "content": "You are an assistant helping me to determine a vehicle's view."}] + \
                        [{"role": "user", "content": q} for q in prompt_list]
            )
            answer = result.choices[0].message.content

            print(f"{colored('[3D Asset View Agent LLM] deciding asset view', color='magenta', attrs=['bold'])} \
                    \n{colored('[Raw Response>>>]', attrs=['bold'])} {answer}")
            start = answer.index("{")
            answer = answer[start:]
            end = answer.rfind("}")
            answer = answer[:end+1]
            view = eval(answer)
            print(f"{colored('[Extracted Response>>>]', attrs=['bold'])} {view} \n")

        except Exception as e:
            print(e)
            traceback.print_exc()
            return "[3D Asset View Agent LLM] deciding asset view fails."
        
        return view


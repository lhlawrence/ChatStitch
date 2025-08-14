import openai
from termcolor import colored
import traceback

class ForegroundRenderingAgent:
    def __init__(self, config):
        self.config = config

    def llm_foreground_rendering(self, scene, message):
        try:
            q0 = "I will provide you with an operation statement to show a vehicle, and I need you to determine the rendering of the vehicle."

            q1 = "You need to return a JSON dictionary with 1 key 'rendering', representing the rendering of the vehicle. The value should be one of ['normal', 'highlighted', 'shadowed']. If the rendering is not mentioned, the value defaults to 'normal'."

            q2 = "An example: Given operation statement 'Show the E300 behind the building', you should return: {'rendering':'normal'}"

            q3 = "An example: Given operation statement 'Highlight the E300 behind the building', you should return: {'rendering':'highlighted'}"

            q4 = "An example: Given operation statement 'Show the shadowed view of the E300 behind the building', you should return: {'rendering':'shadowed'}"

            q5 = "Note that you should not return any code or explanations, only provide a JSON dictionary."

            q6 = "The operation statement is:" + message

            prompt_list = [q0, q1, q2, q3, q4, q5, q6]

            result = openai.chat.completions.create(
                model="gpt-4o",
                messages=[{"role": "system", "content": "You are an assistant helping me to determine a vehicle's rendering."}] + \
                        [{"role": "user", "content": q} for q in prompt_list]
            )
            answer = result.choices[0].message.content

            print(f"{colored('[Foreground Rendering Agent LLM] deciding asset rendering', color='magenta', attrs=['bold'])} \
                    \n{colored('[Raw Response>>>]', attrs=['bold'])} {answer}")
            start = answer.index("{")
            answer = answer[start:]
            end = answer.rfind("}")
            answer = answer[:end+1]
            rendering = eval(answer)
            print(f"{colored('[Extracted Response>>>]', attrs=['bold'])} {rendering} \n")

        except Exception as e:
            print(e)
            traceback.print_exc()
            return "[Foreground Rendering Agent LLM] deciding asset rendering fails."
        
        return
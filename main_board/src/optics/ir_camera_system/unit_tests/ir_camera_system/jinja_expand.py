import sys
from jinja2 import Environment, FileSystemLoader

template_dir = sys.argv[1]
env = Environment(loader=FileSystemLoader(template_dir))
template = env.get_template(sys.argv[2])
output_file_name = sys.argv[3]

with open(output_file_name, mode="w") as fp:
    fp.write(template.render({"cameras": ["ir_eye_camera", "ir_face_camera", "2d_tof_camera"]}))

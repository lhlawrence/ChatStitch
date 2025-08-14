import yaml
import numpy as np
import os

def read_yaml(file_path):
    with open(file_path, 'r') as file:
        return yaml.safe_load(file)

def rotate(point, angle):
    """Rotates a point around the origin by the specified angle in radians."""
    rotation_matrix = np.array(
        [
            [np.cos(angle), -np.sin(angle), 0],
            [np.sin(angle), np.cos(angle), 0],
            [0, 0, 1],
        ]
    )
    return np.dot(rotation_matrix, point)

def generate_vertices(car):
    """Generates the vertices of a 3D box."""
    x = car["cx"]
    y = car["cy"]
    z = car["cz"]
    length = car["length"]
    width = car["width"]
    height = car["height"]
    heading = car["heading"]
    box_center = np.array([x, y, z])
    half_dims = np.array([length / 2, width / 2, height / 2])

    # The relative positions of the vertices from the box center before rotation.
    relative_positions = (
        np.array(
            [
                [-1, -1, -1],
                [-1, -1, 1],
                [-1, 1, -1],
                [-1, 1, 1],
                [1, -1, -1],
                [1, -1, 1],
                [1, 1, -1],
                [1, 1, 1],
            ]
        )
        * half_dims
    )

    # Rotate each relative position and add the box center position.
    vertices = np.asarray(
        [rotate(pos, heading) + box_center for pos in relative_positions]
    )
    return vertices

def check_and_mkdirs(path):
    if not os.path.exists(path):
        os.makedirs(path)
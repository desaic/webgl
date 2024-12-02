import bpy
import sys
from mathutils import Euler
track_name = 'trackUnitRigid'

def load_text_to_2d_float_list(filename):
  data = []
  with open(filename, 'r') as f:
    for line in f:
      # Remove leading/trailing whitespace from the line
      line = line.strip()
      # Split the line based on a delimiter (assuming whitespace)
      row_data = line.split()
      # Convert each value in the row to a float
      try:
        float_row = [float(val) for val in row_data]
      except ValueError:
        raise ValueError(f"Line '{line}' contains non-numeric values.")
      # Append the row of floats to the main data list
      data.append(float_row)
  return data

def make_track():
    baseObj = 0
    found = False
    for o in bpy.context.scene.objects:
        if(track_name == o.name):
            baseObj = o;
            found = True
    if(not found):
        return -1    
    print("found " + baseObj.name+'\n')
    duplicate = baseObj.copy()
    bpy.context.collection.objects.link(duplicate)
    places = load_text_to_2d_float_list('F:/meshes/tank/place_tracks.txt')
    print(len(places))

    for row in places:
      dup = baseObj.copy()
      bpy.context.collection.objects.link(dup)
      dup.location = (row[0],0.011,row[1])
      dup.rotation_euler = (0, -row[2],0)
    return 0
make_track();
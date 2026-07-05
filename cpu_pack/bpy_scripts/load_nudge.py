import bpy
import json
import math
from mathutils import Vector, Quaternion

JSON_FILE = "/media/desaic/WD/meshes/fruit_hand/out/nudge_debug_WhiteYellowGreen_60.json"
MESH_FILE = ""
MAX_CONTACTS = 64
FRAMES_PER_STEP = 3
START_FRAME = 1

CONTACT_ARROW_LENGTH = 1.0
LINVEL_SCALE = 0.02
ANGVEL_SCALE = 0.08
FORCE_SCALE = 0.005
MIN_ARROW_LEN = 0.05

CONTACT_COLOR = (1.0, 0.1, 0.1, 1.0)
LINVEL_COLOR = (0.1, 1.0, 0.2, 1.0)
ANGVEL_COLOR = (0.2, 0.4, 1.0, 1.0)
FORCE_COLOR = (1.0, 0.9, 0.1, 1.0)


def make_arrow_mesh(name, shaft_radius=0.04, shaft_len=0.7,
                    tip_radius=0.11, tip_len=0.3, n=12):
    existing = bpy.data.meshes.get(name)
    if existing is not None:
        return existing
    verts = []
    faces = []

    shaft_base = len(verts)
    for i in range(n):
        ang = 2.0 * math.pi * i / n
        x = shaft_radius * math.cos(ang)
        y = shaft_radius * math.sin(ang)
        verts.append((x, y, 0.0))
        verts.append((x, y, shaft_len))

    for i in range(n):
        a = shaft_base + 2 * i
        b = shaft_base + 2 * i + 1
        c = shaft_base + 2 * ((i + 1) % n) + 1
        d = shaft_base + 2 * ((i + 1) % n)
        faces.append((a, b, c, d))

    shaft_bottom_center = len(verts)
    verts.append((0.0, 0.0, 0.0))
    for i in range(n):
        a = shaft_base + 2 * i
        b = shaft_base + 2 * ((i + 1) % n)
        faces.append((shaft_bottom_center, b, a))

    tip_base = len(verts)
    for i in range(n):
        ang = 2.0 * math.pi * i / n
        x = tip_radius * math.cos(ang)
        y = tip_radius * math.sin(ang)
        verts.append((x, y, shaft_len))
    tip_apex = len(verts)
    verts.append((0.0, 0.0, shaft_len + tip_len))

    for i in range(n):
        a = tip_base + i
        b = tip_base + (i + 1) % n
        faces.append((a, b, tip_apex))

    for i in range(n):
        a = shaft_base + 2 * i + 1
        b = shaft_base + 2 * ((i + 1) % n) + 1
        c = tip_base + (i + 1) % n
        d = tip_base + i
        faces.append((a, b, c, d))

    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], faces)
    mesh.validate()
    mesh.update()
    return mesh


def make_material(name, color):
    existing = bpy.data.materials.get(name)
    if existing is not None:
        return existing
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    if bsdf is not None:
        bsdf.inputs["Base Color"].default_value = color
        bsdf.inputs["Roughness"].default_value = 0.4
    mat.diffuse_color = color
    mat.use_backface_culling = False
    return mat


def make_arrow_object(name, mesh, collection):
    obj = bpy.data.objects.get(name)
    if obj is None:
        obj = bpy.data.objects.new(name, mesh)
    if obj.name not in collection.objects:
        collection.objects.link(obj)
    obj.rotation_mode = 'QUATERNION'
    obj.hide_viewport = True
    obj.hide_render = True
    return obj


def make_arrow_pool(prefix, mesh, count, collection):
    return [make_arrow_object(f"{prefix}_{i:02d}", mesh, collection)
            for i in range(count)]


def vec_to_quat(direction):
    z_axis = Vector((0.0, 0.0, 1.0))
    d = Vector(direction)
    if d.length < 1e-9:
        return Quaternion((1.0, 0.0, 0.0, 0.0))
    d.normalize()
    return z_axis.rotation_difference(d)


def key_arrow(obj, frame):
    obj.keyframe_insert(data_path="hide_viewport", frame=frame)
    obj.keyframe_insert(data_path="hide_render", frame=frame)
    obj.keyframe_insert(data_path="location", frame=frame)
    obj.keyframe_insert(data_path="rotation_quaternion", frame=frame)
    obj.keyframe_insert(data_path="scale", frame=frame)


def set_arrow(obj, origin, direction, length, visible):
    obj.hide_viewport = not visible
    obj.hide_render = not visible
    if visible:
        obj.location = origin
        obj.rotation_quaternion = vec_to_quat(direction)
        obj.scale = (1.0, 1.0, max(length, MIN_ARROW_LEN))


def clear_animation(obj):
    if obj.animation_data is not None:
        obj.animation_data_clear()


def get_body_object(collection):
    active = bpy.context.active_object
    if active is not None and active.type == 'MESH':
        return active

    if MESH_FILE:
        path = bpy.path.abspath(MESH_FILE) if MESH_FILE.startswith("//") \
            else MESH_FILE
        bpy.ops.wm.obj_import(filepath=path)
        obj = bpy.context.selected_objects[0]
        if obj.name not in collection.objects:
            collection.objects.link(obj)
        return obj

    raise RuntimeError("No mesh object selected. Select the body or set MESH_FILE.")


def animate(data, body_obj, collection):
    contact_mesh = make_arrow_mesh("NudgeContactArrow")
    contact_mesh.materials.append(make_material("NudgeContactMat", CONTACT_COLOR))
    linvel_mesh = make_arrow_mesh("NudgeLinVelArrow")
    linvel_mesh.materials.append(make_material("NudgeLinVelMat", LINVEL_COLOR))
    angvel_mesh = make_arrow_mesh("NudgeAngVelArrow")
    angvel_mesh.materials.append(make_material("NudgeAngVelMat", ANGVEL_COLOR))
    force_mesh = make_arrow_mesh("NudgeForceArrow")
    force_mesh.materials.append(make_material("NudgeForceMat", FORCE_COLOR))

    contact_pool = make_arrow_pool("NudgeContact", contact_mesh,
                                   MAX_CONTACTS, collection)
    linvel_arrow = make_arrow_object("NudgeLinVel", linvel_mesh, collection)
    angvel_arrow = make_arrow_object("NudgeAngVel", angvel_mesh, collection)
    force_arrow = make_arrow_object("NudgeForce", force_mesh, collection)

    all_arrows = list(contact_pool) + [linvel_arrow, angvel_arrow, force_arrow]
    for obj in all_arrows:
        clear_animation(obj)

    body_obj.rotation_mode = 'QUATERNION'
    clear_animation(body_obj)

    steps = data.get("steps", [])
    frame = START_FRAME

    for step in steps:
        pos = Vector(step["position"])
        rot = Quaternion((step["rotation"][0], step["rotation"][1],
                          step["rotation"][2], step["rotation"][3]))

        body_obj.location = pos
        body_obj.rotation_quaternion = rot
        body_obj.keyframe_insert(data_path="location", frame=frame)
        body_obj.keyframe_insert(data_path="rotation_quaternion", frame=frame)

        lv = Vector(step["linearVelocity"])
        set_arrow(linvel_arrow, pos, lv, lv.length * LINVEL_SCALE, lv.length > 1e-9)
        key_arrow(linvel_arrow, frame)

        av = Vector(step["angularVelocity"])
        set_arrow(angvel_arrow, pos, av, av.length * ANGVEL_SCALE, av.length > 1e-9)
        key_arrow(angvel_arrow, frame)

        ef = Vector(step["externalForce"])
        set_arrow(force_arrow, pos, ef, ef.length * FORCE_SCALE, ef.length > 1e-9)
        key_arrow(force_arrow, frame)

        contacts = step.get("contacts", [])[:MAX_CONTACTS]
        for i in range(MAX_CONTACTS):
            arrow = contact_pool[i]
            if i < len(contacts):
                c = contacts[i]
                cpos = Vector(c["worldPos"])
                nrm = Vector(c["normal"])
                set_arrow(arrow, cpos, nrm, CONTACT_ARROW_LENGTH, True)
            else:
                set_arrow(arrow, pos, (0.0, 0.0, 1.0), CONTACT_ARROW_LENGTH, False)
            key_arrow(arrow, frame)

        frame += FRAMES_PER_STEP

    bpy.context.scene.frame_start = START_FRAME
    bpy.context.scene.frame_end = max(START_FRAME, frame - FRAMES_PER_STEP)
    bpy.context.scene.frame_set(START_FRAME)


def main():
    path = bpy.path.abspath(JSON_FILE) if JSON_FILE.startswith("//") else JSON_FILE
    with open(path, "r") as f:
        data = json.load(f)

    collection = bpy.context.collection
    body_obj = get_body_object(collection)
    print(f"Visualizing nudge debug on '{body_obj.name}' "
          f"with {len(data.get('steps', []))} steps, "
          f"MAX_CONTACTS={MAX_CONTACTS}")

    animate(data, body_obj, collection)
    print("Nudge debug animation complete")


if __name__ == "__main__":
    main()

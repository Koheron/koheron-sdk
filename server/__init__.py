# (c) Koheron

import os, re
import CppHeaderParser
import jinja2

SDK_PATH = os.getenv("SDK_PATH", "")

# ---------------- helpers & typing utilities ----

FORBIDDEN_INTS = [
    "short","int","unsigned","long","unsigned short","short unsigned",
    "unsigned int","int unsigned","unsigned long","long unsigned",
    "long long","unsigned long long","long long unsigned"
]

def is_std_array(t: str) -> bool:
    return t.split("<")[0].strip() in ["std::array", "const std::array"]

def check_type(_type, driver_name, opname):
    if _type in FORBIDDEN_INTS:
        raise ValueError(f'[{driver_name}::{opname}] Invalid type "{_type}": '
                         'Use fixed-width integers (e.g. uint32_t).')

def exact_ret_type(classname, operation):
    ret = operation["ret_type"]
    if "auto" in ret or is_std_array(ret):
        decl_args = ", ".join(
            f"std::declval<{_full_arg_type(a)}>()"
            for a in operation.get("arguments", [])
        )
        return f"decltype(std::declval<{classname}>().{operation['name']}({decl_args}))"
    return ret

# ------------------------- Jinja env ------------------------

_ENV = None

def _build_jinja_env():
    env = jinja2.Environment(
        block_start_string="{%",
        block_end_string="%}",
        variable_start_string="{{",
        variable_end_string="}}",
        trim_blocks=True,
        lstrip_blocks=True,
        loader=jinja2.FileSystemLoader(os.path.join(SDK_PATH, "server/templates")),
    )

    env.filters["exact_ret_type"] = lambda operation, classname: exact_ret_type(classname, operation)
    env.filters["get_exact_ret_type"] = exact_ret_type
    env.globals["get_exact_ret_type"] = exact_ret_type
    return env

def get_template(filename):
    global _ENV
    if _ENV is None:
        _ENV = _build_jinja_env()
    return _ENV.get_template(filename)

# ------------------------- Parsing -------------------

def parse_header(hppfile):
    cpp_header = CppHeaderParser.CppHeader(hppfile)
    return [parse_driver_header(cls, hppfile) for cls in cpp_header.classes.values()]

def parse_driver_header(_class, hppfile):
    driver = {
        "name": _class["name"],
        "tag": "_".join(re.findall("[A-Z][^A-Z]*", _class["name"])).upper(),
        "includes": [hppfile],
        "objects": [{"type": str(_class["name"]), "name": "__" + _class["name"]}],
        "operations": [],
        "_class": _class,
    }
    op_id = 0
    for method in _class["methods"]["public"]:
        if method["name"] in [_class["name"], "~" + _class["name"]] or method.get("template"):
            continue
        op = parse_header_operation(driver["name"], method)
        op["id"] = op_id
        driver["operations"].append(op)
        op_id += 1
    mark_ops_needing_cast(_class, driver)
    augment_ops(driver)
    return driver

def parse_header_operation(driver_name, method):
    op = {"tag": method["name"].upper(), "name": method["name"], "ret_type": method["rtnType"]}
    check_type(op["ret_type"], driver_name, op["name"])
    params = method.get("parameters") or []
    if params:
        op["arguments"] = []
        for p in params:
            arg_type = p["type"].strip()
            by_ref = arg_type.endswith("&")
            is_const = arg_type.startswith("const ")

            if by_ref:
                arg_type = arg_type[:-1].strip()
            if is_const:
                arg_type = arg_type[len("const "):].strip()

            check_type(arg_type, driver_name, op["name"])
            arg = {"name": str(p["name"]), "type": arg_type}

            if by_ref:
                arg["by_reference"] = True

            if is_const:
                arg["is_const"] = True

            op["arguments"].append(arg)
    return op

def mark_ops_needing_cast(_class, driver_dict):
    templated_names = {m["name"] for m in _class["methods"]["public"] if m.get("template", False)}
    counts = {}
    for op in driver_dict["operations"]:
        counts[op["name"]] = counts.get(op["name"], 0) + 1
    for op in driver_dict["operations"]:
        op["needs_cast"] = op["name"] in templated_names or counts[op["name"]] > 1

def _full_arg_type(a):
    t = a['type'].strip()
    if a.get('is_const'):
        t = f'const {t}'
    if a.get('by_reference'):
        t = f'{t}&'
    return t

def augment_ops(driver_dict):
    classname = driver_dict['name']
    for op in driver_dict['operations']:
        op['arg_types_str'] = ', '.join(_full_arg_type(a) for a in op.get('arguments', []))
        op['ret_expr'] = exact_ret_type(classname, op)

# ------------------------- Driver wrappers & public fns ----------------------

class Driver:
    def __init__(self, path, base_dir="."):
        dev = parse_header(os.path.join(base_dir, path))[0]
        self.operations = dev["operations"]
        self.tag = dev["tag"]
        self.name = dev["name"]
        self.class_name = "InTerface" + self.tag.capitalize()
        self.objects = dev["objects"]
        self.includes = dev["includes"]
        self.interface_name = "interface_" + os.path.basename(self.includes[0]).split(".")[0]
        self.id = None

def get_driver(path, driver_id=0):
    d = Driver(path)
    d.id = driver_id
    return d

def get_drivers(drivers_list):
    drivers = []
    for driver_id, path in enumerate(drivers_list or [], 2):
        assert path.endswith((".hpp", ".h"))
        drivers.append(get_driver(path, driver_id))
    return drivers

def get_driver_id(drivers_list, driver_path):
    for driver_id, path in enumerate(drivers_list or [], 2):
        assert path.endswith((".hpp", ".h"))
        if os.path.split(path)[1] == os.path.split(driver_path)[1]:
            return driver_id
    return None

# ------------------------- Rendering entry points ---------------------------

def render_template(template_filename, output_filename, drivers):
    with open(output_filename, "w") as out:
        out.write(get_template(os.path.basename(template_filename))
           .render(drivers=drivers))

def render_driver(driver, output_filename_hpp):
    base, ext = os.path.splitext(output_filename_hpp)
    assert ext in [".hpp", ".cpp"]
    with open(base + ext, "w") as out:
        out.write(get_template("interface_driver" + ext).render(driver=driver))

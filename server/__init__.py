# (c) Koheron

import os, re, json
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

def is_std_vector(t: str) -> bool:
    return t.split("<")[0].strip() in ["std::vector", "const std::vector"]

def is_std_string(t: str) -> bool:
    return t.strip() in ["std::string", "const std::string"]

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

def format_type(_type):
    if is_std_array(_type):
        templates = _type.split('<')[1].split('>')[0].split(',')
        return f'std::array<{templates[0]}, " << {templates[1]} << ">'
    else:
        return _type

def format_ret_type(classname, operation):
    if "auto" in operation["ret_type"] or is_std_array(operation["ret_type"]):
        return '" << get_type_str<{}>() << "'.format(exact_ret_type(classname, operation))

    return operation["ret_type"]

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
    env.filters["get_exact_ret_type"] = lambda classname, operation: exact_ret_type(classname, operation)
    env.globals["get_exact_ret_type"] = exact_ret_type
    return env

def _env():
    global _ENV
    if _ENV is None:
        _ENV = _build_jinja_env()
    return _ENV

def get_template(filename):
    return _env().get_template(filename)

# ------------------------- Parsing -------------------

def parse_header(hppfile):
    cpp_header = CppHeaderParser.CppHeader(hppfile)
    drivers = []
    for _, cls in cpp_header.classes.items():
        drivers.append(parse_driver_header(cls, hppfile))
    return drivers

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
        if (method["name"] in ["" + _class["name"], "~" + _class["name"]]) or method.get("template"):
            continue
        op = parse_header_operation(driver["name"], method)
        op["id"] = op_id
        driver["operations"].append(op)
        op_id += 1
    mark_overloads(driver)
    mark_ops_needing_cast(_class, driver)
    augment_ops(driver)
    return driver

def parse_header_operation(driver_name, method):
    op = {"tag": method["name"].upper(), "name": method["name"], "ret_type": method["rtnType"]}
    check_type(op["ret_type"], driver_name, op["name"])
    params = method.get("parameters") or []
    if params:
        op["arguments"], op["args_client"] = [], []
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
            op["args_client"].append({"name": arg["name"], "type": format_type(arg["type"])})
    return op

def mark_overloads(driver_dict):
    counts = {}
    for op in driver_dict["operations"]:
        counts[op["name"]] = counts.get(op["name"], 0) + 1
    for op in driver_dict["operations"]:
        op["is_overloaded"] = counts[op["name"]] > 1

def mark_ops_needing_cast(_class, driver_dict):
    templated_names = {m["name"] for m in _class["methods"]["public"] if m.get("template", False)}
    counts = {}
    for op in driver_dict["operations"]:
        counts[op["name"]] = counts.get(op["name"], 0) + 1
    overloaded_names = {n for n, c in counts.items() if c > 1}
    for op in driver_dict["operations"]:
        op["needs_cast"] = (op["name"] in templated_names) or (op["name"] in overloaded_names)

def _full_arg_type(a):
    t = a['type'].strip()
    if a.get('is_const'):
        t = f'const {t}'
    if a.get('by_reference'):
        t = f'{t}&'
    return t

def build_arg_types_str(op):
    args = op.get('arguments', [])
    return ', '.join(_full_arg_type(a) for a in args) if args else ''

def build_ret_expr(classname, operation):
    rt = operation['ret_type']
    if 'auto' in rt or is_std_array(rt):
        decl_args = []
        for a in operation.get('arguments', []):
            decl_args.append(f'std::declval<{_full_arg_type(a)}>()')
        call_args = ', '.join(decl_args)
        return f'decltype(std::declval<{classname}>().{operation["name"]}({call_args}))'
    return rt

def augment_ops(driver_dict):
    classname = driver_dict['name']
    for op in driver_dict['operations']:
        op['arg_types_str'] = ', '.join(_full_arg_type(a) for a in op.get('arguments', []))
        op['ret_expr'] = exact_ret_type(classname, op)

# ------------------------- Driver wrappers & public fns ----------------------

class Driver:
    def __init__(self, path, base_dir="."):
        dev = parse_header(os.path.join(base_dir, path))[0]
        self.raw = dev
        self.header_path = os.path.dirname(path)
        self.path = path
        self.operations = dev["operations"]
        self.tag = dev["tag"]
        self.name = dev["name"]
        self.class_name = "InTerface" + self.tag.capitalize()
        self.objects = dev["objects"]
        self.includes = dev["includes"]
        self.interface_name = "interface_" + os.path.basename(self.includes[0]).split(".")[0]
        self.id = None
        self.calls = None

def get_driver(path, driver_id=0):
    d = Driver(path)
    d.id = driver_id
    return d

def get_drivers(drivers_list):
    drivers, driver_id = [], 2
    for path in drivers_list or []:
        assert path.endswith((".hpp", ".h"))
        drivers.append(get_driver(path, driver_id))
        driver_id += 1
    return drivers

def get_driver_id(drivers_list, driver_path):
    driver_id = 2
    for path in drivers_list or []:
        assert path.endswith((".hpp", ".h"))
        if os.path.split(path)[1] == os.path.split(driver_path)[1]:
            return driver_id
        driver_id += 1
    return None

def get_json(drivers):
    data = [{
        "class": "KServer",
        "id": 1,
        "functions": [
            {"name": "get_version", "id": 0, "args": [], "ret_type": "const char *"},
            {"name": "get_cmds", "id": 1, "args": [], "ret_type": "std::string"},
        ],
    }]
    for driver in drivers:
        data.append({
            "class": driver.name,
            "id": driver.id,
            "functions": [
                {
                    "name": op["name"],
                    "id": op["id"],
                    "ret_type": format_ret_type(driver.name, op),
                    "args": op.get("args_client", []),
                }
                for op in driver.operations
            ],
        })

    return json.dumps(data, separators=(',', ':')).replace('"', '\\"').replace('\\\\','')

# ------------------------- Rendering entry points ---------------------------

def render_template(template_filename, output_filename, drivers):
    with open(output_filename, "w") as out:
        out.write(get_template(os.path.basename(template_filename))
           .render(drivers=drivers, json=get_json(drivers)))

def render_driver(driver, output_filename_hpp):
    base, ext = os.path.splitext(output_filename_hpp)
    assert ext in [".hpp", ".cpp"]
    with open(base + ext, "w") as out:
        out.write(get_template("interface_driver" + ext).render(driver=driver))

"""Small KiCad s-expression helpers: parse, write, and copy symbols out of
KiCad's own symbol libraries (flattening derived symbols) for embedding in a
generated schematic."""
import copy
import os
import re

KICAD_SYMBOLS = os.environ.get("KICAD_SYMBOLS", "/Applications/KiCad/KiCad.app/Contents/SharedSupport/symbols")
KICAD_FOOTPRINTS = os.environ.get("KICAD_FOOTPRINTS", "/Applications/KiCad/KiCad.app/Contents/SharedSupport/footprints")

TOK = re.compile(r'\(|\)|"(?:[^"\\]|\\.)*"|[^\s()"]+')


class Q(str):
    """A quoted string."""


def parse(text):
    stack = [[]]
    for m in TOK.finditer(text):
        t = m.group()
        if t == "(":
            stack.append([])
        elif t == ")":
            x = stack.pop()
            stack[-1].append(x)
        elif t[0] == '"':
            stack[-1].append(Q(re.sub(r"\\(.)", r"\1", t[1:-1])))
        else:
            stack[-1].append(t)
    return stack[0][0]


def atom(a):
    if isinstance(a, Q):
        return '"' + a.replace("\\", "\\\\").replace('"', '\\"') + '"'
    return str(a)


def dump(node, ind=0):
    if not any(isinstance(c, list) for c in node):
        return "\t" * ind + "(" + " ".join(atom(a) for a in node) + ")"
    out = ["\t" * ind + "(" + " ".join(atom(a) for a in node if not isinstance(a, list))]
    for c in node:
        if isinstance(c, list):
            out.append(dump(c, ind + 1))
    out.append("\t" * ind + ")")
    return "\n".join(out)


def kids(node, name):
    return [c for c in node if isinstance(c, list) and c and c[0] == name]


def kid(node, name):
    k = kids(node, name)
    return k[0] if k else None


_libs = {}


def _lib(name):
    if name not in _libs:
        _libs[name] = parse(open(os.path.join(KICAD_SYMBOLS, name + ".kicad_sym")).read())
    return _libs[name]


def lib_symbol(lib_id):
    """The symbol "Lib:Name" from KiCad's libraries, flattened and named for a schematic's lib_symbols."""
    lib, name = lib_id.split(":")
    syms = {s[1]: s for s in kids(_lib(lib), "symbol")}
    s = copy.deepcopy(syms[name])
    ext = kid(s, "extends")
    if ext:
        base = lib_symbol(f"{lib}:{ext[1]}")
        props = kids(s, "property")
        base_name = ext[1]
        out = [x for x in base if not (isinstance(x, list) and x[0] == "property")]
        # the derived symbol's own properties replace the parent's
        names = {p[1] for p in props}
        out[2:2] = props + [p for p in kids(base, "property") if p[1] not in names]
        for sub in kids(out, "symbol"):
            sub[1] = Q(sub[1].replace(base_name.split(":")[-1], name, 1))
        s = out
    s[1] = Q(lib_id)
    return s


def pins(sym):
    """{number: (x, y, angle, length, name, type, hidden)} in library coordinates (y up), unit 1."""
    out = {}

    def walk(n):
        for c in n:
            if isinstance(c, list) and c:
                if c[0] == "pin":
                    at = kid(c, "at")
                    out[kid(c, "number")[1]] = (float(at[1]), float(at[2]), float(at[3]) if len(at) > 3 else 0.0,
                                               float(kid(c, "length")[1]), kid(c, "name")[1], c[1], "hide" in c or
                                               any(isinstance(x, list) and x[:2] == ["hide", "yes"] for x in c))
                elif c[0] == "symbol":
                    m = re.search(r"_(\d+)_(\d+)$", c[1])
                    if m and m.group(1) not in ("0", "1"):
                        continue  # other units
                    walk(c)
    walk(sym)
    return out

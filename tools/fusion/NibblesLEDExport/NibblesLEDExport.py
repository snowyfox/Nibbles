"""Nibbles geometry export for Fusion 360.

Exports what the lighting simulator needs to place the shark's LEDs, all in
millimetres in the design's world coordinates:
  tools/fusion/output/tubes.txt   readable list of the diffuser tubes
  tools/fusion/output/geometry.json
      tubes: centre line of every LED diffuser tube, traced from the tube
             bodies (works on pipes, sweeps, imported solids and mirrored
             copies, from any design context); tube bodies are the bodies of
             components whose name contains one of TUBE_COMPONENT_WORDS
      cylinders: every cylindrical face of radius >= CYLINDER_MIN_MM (the
             pole, holes such as the eye opening)
      bodies: every body's bounding box
  tools/fusion/output/shark.obj   a light mesh of the whole design
The LED layout itself is built on the computer from geometry.json by
tools/sim/make_layout.py.
"""
import json
import math
import os
import traceback

import adsk.core
import adsk.fusion

HERE = os.path.dirname(os.path.abspath(__file__))
REPO_TOOLS = os.path.dirname(os.path.dirname(HERE))  # tools/
OUT_DIR = os.path.join(REPO_TOOLS, "fusion", "output")

TUBE_COMPONENT_WORDS = ["tubing"]  # e.g. "Silicone Tubing", "Fin Tubing" (not the "15mm CF Tube" pole)
TUBE_MIN_RADIUS_MM = 3.0           # thinner "tubes" are placeholders, not diffuser tubing
CYLINDER_MIN_MM = 5.0
ALONG_SAMPLES = 80   # centre-line points per face, along the tube
AROUND_SAMPLES = 8   # points around the tube averaged into each centre-line point


def mm(p):
    return [p.x * 10.0, p.y * 10.0, p.z * 10.0]  # Fusion works in cm


def contexts(root):
    """(component, transform to world, label) for the root and every occurrence."""
    yield root, adsk.core.Matrix3D.create(), root.name
    for occ in root.allOccurrences:
        yield occ.component, occ.transform2, occ.fullPathName


def dist(a, b):
    return math.sqrt(sum((a[i] - b[i]) ** 2 for i in range(3)))


def polyline_length(line):
    return sum(dist(line[i], line[i + 1]) for i in range(len(line) - 1))


def face_centre_line(face):
    """Centre line of one side face of a tube: (points in mm, radius in mm),
    or None for faces that aren't part of a tube's side (end caps etc.)."""
    geo = face.geometry
    st = geo.surfaceType
    if st == adsk.core.SurfaceTypes.PlaneSurfaceType:
        return None
    ev = face.evaluator
    rng = ev.parametricRange()  # a BoundingBox2D (not an (ok, value) pair)
    u0, v0, u1, v1 = rng.minPoint.x, rng.minPoint.y, rng.maxPoint.x, rng.maxPoint.y

    def centre(u, v):
        """Step inward from the surface point by the tube radius."""
        prm = adsk.core.Point2D.create(u, v)
        ok, p = ev.getPointAtParameter(prm)
        ok2, n = ev.getNormalAtParameter(prm)
        if not (ok and ok2):
            return None
        if st == adsk.core.SurfaceTypes.CylinderSurfaceType:
            r = geo.radius
        elif st == adsk.core.SurfaceTypes.TorusSurfaceType:
            r = geo.minorRadius
        else:
            ok, _, kmax, kmin = ev.getCurvature(prm)
            k = max(abs(kmax), abs(kmin)) if ok else 0.0
            if k < 1e-6:
                return None
            r = 1.0 / k
        return [(p.x - n.x * r) * 10.0, (p.y - n.y * r) * 10.0, (p.z - n.z * r) * 10.0], r * 10.0

    def grid(n_u, n_v):
        return [[centre(u0 + (u1 - u0) * i / (n_u - 1), v0 + (v1 - v0) * j / (n_v - 1)) for j in range(n_v)]
                for i in range(n_u)]

    def spread(rows):
        """How far apart the centres in each row are (small = the row goes round the tube)."""
        worst = 0.0
        for row in rows:
            pts = [c[0] for c in row if c]
            for a in pts:
                for b in pts:
                    worst = max(worst, dist(a, b))
        return worst

    # Which parameter runs round the tube? Its lines of centres collapse to a point.
    test = grid(6, 6)
    by_u = spread(test)                                       # rows: u fixed, v varies
    by_v = spread([[test[i][j] for i in range(6)] for j in range(6)])  # u varies
    around_is_v = by_u <= by_v
    radii = [c[1] for row in test for c in row if c]
    if not radii:
        return None
    radius = sorted(radii)[len(radii) // 2]
    if min(by_u, by_v) > radius * 0.6:
        return None  # centres don't collapse either way: not a tube side
    line = []
    for i in range(ALONG_SAMPLES):
        t = i / (ALONG_SAMPLES - 1)
        pts = []
        for j in range(AROUND_SAMPLES):
            w = j / AROUND_SAMPLES
            c = centre(u0 + (u1 - u0) * t, v0 + (v1 - v0) * w) if around_is_v else \
                centre(u0 + (u1 - u0) * w, v0 + (v1 - v0) * t)
            if c:
                pts.append(c[0])
        if pts:
            line.append([sum(p[k] for p in pts) / len(pts) for k in range(3)])
    return (line, radius) if len(line) > 1 else None


def join_lines(pieces, gap_mm):
    """Chain centre-line pieces end to end into as few lines as possible."""
    pieces = sorted(pieces, key=polyline_length, reverse=True)
    lines = []
    while pieces:
        line = list(pieces.pop(0))
        grown = True
        while grown and pieces:
            grown = False
            best = None
            for idx, pc in enumerate(pieces):
                for at_end in (True, False):
                    tip = line[-1] if at_end else line[0]
                    for rev in (False, True):
                        cand = list(reversed(pc)) if rev else pc
                        d = dist(tip, cand[0] if at_end else cand[-1])
                        if best is None or d < best[0]:
                            best = (d, idx, at_end, cand)
            if best and best[0] <= gap_mm:
                d, idx, at_end, cand = best
                pieces.pop(idx)
                line = line + cand[1:] if at_end else cand[:-1] + line
                grown = True
        lines.append(line)
    return lines


def find_tubes(design):
    tubes = []
    for occ in design.rootComponent.allOccurrences:
        if not any(w in occ.component.name.lower() for w in TUBE_COMPONENT_WORDS):
            continue
        for body in occ.bRepBodies:  # proxies: geometry in world coordinates
            key = "%s / %s" % (occ.fullPathName, body.name)
            try:
                faces = []
                for face in body.faces:
                    r = face_centre_line(face)
                    if r:
                        faces.append(r)
                if not faces:
                    raise ValueError("no tube-shaped faces")
                outer = max(r for _, r in faces)
                if outer < TUBE_MIN_RADIUS_MM:
                    continue
                pieces = [ln for ln, r in faces if r >= outer * 0.8]  # outer wall only (skip a hollow tube's bore)
                # Tight bends can leave a short untraced gap between faces: join across up to 3 tube radii.
                for n, line in enumerate(join_lines(pieces, gap_mm=max(5.0, outer * 3.0))):
                    tubes.append({
                        "id": "T%d" % (len(tubes) + 1),
                        "key": key + ("" if n == 0 else " #%d" % (n + 1)),
                        "component": occ.fullPathName,
                        "bodies": [body.name],
                        "radius_mm": round(outer, 2),
                        "length_mm": round(polyline_length(line), 1),
                        "start": [round(v, 1) for v in line[0]],
                        "end": [round(v, 1) for v in line[-1]],
                        "line": line,
                    })
            except Exception:
                tubes.append({"id": "T%d" % (len(tubes) + 1), "key": key,
                              "error": traceback.format_exc().splitlines()[-1]})
    return tubes


FEATURE_KINDS = ["sweepFeatures", "pipeFeatures", "extrudeFeatures", "revolveFeatures", "loftFeatures",
                 "coilFeatures", "baseFeatures", "combineFeatures", "mirrorFeatures", "rectangularPatternFeatures",
                 "circularPatternFeatures", "pathPatternFeatures", "copyPasteBodies"]


def diagnostics(design):
    """How the design is built, for when no tubes are found."""
    kind = "parametric (timeline)" if design.designType == adsk.fusion.DesignTypes.ParametricDesignType else "direct (no timeline)"
    lines = ["Design: %s, %s" % (design.rootComponent.name, kind)]
    for comp, matrix, label in contexts(design.rootComponent):
        counts = []
        for k in FEATURE_KINDS:
            try:
                n = getattr(comp.features, k).count
            except Exception:
                n = None
            if n:
                counts.append("%s %d" % (k.replace("Features", ""), n))
        bodies = [b.name for b in comp.bRepBodies]
        sketches = comp.sketches.count
        lines.append("  %s: %s | sketches %d | bodies %d%s" % (
            label, ", ".join(counts) or "no features", sketches, len(bodies),
            (": " + ", ".join(bodies[:40]) + (" ..." if len(bodies) > 40 else "")) if bodies else ""))
    return lines


def thin(line, step_mm):
    out, run = [line[0]], 0.0
    for i in range(1, len(line)):
        run += dist(line[i - 1], line[i])
        if run >= step_mm or i == len(line) - 1:
            out.append([round(v, 1) for v in line[i]])
            run = 0.0
    return out


def find_cylinders(design):
    """Cylindrical faces of radius >= CYLINDER_MIN_MM: centre, axis, radius and span along the axis."""
    out = []
    for occ in design.rootComponent.allOccurrences:
        for body in occ.bRepBodies:
            for face in body.faces:
                geo = face.geometry
                if geo.surfaceType != adsk.core.SurfaceTypes.CylinderSurfaceType or geo.radius * 10.0 < CYLINDER_MIN_MM:
                    continue
                o, ax = geo.origin, geo.axis
                ax.normalize()
                # span of the face along the axis, from its bounding box corners
                bb = face.boundingBox
                ts = []
                for cx in (bb.minPoint.x, bb.maxPoint.x):
                    for cy in (bb.minPoint.y, bb.maxPoint.y):
                        for cz in (bb.minPoint.z, bb.maxPoint.z):
                            ts.append((cx - o.x) * ax.x + (cy - o.y) * ax.y + (cz - o.z) * ax.z)
                t0, t1 = min(ts), max(ts)
                out.append({
                    "component": occ.fullPathName, "body": body.name,
                    "radius_mm": round(geo.radius * 10.0, 2),
                    "axis": [round(ax.x, 4), round(ax.y, 4), round(ax.z, 4)],
                    "from": [round((o.x + ax.x * t0) * 10.0, 1), round((o.y + ax.y * t0) * 10.0, 1), round((o.z + ax.z * t0) * 10.0, 1)],
                    "to": [round((o.x + ax.x * t1) * 10.0, 1), round((o.y + ax.y * t1) * 10.0, 1), round((o.z + ax.z * t1) * 10.0, 1)],
                    "area_cm2": round(face.area, 2),
                })
    return out


def body_boxes(design):
    out = []
    for occ in design.rootComponent.allOccurrences:
        for body in occ.bRepBodies:
            bb = body.boundingBox
            out.append({"component": occ.fullPathName, "body": body.name,
                        "min": mm(bb.minPoint), "max": mm(bb.maxPoint)})
    return out


def write_outputs(tubes, cylinders, boxes, up, diag):
    with open(os.path.join(OUT_DIR, "tubes.txt"), "w") as f:
        f.write("Diffuser tubes (centre lines traced from the tube bodies), in mm; model up axis: %s\n\n" % up)
        for t in tubes:
            if "error" in t:
                f.write("%-4s %s  ERROR %s\n" % (t["id"], t["key"], t["error"]))
                continue
            f.write("%-4s %-55s length %7.1f  radius %5.2f\n     start %s  end %s\n"
                    % (t["id"], t["key"], t["length_mm"], t["radius_mm"], t["start"], t["end"]))
        f.write("\nCylindrical faces (radius >= %.0f mm): %d, see geometry.json\n" % (CYLINDER_MIN_MM, len(cylinders)))
        f.write("\n" + "\n".join(diag) + "\n")
    with open(os.path.join(OUT_DIR, "geometry.json"), "w") as f:
        json.dump({"units": "mm", "up": up,
                   "tubes": [dict(t, line=thin(t["line"], 2.0)) if "line" in t else t for t in tubes],
                   "cylinders": cylinders, "bodies": boxes}, f)


def export_mesh(app, design):
    em = design.exportManager
    opts = em.createOBJExportOptions(design.rootComponent, os.path.join(OUT_DIR, "shark.obj"))
    opts.meshRefinement = adsk.fusion.MeshRefinementSettings.MeshRefinementLow
    em.execute(opts)


def run(context):
    app = adsk.core.Application.get()
    ui = app.userInterface
    try:
        design = adsk.fusion.Design.cast(app.activeProduct)
        if not design:
            ui.messageBox("Open the shark design (Design workspace) first.")
            return
        os.makedirs(OUT_DIR, exist_ok=True)
        z_up = app.preferences.generalPreferences.defaultModelingOrientation == \
            adsk.core.DefaultModelingOrientations.ZUpModelingOrientation
        up = "z" if z_up else "y"
        tubes = find_tubes(design)
        cylinders = find_cylinders(design)
        write_outputs(tubes, cylinders, body_boxes(design), up, diagnostics(design))
        mesh_note = ""
        try:
            export_mesh(app, design)
            mesh_note = "\nMesh: shark.obj"
        except Exception:
            mesh_note = "\n(Mesh export skipped: %s)" % traceback.format_exc().splitlines()[-1]
        ui.messageBox("Exported %d tubes and %d cylindrical faces to\n%s%s"
                      % (len([t for t in tubes if "line" in t]), len(cylinders), OUT_DIR, mesh_note))
    except Exception:
        ui.messageBox("Nibbles geometry export failed:\n%s" % traceback.format_exc())

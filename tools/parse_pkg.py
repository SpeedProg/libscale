"""Helpers for the artifact packaging CI steps.

Reads the ``conan create --format=json`` dependency graph and reports:
  * ``pkg``       - the package folder of the libscale node
  * ``gens``      - the generators folder of libscale's own build
  * ``licenses``  - stages third-party license texts into a distribution
                    and writes THIRD-PARTY-NOTICES.txt

Invoked from .github/workflows/ci.yml as
``python3 tools/parse_pkg.py <mode> <graph.json> [<staging-dir>]``.

License-staging policy (``licenses`` mode): every host-context component with
a license text gets its text staged into ``share/licenses/<name>/`` and is
listed in THIRD-PARTY-NOTICES.txt. ConanCenter ``/system`` packages are listed
as OS-provided. A component whose text cannot be located fails the job with a
``::error`` annotation; to ship such an artifact deliberately anyway, list its
name in the ALLOW_MISSING_LICENSE_TEXT environment variable (comma-separated).
"""

import json
import os
import shutil
import subprocess
import sys

USAGE = "usage: parse_pkg.py <pkg|gens|licenses> <graph.json> [<staging-dir>]"
MODES = ("pkg", "gens", "licenses")
ALLOW_MISSING_LICENSE_TEXT = "ALLOW_MISSING_LICENSE_TEXT"


def clean(text):
    """Keep notices lines and workflow annotations single-line and printable."""
    return " ".join(str(text).split())


def load_graph(path):
    try:
        with open(path, encoding="utf-8") as fh:
            data = json.load(fh)
    except OSError as exc:
        sys.exit("cannot read graph json %s: %s" % (path, exc))
    except json.JSONDecodeError as exc:
        sys.exit("graph json %s is not valid json: %s" % (path, exc))
    try:
        nodes = list(data["graph"]["nodes"].values())
    except (AttributeError, KeyError, TypeError):
        sys.exit("graph json %s does not look like a conan graph" % path)
    libscale = next((n for n in nodes if n.get("ref", "").startswith("libscale/")), None)
    if libscale is None:
        sys.exit("libscale node not found in graph json")
    return nodes, libscale


def resolve_folder(node):
    """Return ``(folder, reason)`` for a graph node.

    Trait-skipped (header-only) dependencies report no package folder in the
    graph even when they are cached; resolve theirs through ``conan cache
    path`` so their license texts can still be collected. ``reason`` describes
    why a folder could not be resolved (None on success).
    """
    folder = node.get("package_folder")
    if folder:
        return folder, None
    ref = node.get("ref", "")
    package_id = node.get("package_id")
    if package_id and "#" in ref:
        key = "%s:%s" % (ref, package_id)
        try:
            out = subprocess.run(
                ["conan", "cache", "path", key],
                capture_output=True, text=True, check=True, timeout=120)
            lines = out.stdout.strip().splitlines()
            if lines:
                return lines[-1], None
            return None, "conan cache path %s printed nothing" % key
        except subprocess.TimeoutExpired:
            return None, "conan cache path %s timed out after 120s" % key
        except FileNotFoundError:
            return None, "conan not found on PATH"
        except subprocess.CalledProcessError as exc:
            return None, "conan cache path %s failed: %s" % (key, clean(exc.stderr))
    return None, "no package folder in graph and no package id to resolve"


def is_system_package(node):
    """True for ConanCenter's /system packages, which come from the OS."""
    return (node.get("ref") or "").split("#")[0].endswith("/system")


def entry_of(node):
    """The THIRD-PARTY-NOTICES.txt tuple for a graph node."""
    lic = node.get("license") or "see project"
    if isinstance(lic, list):
        lic = " OR ".join(lic)
    return (clean(node.get("name", "?")), clean(node.get("version", "?")),
            clean(lic), clean(node.get("homepage") or ""))


def stage_licenses(nodes, staging):
    """Copy dependency license texts into the staged distribution tree.

    Components are sorted into buckets so the notices file never makes a claim
    the archive cannot back up:

    * staged       - text shipped in ``share/licenses/<name>/``
    * system       - ConanCenter ``/system`` package, provided by the consuming
                     machine's OS or package manager (informational section)
    * allowlisted  - named in ALLOW_MISSING_LICENSE_TEXT: deliberately shipped
                     without text (listed, never silently dropped)
    * missing      - text expected but not found: a packaging defect that fails
                     the job loudly instead of shipping an artifact with
                     silently missing texts
    """
    lic_dir = os.path.join(staging, "share", "licenses")
    os.makedirs(lic_dir, exist_ok=True)
    allowed = {name.strip() for name in os.environ.get(ALLOW_MISSING_LICENSE_TEXT, "").split(",")
               if name.strip()}
    staged = []
    system = []
    allowlisted = []
    missing = []
    for node in nodes:
        ref = node.get("ref") or ""
        if node.get("context", "host") != "host":
            continue  # build-context tools (tool_requires) never ship
        if node.get("test") or not ref or ref == "conanfile":
            continue  # test requirements (gtest) never ship in the library
        name = node.get("name", "?")
        entry = entry_of(node) + (None,)
        if name == "libscale":
            # libscale's own LICENSE is already staged from the package's share/
            # (CMake install rule); make sure that claim still holds.
            own = os.path.join(lic_dir, "libscale")
            if not os.path.isdir(own) or not os.listdir(own):
                sys.exit("share/licenses/libscale is missing or empty - the CMake install "
                         "rule for LICENSE no longer stages it")
            staged.append(entry)
            continue
        folder, reason = resolve_folder(node)
        src = os.path.join(folder, "licenses") if folder else None
        if src and os.path.isdir(src) and os.listdir(src):
            shutil.copytree(src, os.path.join(lic_dir, name), dirs_exist_ok=True)
            staged.append(entry)
        elif name in allowed:
            allowlisted.append(entry[:4] + (reason,))
        elif is_system_package(node):
            system.append(entry[:4] + (reason,))
        else:
            missing.append(entry[:4] + (reason,))
    staged.sort()
    system.sort()
    allowlisted.sort()
    missing.sort()
    with open(os.path.join(lic_dir, "THIRD-PARTY-NOTICES.txt"), "w", encoding="utf-8") as fh:
        fh.write("libscale and third-party components\n")
        fh.write("==================================\n\n")
        fh.write("This distribution uses or bundles code from the components\n")
        fh.write("listed below; their license texts are in the matching\n")
        fh.write("subdirectories of this folder.\n\n")
        for name, version, lic, home, _ in staged:
            fh.write("%s %s\n    License: %s\n    Project: %s\n\n" % (name, version, lic, home))
        if allowlisted:
            fh.write("The following components ship without their license text; this was\n")
            fh.write("allowed deliberately via the %s environment variable.\n\n"
                     % ALLOW_MISSING_LICENSE_TEXT)
            for name, version, lic, home, reason in allowlisted:
                fh.write("%s %s\n    License: %s\n    Project: %s\n    Text: not included "
                         "(%s)\n\n" % (name, version, lic, home, reason or "not found"))
        if system:
            fh.write("The following components are referenced by this build but\n")
            fh.write("their license texts are not included here: they are provided\n")
            fh.write("by the operating system or package manager of the consuming\n")
            fh.write("machine - consult the provider for their terms.\n\n")
            for name, version, lic, home, _ in system:
                fh.write("%s %s\n    License: %s\n    Project: %s\n\n"
                         % (name, version, lic, home))
    print("license texts staged for: %s" % ", ".join(n for n, _, _, _, _ in staged))
    if system:
        print("provided by the operating system (no text included): %s"
              % ", ".join(n for n, _, _, _, _ in system))
    if allowlisted:
        print("allowed without license text via %s: %s"
              % (ALLOW_MISSING_LICENSE_TEXT, ", ".join(n for n, _, _, _, _ in allowlisted)))
    if missing:
        detail = "; ".join("%s %s (%s): %s" % (n, v, l, reason)
                           for n, v, l, _, reason in missing)
        print("::error title=Missing license text::%s - add the text to the recipe's "
              "package()/copy(), or allow this component by listing it in %s"
              % (detail, ALLOW_MISSING_LICENSE_TEXT))
        sys.exit("license texts missing for: %s"
                 % ", ".join(n for n, _, _, _, _ in missing))


def main():
    # Keep the job alive (with replacement chars) if a non-ASCII name ever
    # reaches stdout on a narrow-encoding runner.
    try:
        sys.stdout.reconfigure(errors="replace")
    except (AttributeError, OSError):
        pass
    if len(sys.argv) < 3 or sys.argv[1] not in MODES:
        sys.exit(USAGE)
    if sys.argv[1] == "licenses" and len(sys.argv) < 4:
        sys.exit(USAGE)
    mode, path = sys.argv[1], sys.argv[2]
    nodes, libscale = load_graph(path)
    if mode == "pkg":
        folder = libscale.get("package_folder")
        if not folder:
            sys.exit("no package folder for libscale in graph json")
        print(folder.replace("\\", "/"))
    elif mode == "gens":
        # CMakeDeps configs generated for the create's own build cover every
        # dependency the build actually used (None on a pure cache hit).
        print((libscale.get("generators_folder") or "").replace("\\", "/"))
    else:
        stage_licenses(nodes, sys.argv[3])


if __name__ == "__main__":
    main()
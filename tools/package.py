"""Builds the downloadable files for a release into build/:
   AirwindowsChain-<version>-windows.zip   drag-and-drop copy
   AirwindowsChain-<version>-setup.exe     installer (needs makensis)

   python tools\\package.py 0.2.0 [path\\to\\makensis.exe]
"""
import os, subprocess, sys, zipfile

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
version = sys.argv[1]
makensis = sys.argv[2] if len(sys.argv) > 2 else "makensis"
release = os.path.join(root, "build", "AirwindowsChain_artefacts", "Release")
vst3 = os.path.join(release, "VST3", "Airwindows Chain.vst3")
clap = os.path.join(release, "CLAP", "Airwindows Chain.clap")
for p in (vst3, clap):
    if not os.path.exists(p):
        sys.exit("build the plugin first: " + p)

install_txt = """Airwindows Chain %s

Copy "Airwindows Chain.vst3" (the whole folder) to:
    C:\\Program Files\\Common Files\\VST3

Copy "Airwindows Chain.clap" to:
    C:\\Program Files\\Common Files\\CLAP

Then rescan plugins in your DAW. It shows up as Airwindows Chain, under Kani.

Source and licence: https://github.com/clotheshoesandwoes/airwindows-chain
""" % version

zip_path = os.path.join(root, "build", "AirwindowsChain-%s-windows.zip" % version)
with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as z:
    for folder, _, files in os.walk(vst3):
        for f in files:
            full = os.path.join(folder, f)
            z.write(full, os.path.relpath(full, release + os.sep + "VST3"))
    z.write(clap, "Airwindows Chain.clap")
    z.writestr("Install.txt", install_txt)
    z.write(os.path.join(root, "LICENSE"), "LICENSE.txt")
print("wrote", zip_path, os.path.getsize(zip_path) // 1024, "KB")

r = subprocess.run([makensis, "/V2", "/DVERSION=" + version, os.path.join(root, "tools", "installer.nsi")])
if r.returncode != 0:
    sys.exit("makensis failed")
exe = os.path.join(root, "build", "AirwindowsChain-%s-setup.exe" % version)
print("wrote", exe, os.path.getsize(exe) // 1024, "KB")

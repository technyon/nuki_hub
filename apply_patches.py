from os.path import join, isfile

Import("env")

FRAMEWORK_DIR = env.PioPlatform().get_package_dir("framework-arduinoespressif32")
patchflag_path = join(FRAMEWORK_DIR, ".hosted-patching-done")

# patch file only if we didn't do it before
if not isfile(join(FRAMEWORK_DIR, ".hosted-patching-done")):
    original_file = join(FRAMEWORK_DIR, "cores", "esp32", "esp32-hal-hosted.c")
    patched_file = join("resources", "esp32-hal-hosted.c.patch")

    assert isfile(original_file) and isfile(patched_file)

    env.Execute("patch %s %s" % (original_file, patched_file))
    # env.Execute("touch " + patchflag_path)

    original_file = join(FRAMEWORK_DIR, "cores", "esp32", "esp32-hal-hosted.h")
    patched_file = join("resources", "esp32-hal-hosted.h.patch")

    assert isfile(original_file) and isfile(patched_file)

    env.Execute("patch %s %s" % (original_file, patched_file))

    def _touch(path):
        with open(path, "w") as fp:
            fp.write("")

    env.Execute(lambda *args, **kwargs: _touch(patchflag_path))